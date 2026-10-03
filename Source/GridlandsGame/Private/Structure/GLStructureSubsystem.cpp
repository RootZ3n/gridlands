#include "Structure/GLStructureSubsystem.h"

#include "Building/GLPendingCollapse.h"
#include "Character/GLCharacter.h"
#include "Combat/GLCreature.h"
#include "Combat/GLCreatureRules.h"
#include "Combat/GLHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Character.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"
#include "Kismet/GameplayStatics.h"
#include "Noise/GLNoiseSubsystem.h"
#include "Pehlichi/GLPehlichi.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Structure/GLStructurePart.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "World/GLPlacementSubsystem.h"

namespace
{
	/** A part's footprint: intact from its piece (oriented at its yaw), debris from the box around its rest pose. */
	FGLFootprint PartFootprint(const FGLStructurePartRuntime& Part);

	/** A part's world bounds: intact from its piece, debris from its rest transform. */
	FBox PartBox(const FGLStructurePartRuntime& Part)
	{
		const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Part.Piece.Def);
		if (!Def)
		{
			return FBox(ForceInit);
		}
		if (Part.State == EGLStructurePartState::Intact)
		{
			return GLStructureRules::Bounds(*Def, Part.Piece);
		}
		const double HalfX = Def->Size.IsValidIndex(0) ? Def->Size[0] * 50.0 : 0.0;
		const double HalfY = Def->Size.IsValidIndex(1) ? Def->Size[1] * 50.0 : 0.0;
		const double Height = Def->Size.IsValidIndex(2) ? Def->Size[2] * 100.0 : 0.0;
		FBox Box(ForceInit);
		for (double X : { -HalfX, HalfX }) for (double Y : { -HalfY, HalfY }) for (double Z : { 0.0, Height })
		{
			Box += Part.Rest.TransformPosition(FVector(X, Y, Z));
		}
		return Box;
	}

	FGLFootprint PartFootprint(const FGLStructurePartRuntime& Part)
	{
		const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Part.Piece.Def);
		if (Def && Part.State == EGLStructurePartState::Intact)
		{
			return GLStructureRules::Footprint(*Def, Part.Piece);
		}
		return FGLFootprint::FromBox(PartBox(Part)); // debris lies in any pose: held by its box (conservative)
	}

	/** A storage piece's slots (0 when it stores nothing). */
	int32 StorageSlotsOf(FName Def)
	{
		const FGLBuildPieceDef* Piece = GLContent::Get().Find<FGLBuildPieceDef>(Def);
		return Piece ? Piece->Storage.Slots : 0;
	}

	int32 PieceIdFromPart(FName Part)
	{
		const FString Text = Part.ToString();
		return Text.StartsWith(TEXT("p")) ? FCString::Atoi(*Text.RightChop(1)) : 0;
	}

	bool IsPresent(EGLStructurePartState State)
	{
		return State == EGLStructurePartState::Intact || State == EGLStructurePartState::Debris;
	}

	void EmitStructureEvent(const UObject* Context, const TCHAR* Tag, FName Subject, AActor* Instigator, int32 Parts)
	{
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(Tag);
		Event.Subject = Subject;
		Event.Instigator = Instigator;
		Event.Numbers.Add(TEXT("parts"), Parts);
		UGLEventSubsystem::Emit(Context, MoveTemp(Event));
	}

	FName MaterialOf(const FGLPlacedPiece& Piece)
	{
		const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Piece.Def);
		return Def ? Def->Material : NAME_None;
	}
}

void UGLStructureSubsystem::Tick(float DeltaSeconds)
{
	Advance(DeltaSeconds);
}

FName UGLStructureSubsystem::IdentityOf(const AActor* Actor)
{
	if (Cast<AGLCharacter>(Actor))
	{
		return TEXT("Zenny");
	}
	if (Cast<AGLPehlichi>(Actor))
	{
		return TEXT("Pehlichi");
	}
	if (const AGLCreature* Creature = Cast<AGLCreature>(Actor))
	{
		return Creature->GetPlacementId();
	}
	return NAME_None;
}

AActor* UGLStructureSubsystem::ActorOf(FName Identity) const
{
	UWorld* World = GetWorld();
	if (!World || Identity.IsNone())
	{
		return nullptr;
	}
	if (Identity == TEXT("Zenny"))
	{
		for (TActorIterator<AGLCharacter> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
	if (Identity == TEXT("Pehlichi"))
	{
		for (TActorIterator<AGLPehlichi> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
	const UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>();
	return Placements ? Placements->FindCreature(Identity) : nullptr;
}

double UGLStructureSubsystem::GroundAt(const FVector2D& At) const
{
	const UGLTerrainSubsystem* Terrain = GetWorld() ? GetWorld()->GetSubsystem<UGLTerrainSubsystem>() : nullptr;
	return Terrain ? Terrain->HeightAt(At) : 0.0;
}

bool UGLStructureSubsystem::SpawnStructure(FName Placement, FName DefId, FName Cell, const FVector& Origin, int32 YawStep, bool bDeferPresentation)
{
	const FGLContentRegistry& Content = GLContent::Get();
	const FGLStructureDef* Def = Content.Find<FGLStructureDef>(DefId);
	if (!Def || Structures.Contains(Placement))
	{
		UE_LOG(LogGridlands, Warning, TEXT("Structures: cannot spawn %s (%s)"), *Placement.ToString(), Def ? TEXT("already spawned") : TEXT("unknown structure"));
		return false;
	}
	FGLStructureRuntime& Structure = Structures.Add(Placement);
	Structure.Placement = Placement;
	Structure.Def = DefId;
	Structure.Cell = Cell;
	const FVector Base(Origin.X, Origin.Y, GroundAt(FVector2D(Origin)));
	for (const FGLStructurePartDef& PartDef : Def->Parts)
	{
		const FGLBuildPieceDef* Piece = Content.Find<FGLBuildPieceDef>(PartDef.Piece);
		if (!Piece || PartDef.Location.Num() != 3)
		{
			UE_LOG(LogGridlands, Error, TEXT("Structures: %s part %s has no piece"), *DefId.ToString(), *PartDef.Name.ToString());
			continue;
		}
		FGLStructurePartRuntime& Part = Structure.Parts.AddDefaulted_GetRef();
		Part.Name = PartDef.Name;
		Part.Salvage = PartDef.Salvage;
		Part.Piece.Id = NextPieceId++;
		Part.Piece.Def = PartDef.Piece;
		const FVector2D Local = GLStructureRules::RotateXY(FVector2D(PartDef.Location[0], PartDef.Location[1]) * 100.0, YawStep);
		Part.Piece.Location = Base + FVector(Local, PartDef.Location[2] * 100.0);
		Part.Piece.YawStep = GLStructureRules::NormalizeYawStep(YawStep + GLStructureRules::YawStepFromDegrees(PartDef.Yaw));
		Part.Piece.Cell = Cell;
		const FGLCollapseDef& Collapse = PartDef.Collapse.Motion.IsNone() ? Piece->Collapse : PartDef.Collapse;
		Part.Motion = GLCollapseRules::MotionFromData(Collapse.Motion);
		Part.Direction = GLCollapseRules::DirectionFromData(Collapse.Direction);
		const FGLMaterialDef* Material = Content.Find<FGLMaterialDef>(Piece->Material);
		Part.DamageScale = Material ? Material->ImpactScale : 1.0;
	}
	for (FGLStructurePartRuntime& Part : Structure.Parts)
	{
		if (bDeferPresentation)
		{
			Pending.Add({ Placement, Part.Name });
		}
		else
		{
			SpawnPart(Structure, Part);
		}
	}
	return true;
}

bool UGLStructureSubsystem::IsPending(FName Placement, FName Part) const
{
	return Pending.Contains(TPair<FName, FName>(Placement, Part));
}

bool UGLStructureSubsystem::PresentPart(FName Placement, FName PartName)
{
	if (Pending.Remove(TPair<FName, FName>(Placement, PartName)) == 0)
	{
		return false;
	}
	FGLStructureRuntime* Structure = Structures.Find(Placement);
	FGLStructurePartRuntime* Part = Structure ? Structure->Find(PartName) : nullptr;
	if (Part && !Part->Actor.IsValid())
	{
		Present(*Structure, *Part);
	}
	return true;
}

bool UGLStructureSubsystem::IsCellPresented(FName Cell) const
{
	for (const TPair<FName, FName>& Entry : Pending)
	{
		const FGLStructureRuntime* Structure = Structures.Find(Entry.Key);
		if (Structure && Structure->Cell == Cell)
		{
			return false;
		}
	}
	return true;
}

AGLStructurePart* UGLStructureSubsystem::Present(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part)
{
	if (Part.Actor.IsValid() || !IsPresent(Part.State))
	{
		return Part.Actor.Get(); // already made (a landing made it), or gone for good: never made
	}
	const FName Placement = Structure.Placement, Name = Part.Name;
	const FGLActiveCollapse* Falling = Active.FindByPredicate([Placement, Name](const FGLActiveCollapse& C) { return C.Placement == Placement && C.Part == Name; });
	if (Part.State == EGLStructurePartState::Debris && !Falling)
	{
		MakeDebris(Structure, Part); // at its authoritative rest, solid, salvageable as debris
		return Part.Actor.Get();
	}
	AGLStructurePart* Actor = SpawnPart(Structure, Part);
	if (Actor && Falling)
	{
		// Mid-fall (decided while it was waiting): the plan's pose, not solid, until it lands.
		Actor->SetSolid(false);
		Actor->SetActorTransform(GLCollapseRules::Motion(Falling->Outcome, Falling->Elapsed));
	}
	return Actor;
}

int32 UGLStructureSubsystem::PumpPresentation(const FVector& Where, double BudgetSeconds, double NearCm)
{
	const double Start = FPlatformTime::Seconds();
	int32 Made = 0;
	// Retired actors of unloaded cells go first, within the same budget (never in a near-only frame).
	while (Retiring.Num() > 0 && BudgetSeconds >= 0.0 && (BudgetSeconds == 0.0 || FPlatformTime::Seconds() - Start < BudgetSeconds))
	{
		if (AGLStructurePart* Actor = Retiring.Pop(EAllowShrinking::No).Get())
		{
			Actor->Destroy();
		}
	}
	// P11: one pass to find every waiting part (parts indexed by name once per structure), then nearest first. The
	// pre-P11 pump rescanned every waiting entry per part and found each part by a linear search: harmless for authored
	// structures (a dozen parts), quadratic for a 300-piece player structure (9 ms for one unit).
	TMap<FName, TMap<FName, FGLStructurePartRuntime*>> Index;
	TArray<TPair<double, int32>> Order;
	Order.Reserve(Pending.Num());
	for (int32 I = 0; I < Pending.Num(); ++I)
	{
		FGLStructurePartRuntime* Part = nullptr;
		if (FGLStructureRuntime* Structure = Structures.Find(Pending[I].Key))
		{
			TMap<FName, FGLStructurePartRuntime*>* Parts = Index.Find(Pending[I].Key);
			if (!Parts)
			{
				Parts = &Index.Add(Pending[I].Key);
				for (FGLStructurePartRuntime& P : Structure->Parts)
				{
					Parts->Add(P.Name, &P);
				}
			}
			Part = Parts->FindRef(Pending[I].Value);
		}
		Order.Add({ Part ? FVector::DistSquared2D(Part->Piece.Location, Where) : 0.0, I }); // stale entries go first (and are dropped)
	}
	Order.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key != B.Key ? A.Key < B.Key : A.Value < B.Value; });
	TArray<bool> Done;
	Done.Init(false, Pending.Num());
	for (const TPair<double, int32>& Next : Order)
	{
		const bool bNear = Next.Key <= NearCm * NearCm;
		if (!bNear && (BudgetSeconds < 0.0 || (BudgetSeconds > 0.0 && FPlatformTime::Seconds() - Start >= BudgetSeconds)))
		{
			break;
		}
		Done[Next.Value] = true;
		const TPair<FName, FName>& Entry = Pending[Next.Value];
		FGLStructureRuntime* Structure = Structures.Find(Entry.Key);
		FGLStructurePartRuntime* Part = Structure ? Index.FindRef(Entry.Key).FindRef(Entry.Value) : nullptr;
		if (Part && !Part->Actor.IsValid() && Present(*Structure, *Part))
		{
			++Made;
		}
	}
	TArray<TPair<FName, FName>> Waiting;
	for (int32 I = 0; I < Pending.Num(); ++I)
	{
		if (!Done[I])
		{
			Waiting.Add(Pending[I]);
		}
	}
	Pending = MoveTemp(Waiting);
	return Made;
}

AGLStructurePart* UGLStructureSubsystem::SpawnPart(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part)
{
	AGLStructurePart* Actor = GetWorld()->SpawnActor<AGLStructurePart>();
	// P11: an intact player piece is taken apart by the building verbs (dismantle, smash), never by the salvage
	// interaction; its debris salvages like any debris (MakeDebris).
	const bool bSalvageable = !(Part.IsPlayer() && Part.State == EGLStructurePartState::Intact);
	if (!Actor || !Actor->Setup(Part.Piece) || (bSalvageable && !Actor->GetSalvageable()->Setup(Part.Salvage)))
	{
		UE_LOG(LogGridlands, Error, TEXT("Structures: could not spawn %s/%s"), *Structure.Placement.ToString(), *Part.Name.ToString());
		if (Actor)
		{
			Actor->Destroy();
		}
		return nullptr;
	}
	Actor->StructurePlacement = Structure.Placement;
	Actor->PartName = Part.Name;
	const FName Placement = Structure.Placement, Name = Part.Name;
	Actor->GetSalvageable()->OnSalvaged.AddWeakLambda(this, [this, Placement, Name](AActor* By) { HandlePartSalvaged(Placement, Name, By); });
	Part.Actor = Actor;
	return Actor;
}

void UGLStructureSubsystem::MakeDebris(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part)
{
	AGLStructurePart* Actor = Part.Actor.Get();
	if (!Actor)
	{
		Actor = SpawnPart(Structure, Part);
	}
	if (Actor)
	{
		Actor->SetActorTransform(Part.Rest);
		Actor->SetSolid(true);
		Actor->SetActorHiddenInGame(false);
		// P11: debris salvages by the collapse path (predominantly scrap), and gives back a storage piece's contents with it.
		Actor->GetSalvageable()->Setup(Part.Salvage, nullptr, EGLSalvagePath::Collapse);
		TMap<FName, int32> Held;
		for (const FGLInventoryStack& Stack : Part.Contents.GetStacks())
		{
			Held.FindOrAdd(Stack.Item) += Stack.Count;
		}
		Actor->GetSalvageable()->SetExtraYield(Held);
		TArray<FName> Layers; // a finished piece's finish comes back from its debris too, by the same (collapse) path
		for (const FName& Layer : Part.Piece.Layers)
		{
			if (const FGLFinishDef* Finish = GLContent::Get().Find<FGLFinishDef>(Layer))
			{
				Layers.Add(Finish->Salvage);
			}
		}
		Actor->GetSalvageable()->SetLayerSalvage(Layers);
	}
}

void UGLStructureSubsystem::HandlePartSalvaged(FName Placement, FName PartName, AActor* By)
{
	FGLStructureRuntime* Structure = Structures.Find(Placement);
	FGLStructurePartRuntime* Part = Structure ? Structure->Find(PartName) : nullptr;
	if (!Part)
	{
		return;
	}
	ToDestroy.Add(Part->Actor);
	if (Part->State == EGLStructurePartState::Debris)
	{
		Part->State = EGLStructurePartState::DebrisSalvaged;
		Part->Contents = FGLInventory(0); // delivered with the debris yield, all or nothing (the salvage refused otherwise)
		if (Structure->bPlayer)
		{
			// A player piece that is gone is gone: nothing to keep (an authored part keeps its state against its data).
			const int32 Id = Part->Piece.Id;
			Structure->Parts.RemoveAll([Id](const FGLStructurePartRuntime& P) { return P.Piece.Id == Id; });
			PlayerPieceCells.Remove(Id);
		}
		return;
	}
	if (Part->State != EGLStructurePartState::Intact)
	{
		return;
	}
	Part->State = EGLStructurePartState::Removed;
	const FVector Where = PartBox(*Part).GetCenter();
	UGLNoiseSubsystem::EmitAction(this, TEXT("Noise.Structure.Break"), Where, By, MaterialOf(Part->Piece));
	EmitStructureEvent(this, TEXT("Event.Structure.PartSalvaged"), Structure->Def, By, 1);
	Collapse(*Structure, By, Where);
}

TArray<int32> UGLStructureSubsystem::Collapse(FGLStructureRuntime& Structure, AActor* By, const FVector& From, bool bSilent)
{
	const double Began = FPlatformTime::Seconds();
	const FGLContentRegistry& Content = GLContent::Get();
	auto Ground = [this](const FVector2D& At) { return GroundAt(At); };
	TArray<FGLPlacedPiece> Remaining;
	for (const FGLStructurePartRuntime& Part : Structure.Parts)
	{
		if (Part.State == EGLStructurePartState::Intact)
		{
			Remaining.Add(Part.Piece);
		}
	}
	const TArray<int32> Falling = GLCollapseRules::Unsupported(Content, Remaining, Ground);
	if (Falling.Num() == 0)
	{
		return Falling;
	}
	TArray<FGLCollapseRequest> Requests;
	TArray<FGLPlacedPiece> Standing;
	for (const FGLStructurePartRuntime& Part : Structure.Parts)
	{
		if (Part.State != EGLStructurePartState::Intact)
		{
			continue;
		}
		if (Falling.Contains(Part.Piece.Id))
		{
			Requests.Add({ Part.Piece, Part.Motion, Part.Direction, Part.DamageScale });
		}
		else
		{
			Standing.Add(Part.Piece);
		}
	}
	const FGLCollapsePlan Plan = GLCollapseRules::Plan(Content, Requests, Standing, Ground, By ? By->GetActorLocation() : From, GLContent::Tuning().Collapse);
	for (const FGLCollapseOutcome& Outcome : Plan.Outcomes)
	{
		FGLStructurePartRuntime* Part = Structure.Parts.FindByPredicate([&Outcome](const FGLStructurePartRuntime& P) { return P.Piece.Id == Outcome.PieceId; });
		if (!Part)
		{
			continue;
		}
		// The outcome is final now: a save, an unload or a restart from here keeps the debris where it rests.
		Part->State = EGLStructurePartState::Debris;
		Part->Rest = Outcome.Rest;
		if (AGLStructurePart* Actor = Part->Actor.Get())
		{
			Actor->SetSolid(false);
		}
		FGLActiveCollapse& Entry = Active.AddDefaulted_GetRef();
		Entry.Placement = Structure.Placement;
		Entry.Part = Part->Name;
		Entry.Cell = Structure.Cell;
		Entry.Outcome = Outcome;
		Entry.Material = MaterialOf(Part->Piece);
		// P10: what physically caused it and who is credited are separate facts. Today the credit is the one who
		// removed the support (P6's rule); attribution through Pehlichi is a future operator decision (ADR-0038).
		Entry.Cause = IdentityOf(By);
		Entry.Credit = Entry.Cause;
		Entry.CreditActor = By;
	}
	UE_LOG(LogGridlands, Log, TEXT("Structures: %s lost support: %d part(s) collapse (decided in %.3f ms)"), *Structure.Placement.ToString(), Plan.Outcomes.Num(), (FPlatformTime::Seconds() - Began) * 1000.0);
	if (!bSilent)
	{
		EmitStructureEvent(this, TEXT("Event.Structure.Collapsed"), Structure.Def, By, Plan.Outcomes.Num());
	}
	return Falling;
}

void UGLStructureSubsystem::Advance(double Seconds)
{
	const double Dt = FMath::Max(0.0, Seconds);
	Clock += Dt;
	const UGLPlacementSubsystem* Placements = GetWorld() ? GetWorld()->GetSubsystem<UGLPlacementSubsystem>() : nullptr;
	for (int32 I = 0; I < Active.Num(); ++I) // by index: an impact can present a creature, never add a collapse
	{
		FGLActiveCollapse& Collapse = Active[I];
		// P10: a creature waiting for presentation is frozen (ADR-0037), so the fall waits with it: the race between
		// the structure and whoever might escape it is run on one clock, or not at all.
		if (Placements && Placements->HasPendingCreatures(Collapse.Cell))
		{
			continue;
		}
		Collapse.Elapsed += Dt;
		if (AGLStructurePart* Actor = FindPart(Collapse.Placement, Collapse.Part))
		{
			Actor->SetActorTransform(GLCollapseRules::Motion(Collapse.Outcome, Collapse.Elapsed));
		}
		if (!Collapse.bImpacted && Collapse.Elapsed >= Collapse.Outcome.ImpactSeconds)
		{
			Land(Active[I]);
		}
	}
	Active.RemoveAll([](const FGLActiveCollapse& Collapse) { return Collapse.bImpacted; });
	for (const TWeakObjectPtr<AActor>& Actor : ToDestroy)
	{
		if (AActor* Live = Actor.Get())
		{
			Live->Destroy();
		}
	}
	ToDestroy.Reset();
}

void UGLStructureSubsystem::Land(FGLActiveCollapse& Collapse)
{
	const double Began = FPlatformTime::Seconds();
	Collapse.bImpacted = true; // at once: from here a save holds this part as debris at rest, never as still to hit
	FGLImpactRecord Record;
	Record.Placement = Collapse.Placement;
	Record.Part = Collapse.Part;
	Record.Damage = Collapse.Outcome.Damage;
	Record.Severity = Collapse.Outcome.Severity;
	// Copies: presenting a creature below can grow the actor arrays, never this collapse's facts.
	const FGLCollapseOutcome Outcome = Collapse.Outcome;
	const FName Placement = Collapse.Placement, PartName = Collapse.Part, Material = Collapse.Material, Cause = Collapse.Cause;
	AActor* Credit = Collapse.CreditActor.IsValid() ? Collapse.CreditActor.Get() : ActorOf(Collapse.Credit);
	// P10 (ADR-0038): the impact decides from the world as it is NOW. A creature model (presented or not) touching the
	// volume gets ONE outcome: pinned (Neutralize.Pinned: no damage, no death, no kill) when the impact is severe
	// enough and it is susceptible, else the ordinary damage path, which may defeat it through its health.
	UGLPlacementSubsystem* Placements = GetWorld()->GetSubsystem<UGLPlacementSubsystem>();
	const bool bPins = GLCollapseRules::Pins(Outcome, GLContent::Tuning().Collapse);
	if (Placements)
	{
		for (const FName& Creature : Placements->ActiveCreaturesTouching(Outcome.Impact))
		{
			FVector Feet;
			Placements->CreatureLocation(Creature, Feet);
			const FGLActorPlacement* Model = Placements->FindActorModel(Creature);
			const double Yaw = Model ? Model->Creature.Yaw : 0.0;
			if (bPins && Placements->TryNeutralize(Creature, TEXT("Neutralize.Pinned"), Placement, Feet, Yaw))
			{
				Record.Pinned.Add(Creature);
			}
			else if (Outcome.Damage > 0.0 && Placements->DamageCreature(Creature, Outcome.Damage, Credit))
			{
				Record.Damaged.Add(Creature);
			}
			Record.Hit.Add(Placements->FindCreature(Creature));
		}
	}
	// Every other pawn with health (Zenny; a dev proof creature, which has no model): the normal health system.
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const AGLCreature* AsCreature = Cast<AGLCreature>(*It);
		if (AsCreature && Placements && Placements->FindActorModel(AsCreature->GetPlacementId()))
		{
			continue; // decided on its model above
		}
		UGLHealthComponent* Health = It->FindComponentByClass<UGLHealthComponent>();
		if (!Health || Health->IsDead() || (AsCreature && !AsCreature->IsActiveHostile()))
		{
			continue;
		}
		double Radius = 40.0, Half = 90.0;
		if (const ACharacter* Character = Cast<ACharacter>(*It))
		{
			Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
			Half = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		if (Outcome.Impact.TouchesCapsule(It->GetActorLocation(), Radius, Half))
		{
			Record.Hit.Add(*It);
			if (Outcome.Damage > 0.0)
			{
				Health->ApplyDamage(Outcome.Damage, Credit);
			}
		}
	}
	UE_LOG(LogGridlands, Log, TEXT("Structures: %s/%s hit (%.0f damage, severity %.2f, %d hit, %d pinned, %d damaged creatures; impact resolved in %.3f ms)"), *Placement.ToString(), *PartName.ToString(),
		Record.Damage, Record.Severity, Record.Hit.Num(), Record.Pinned.Num(), Record.Damaged.Num(), (FPlatformTime::Seconds() - Began) * 1000.0);
	UGLNoiseSubsystem::EmitAction(this, TEXT("Noise.Structure.Collapse"), Outcome.Impact.Centre, Cause == Collapse.Credit && Credit ? Credit : ActorOf(Cause), Material);
	FGLGameplayEvent Impact;
	Impact.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Structure.Impact"));
	Impact.Subject = Placement;
	Impact.Instigator = Credit;
	Impact.Numbers.Add(TEXT("pinned"), Record.Pinned.Num());
	Impact.Numbers.Add(TEXT("damaged"), Record.Damaged.Num());
	UGLEventSubsystem::Emit(this, MoveTemp(Impact));
	FGLStructureRuntime* Structure = Structures.Find(Placement);
	FGLStructurePartRuntime* Part = Structure ? Structure->Find(PartName) : nullptr;
	if (Part && Part->State == EGLStructurePartState::Debris)
	{
		MakeDebris(*Structure, *Part);
		if (AGLStructurePart* Actor = Part->Actor.Get())
		{
			Actor->OnPresentationImpact.Broadcast(Actor);
		}
	}
	Impacts.Add(MoveTemp(Record));
}

int32 UGLStructureSubsystem::RemoveCell(FName Cell)
{
	int32 Removed = 0;
	for (auto It = Structures.CreateIterator(); It; ++It)
	{
		if (It.Value().Cell != Cell)
		{
			continue;
		}
		for (FGLStructurePartRuntime& Part : It.Value().Parts)
		{
			if (AGLStructurePart* Actor = Part.Actor.Get())
			{
				// Gameplay leaves now (ADR-0033): unbound from the salvage pipeline (a later load of the
				// same placement can never be reached through it), hidden, no collision. Destroyed later.
				Actor->GetSalvageable()->OnSalvaged.RemoveAll(this);
				Actor->GetSalvageable()->RestoreSalvaged(); // refuses any further salvage; hidden, no collision
				Actor->bRetired = true;
				Retiring.Add(Actor);
				Part.Actor = nullptr;
			}
		}
		if (It.Value().bPlayer)
		{
			for (const FGLStructurePartRuntime& Part : It.Value().Parts)
			{
				PlayerPieceCells.Remove(Part.Piece.Id);
			}
		}
		const FName Placement = It.Key();
		// P10: the cell's record captured its collapses in flight (StowCell runs first); they wait there, frozen.
		Active.RemoveAll([Placement](const FGLActiveCollapse& Collapse) { return Collapse.Placement == Placement; });
		Pending.RemoveAll([Placement](const TPair<FName, FName>& Entry) { return Entry.Key == Placement; }); // cancelled presentation
		It.RemoveCurrent();
		++Removed;
	}
	return Removed;
}

void UGLStructureSubsystem::CaptureCell(FName Cell, TArray<FGLSavedStructurePart>& Out) const
{
	for (const TPair<FName, FGLStructureRuntime>& Entry : Structures)
	{
		if (Entry.Value.Cell != Cell || Entry.Value.bPlayer) // player pieces are saved whole (CapturePlayerCell)
		{
			continue;
		}
		for (const FGLStructurePartRuntime& Part : Entry.Value.Parts)
		{
			if (Part.State == EGLStructurePartState::Intact)
			{
				continue;
			}
			FGLSavedStructurePart& Saved = Out.AddDefaulted_GetRef();
			Saved.Placement = Entry.Key;
			Saved.Part = Part.Name;
			Saved.State = Part.State;
			if (Part.State == EGLStructurePartState::Debris)
			{
				Saved.Location = Part.Rest.GetLocation();
				Saved.Rotation = Part.Rest.Rotator();
			}
		}
	}
	Out.Sort([](const FGLSavedStructurePart& A, const FGLSavedStructurePart& B)
	{
		return A.Placement != B.Placement ? A.Placement.LexicalLess(B.Placement) : A.Part.LexicalLess(B.Part);
	});
}

void UGLStructureSubsystem::CaptureCollapses(FName Cell, TArray<FGLSavedCollapse>& Out) const
{
	for (const FGLActiveCollapse& Collapse : Active)
	{
		if (Collapse.Cell == Cell && !Collapse.bImpacted)
		{
			Out.Add(GLPendingCollapse::Capture(Collapse.Placement, Collapse.Part, Collapse.Outcome, Collapse.Elapsed, Collapse.Material, Collapse.Cause, Collapse.Credit));
		}
	}
	Out.Sort([](const FGLSavedCollapse& A, const FGLSavedCollapse& B)
	{
		return A.Placement != B.Placement ? A.Placement.LexicalLess(B.Placement) : A.Part.LexicalLess(B.Part);
	});
}

void UGLStructureSubsystem::RestoreCell(FName Cell, const TArray<FGLSavedStructurePart>& Saved, const TArray<FGLSavedCollapse>& InFlight, TArray<FString>* OutProblems)
{
	auto Problem = [OutProblems](const FString& Message)
	{
		UE_LOG(LogGridlands, Warning, TEXT("Load: %s"), *Message);
		if (OutProblems)
		{
			OutProblems->Add(Message);
		}
	};
	TSet<FName> Touched;
	for (const FGLSavedStructurePart& Entry : Saved)
	{
		FGLStructureRuntime* Structure = Structures.Find(Entry.Placement);
		FGLStructurePartRuntime* Part = Structure && Structure->Cell == Cell ? Structure->Find(Entry.Part) : nullptr;
		if (!Part)
		{
			Problem(FString::Printf(TEXT("saved structure part %s/%s no longer exists"), *Entry.Placement.ToString(), *Entry.Part.ToString()));
			continue;
		}
		Touched.Add(Entry.Placement);
		Part->State = Entry.State;
		const FName Name = Part->Name, Placement = Entry.Placement;
		Active.RemoveAll([Name, Placement](const FGLActiveCollapse& Collapse) { return Collapse.Placement == Placement && Collapse.Part == Name; });
		const FGLSavedCollapse* Flight = InFlight.FindByPredicate([Name, Placement](const FGLSavedCollapse& C) { return C.Placement == Placement && C.Part == Name; });
		FGLCollapseOutcome Outcome;
		FString Why;
		if (Flight && Entry.State == EGLStructurePartState::Debris && !GLPendingCollapse::Reconstruct(*Flight, Outcome, &Why))
		{
			Problem(FString::Printf(TEXT("saved collapse of %s/%s is not a plan (%s): it settles at rest"), *Placement.ToString(), *Name.ToString(), *Why));
			Flight = nullptr;
		}
		if (Flight && Entry.State == EGLStructurePartState::Debris)
		{
			// P10: still in flight. Silently resumed from where it was (no collapse event, no noise); its impact is to come.
			Outcome.PieceId = Part->Piece.Id;
			Outcome.Def = Part->Piece.Def;
			Part->Rest = Outcome.Rest;
			FGLActiveCollapse& Resumed = Active.AddDefaulted_GetRef();
			Resumed.Placement = Placement;
			Resumed.Part = Name;
			Resumed.Cell = Structure->Cell;
			Resumed.Outcome = MoveTemp(Outcome);
			Resumed.Elapsed = Flight->ElapsedSeconds;
			Resumed.Material = Flight->Material;
			Resumed.Cause = Flight->Cause;
			Resumed.Credit = Flight->Credit;
			if (AGLStructurePart* Actor = Part->Actor.Get())
			{
				Actor->SetSolid(false);
				Actor->SetActorHiddenInGame(false);
				Actor->SetActorTransform(GLCollapseRules::Motion(Resumed.Outcome, Resumed.Elapsed));
			}
			// A part still waiting for presentation is made later at the plan's pose (Present).
		}
		else if (Entry.State == EGLStructurePartState::Debris)
		{
			Part->Rest = FTransform(Entry.Rotation, Entry.Location);
			if (!IsPending(Entry.Placement, Part->Name))
			{
				MakeDebris(*Structure, *Part);
			}
			// A part still waiting for presentation is made later, directly as this debris.
		}
		else if (AGLStructurePart* Actor = Part->Actor.Get())
		{
			Actor->Destroy(); // removed or salvaged debris: gone (silently)
			Part->Actor = nullptr;
		}
	}
	for (const FGLSavedCollapse& Flight : InFlight)
	{
		if (!Active.ContainsByPredicate([&Flight](const FGLActiveCollapse& C) { return C.Placement == Flight.Placement && C.Part == Flight.Part; }))
		{
			Problem(FString::Printf(TEXT("saved collapse of %s/%s has no fallen part to resume"), *Flight.Placement.ToString(), *Flight.Part.ToString()));
		}
	}
	// A saved world must be structurally consistent. If data changed so that an intact part is now
	// unsupported, it settles silently (no damage, no noise) rather than hanging in the air.
	for (const FName& Placement : Touched)
	{
		FGLStructureRuntime& Structure = Structures[Placement];
		TArray<FGLPlacedPiece> Remaining;
		for (const FGLStructurePartRuntime& Part : Structure.Parts)
		{
			if (Part.State == EGLStructurePartState::Intact)
			{
				Remaining.Add(Part.Piece);
			}
		}
		const TArray<int32> Loose = GLCollapseRules::Unsupported(GLContent::Get(), Remaining, [this](const FVector2D& At) { return GroundAt(At); });
		if (Loose.Num() > 0)
		{
			Problem(FString::Printf(TEXT("%s: %d intact part(s) had no support after loading; settled as debris"), *Placement.ToString(), Loose.Num()));
			const int32 Before = Active.Num();
			Collapse(Structure, nullptr, FVector::ZeroVector, true);
			for (int32 I = Active.Num() - 1; I >= Before; --I)
			{
				if (FGLStructurePartRuntime* Part = Structure.Find(Active[I].Part); Part && !IsPending(Placement, Part->Name))
				{
					MakeDebris(Structure, *Part);
				}
				Active.RemoveAt(I);
			}
		}
	}
}

AGLStructurePart* UGLStructureSubsystem::FindPart(FName Placement, FName Part) const
{
	const FGLStructureRuntime* Structure = Structures.Find(Placement);
	const FGLStructurePartRuntime* Found = Structure ? Structure->Parts.FindByPredicate([Part](const FGLStructurePartRuntime& P) { return P.Name == Part; }) : nullptr;
	return Found ? Found->Actor.Get() : nullptr;
}

void UGLStructureSubsystem::CollectFootprints(const FBox2D& Area, double MarginCm, TArray<FGLFootprint>& Out) const
{
	for (const TPair<FName, FGLStructureRuntime>& Entry : Structures)
	{
		for (const FGLStructurePartRuntime& Part : Entry.Value.Parts)
		{
			const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Part.Piece.Def);
			const bool bGroundedIntact = Part.State == EGLStructurePartState::Intact && Def && Def->Grounded;
			if (!bGroundedIntact && Part.State != EGLStructurePartState::Debris)
			{
				continue;
			}
			FGLFootprint Footprint = PartFootprint(Part);
			Footprint.Half += FVector2D(MarginCm, MarginCm);
			const FBox Enclosing = Footprint.Enclosing();
			if (FBox2D(FVector2D(Enclosing.Min), FVector2D(Enclosing.Max)).Intersect(Area))
			{
				Out.Add(Footprint);
			}
		}
	}
}

bool UGLStructureSubsystem::IsUnderStructure(const FVector2D& World, double MarginCm) const
{
	TArray<FGLFootprint> Footprints;
	CollectFootprints(FBox2D(World, World), MarginCm, Footprints);
	return Footprints.ContainsByPredicate([&World](const FGLFootprint& F) { return F.ContainsXY(World); });
}

bool UGLStructureSubsystem::Overlaps(const FGLFootprint& Footprint) const
{
	for (const TPair<FName, FGLStructureRuntime>& Entry : Structures)
	{
		for (const FGLStructurePartRuntime& Part : Entry.Value.Parts)
		{
			if (!IsPresent(Part.State) || (Part.IsPlayer() && Part.State == EGLStructurePartState::Intact))
			{
				continue; // intact player pieces are the structural rules' own overlap check
			}
			if (PartFootprint(Part).Overlaps(Footprint, GLStructureRules::OverlapShrinkCm))
			{
				return true;
			}
		}
	}
	return false;
}

// ---- P11 (ADR-0039): player-built structures in the canonical structural model ----

FName UGLStructureSubsystem::PlayerKey(FName Cell)
{
	return FName(*FString::Printf(TEXT("player:%s"), *Cell.ToString()));
}

FName UGLStructureSubsystem::PlayerPartName(int32 PieceId)
{
	return FName(*FString::Printf(TEXT("p%d"), PieceId));
}

bool UGLStructureSubsystem::AddPlayerPiece(const FGLPlacedPiece& InPiece, bool bDeferPresentation)
{
	const FGLContentRegistry& Content = GLContent::Get();
	const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(InPiece.Def);
	if (!Def || PlayerPieceCells.Contains(InPiece.Id)) // a piece outside any cell (a bare test world) keeps None, as in v0
	{
		return false;
	}
	const FName Key = PlayerKey(InPiece.Cell);
	FGLStructureRuntime& Structure = Structures.FindOrAdd(Key);
	Structure.Placement = Key;
	Structure.Def = TEXT("player");
	Structure.Cell = InPiece.Cell;
	Structure.bPlayer = true;
	FGLStructurePartRuntime& Part = Structure.Parts.AddDefaulted_GetRef();
	Part.Name = PlayerPartName(InPiece.Id);
	Part.Piece = InPiece;
	Part.Piece.Origin = EGLPieceOrigin::Player;
	Part.Salvage = Def->Salvage;
	Part.Motion = GLCollapseRules::MotionFromData(Def->Collapse.Motion);
	Part.Direction = GLCollapseRules::DirectionFromData(Def->Collapse.Direction);
	const FGLMaterialDef* Material = Content.Find<FGLMaterialDef>(Def->Material);
	Part.DamageScale = Material ? Material->ImpactScale : 1.0;
	Part.Contents = FGLInventory(StorageSlotsOf(InPiece.Def));
	PlayerPieceCells.Add(InPiece.Id, InPiece.Cell);
	if (bDeferPresentation)
	{
		Pending.Add({ Key, Part.Name });
	}
	else
	{
		SpawnPart(Structure, Part);
	}
	return true;
}

TArray<FGLPlacedPiece> UGLStructureSubsystem::PlayerPieces(FName Cell, bool bIncludeDebris) const
{
	TArray<FGLPlacedPiece> Out;
	for (const TPair<FName, FGLStructureRuntime>& Entry : Structures)
	{
		if (!Entry.Value.bPlayer || (!Cell.IsNone() && Entry.Value.Cell != Cell))
		{
			continue;
		}
		for (const FGLStructurePartRuntime& Part : Entry.Value.Parts)
		{
			if (Part.State == EGLStructurePartState::Intact || (bIncludeDebris && Part.State == EGLStructurePartState::Debris))
			{
				Out.Add(Part.Piece);
			}
		}
	}
	Out.Sort([](const FGLPlacedPiece& A, const FGLPlacedPiece& B) { return A.Id < B.Id; });
	return Out;
}

const FGLStructurePartRuntime* UGLStructureSubsystem::FindPlayerPiece(int32 PieceId) const
{
	return const_cast<UGLStructureSubsystem*>(this)->FindPlayerPieceMutable(PieceId);
}

FGLStructurePartRuntime* UGLStructureSubsystem::FindPlayerPieceMutable(int32 PieceId)
{
	const FName* Cell = PlayerPieceCells.Find(PieceId);
	FGLStructureRuntime* Structure = Cell ? Structures.Find(PlayerKey(*Cell)) : nullptr;
	return Structure ? Structure->Find(PlayerPartName(PieceId)) : nullptr;
}

TArray<FGLPlacedPiece> UGLStructureSubsystem::PlayerStructureOf(int32 PieceId) const
{
	const FName* Cell = PlayerPieceCells.Find(PieceId);
	return Cell ? PlayerPieces(*Cell) : TArray<FGLPlacedPiece>();
}

TArray<int32> UGLStructureSubsystem::RemovePlayerPiece(int32 PieceId, AActor* By)
{
	const FName* Cell = PlayerPieceCells.Find(PieceId);
	FGLStructureRuntime* Structure = Cell ? Structures.Find(PlayerKey(*Cell)) : nullptr;
	FGLStructurePartRuntime* Part = Structure ? Structure->Find(PlayerPartName(PieceId)) : nullptr;
	if (!Part || Part->State != EGLStructurePartState::Intact)
	{
		return {};
	}
	const FVector Where = PartBox(*Part).GetCenter();
	ToDestroy.Add(Part->Actor);
	Part->Actor = nullptr;
	Pending.Remove(TPair<FName, FName>(Structure->Placement, Part->Name));
	// Gone whole: a removed player piece leaves no fact behind.
	Structure->Parts.RemoveAll([PieceId](const FGLStructurePartRuntime& P) { return P.Piece.Id == PieceId; });
	PlayerPieceCells.Remove(PieceId);
	// The same canonical collapse as an authored structure losing a part (P6/P10): whatever lost support falls.
	return Collapse(*Structure, By, Where);
}

bool UGLStructureSubsystem::SetPlayerLayers(int32 PieceId, const TArray<FName>& Layers)
{
	FGLStructurePartRuntime* Part = FindPlayerPieceMutable(PieceId);
	if (!Part || Part->State != EGLStructurePartState::Intact)
	{
		return false;
	}
	Part->Piece.Layers = Layers;
	if (AGLStructurePart* Actor = Part->Actor.Get())
	{
		Actor->Setup(Part->Piece); // presentation follows the fact: the frame becomes a finished wall
	}
	return true;
}

TSet<FName> UGLStructureSubsystem::CellsWithPlayerPieces() const
{
	TSet<FName> Out;
	for (const TPair<FName, FGLStructureRuntime>& Entry : Structures)
	{
		if (Entry.Value.bPlayer && Entry.Value.Parts.Num() > 0)
		{
			Out.Add(Entry.Value.Cell);
		}
	}
	return Out;
}

void UGLStructureSubsystem::CapturePlayerCell(FName Cell, TArray<FGLSavedPiece>& Out) const
{
	const FGLStructureRuntime* Structure = Structures.Find(PlayerKey(Cell));
	if (!Structure)
	{
		return;
	}
	for (const FGLStructurePartRuntime& Part : Structure->Parts)
	{
		if (Part.State != EGLStructurePartState::Intact && Part.State != EGLStructurePartState::Debris)
		{
			continue;
		}
		FGLSavedPiece& Saved = Out.AddDefaulted_GetRef();
		Saved.Id = Part.Piece.Id;
		Saved.Def = Part.Piece.Def;
		Saved.Location = Part.Piece.Location;
		Saved.YawStep = Part.Piece.YawStep;
		Saved.Origin = static_cast<uint8>(Part.Piece.Origin);
		Saved.Layers = Part.Piece.Layers;
		TMap<FName, int32> Held;
		for (const FGLInventoryStack& Stack : Part.Contents.GetStacks())
		{
			Held.FindOrAdd(Stack.Item) += Stack.Count;
		}
		TArray<FName> Items;
		Held.GetKeys(Items);
		Items.Sort(FNameLexicalLess());
		for (const FName& Item : Items)
		{
			Saved.Contents.Add({ Item, Held[Item] });
		}
		Saved.State = Part.State;
		if (Part.State == EGLStructurePartState::Debris)
		{
			Saved.RestLocation = Part.Rest.GetLocation();
			Saved.RestRotation = Part.Rest.GetRotation();
		}
	}
	Out.Sort([](const FGLSavedPiece& A, const FGLSavedPiece& B) { return A.Id < B.Id; });
}

void UGLStructureSubsystem::RestorePlayerCell(FName Cell, const TArray<FGLSavedPiece>& Saved, const TArray<FGLSavedCollapse>& InFlight, TArray<FString>* OutProblems)
{
	auto Problem = [OutProblems](const FString& Message)
	{
		UE_LOG(LogGridlands, Warning, TEXT("Load: %s"), *Message);
		if (OutProblems)
		{
			OutProblems->Add(Message);
		}
	};
	const FGLContentRegistry& Content = GLContent::Get();
	// The saved record is the truth for this cell: whatever player construction is live there now gives way to it.
	const FName Key = PlayerKey(Cell);
	if (FGLStructureRuntime* Live = Structures.Find(Key))
	{
		for (FGLStructurePartRuntime& Part : Live->Parts)
		{
			PlayerPieceCells.Remove(Part.Piece.Id);
			if (AGLStructurePart* Actor = Part.Actor.Get())
			{
				Actor->Destroy();
			}
		}
		Active.RemoveAll([Key](const FGLActiveCollapse& C) { return C.Placement == Key; });
		Pending.RemoveAll([Key](const TPair<FName, FName>& Entry) { return Entry.Key == Key; });
		Structures.Remove(Key);
	}
	TArray<FGLSavedStructurePart> Debris;
	for (const FGLSavedPiece& Entry : Saved)
	{
		if (!Content.Find<FGLBuildPieceDef>(Entry.Def))
		{
			Problem(FString::Printf(TEXT("saved build piece %s no longer exists"), *Entry.Def.ToString()));
			continue;
		}
		if (PlayerPieceCells.Contains(Entry.Id))
		{
			continue; // never duplicate a piece
		}
		FGLPlacedPiece Piece;
		Piece.Id = Entry.Id;
		Piece.Def = Entry.Def;
		Piece.Location = Entry.Location;
		Piece.YawStep = GLStructureRules::NormalizeYawStep(Entry.YawStep);
		Piece.Cell = Cell;
		Piece.Origin = Entry.Origin == static_cast<uint8>(EGLPieceOrigin::Authored) ? EGLPieceOrigin::Authored : EGLPieceOrigin::Player;
		Piece.Layers = Entry.Layers;
		AddPlayerPiece(Piece, true); // the pump presents it within the frame budget (P8 synchronous-restore debt)
		FGLStructurePartRuntime* Part = FindPlayerPieceMutable(Entry.Id);
		for (const FGLSavedCount& Held : Entry.Contents)
		{
			if (!Content.Find<FGLItemDef>(Held.Id))
			{
				Problem(FString::Printf(TEXT("saved stored item %s no longer exists"), *Held.Id.ToString()));
				continue;
			}
			Part->Contents.ForceAdd(Content, Held.Id, Held.Count); // a save is never truncated
		}
		if (Entry.State == EGLStructurePartState::Debris)
		{
			FGLSavedStructurePart& D = Debris.AddDefaulted_GetRef();
			D.Placement = PlayerKey(Cell);
			D.Part = PlayerPartName(Entry.Id);
			D.State = EGLStructurePartState::Debris;
			D.Location = Entry.RestLocation;
			D.Rotation = Entry.RestRotation.Rotator();
		}
	}
	// Debris and collapses in flight go through the one restore path authored structures use (no replay, no damage).
	if (Debris.Num() > 0 || InFlight.Num() > 0 || Structures.Contains(PlayerKey(Cell)))
	{
		RestoreCell(Cell, Debris, InFlight, OutProblems);
	}
}

UGLStructureSubsystem::ERenewal UGLStructureSubsystem::Renew(FName Placement, TConstArrayView<FGLClaim> Claims)
{
	FGLStructureRuntime* Structure = Structures.Find(Placement);
	if (!Structure)
	{
		return ERenewal::Unknown;
	}
	// WORLD RENEWAL NEVER DELETES OR REPLACES PLAYER-OWNED CONSTRUCTION (ADR-0039): every part must be renewable.
	for (const FGLStructurePartRuntime& Part : Structure->Parts)
	{
		if (!GLClaimRules::MayRenew(Part.Piece.Origin, FVector2D(Part.Piece.Location), Claims))
		{
			return Part.Piece.Origin == EGLPieceOrigin::Player ? ERenewal::PlayerOwned : ERenewal::InsideClaim;
		}
	}
	if (Structure->bPlayer)
	{
		return ERenewal::PlayerOwned;
	}
	Active.RemoveAll([Placement](const FGLActiveCollapse& C) { return C.Placement == Placement; });
	for (FGLStructurePartRuntime& Part : Structure->Parts)
	{
		if (AGLStructurePart* Actor = Part.Actor.Get())
		{
			Actor->Destroy();
		}
		Part.Actor = nullptr;
		Part.State = EGLStructurePartState::Intact;
		Part.Rest = FTransform::Identity;
		SpawnPart(*Structure, Part);
	}
	UE_LOG(LogGridlands, Log, TEXT("Structures: %s renewed (all parts intact)"), *Placement.ToString());
	return ERenewal::Renewed;
}
