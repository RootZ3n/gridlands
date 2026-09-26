#include "Structure/GLStructureSubsystem.h"

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
#include "Noise/GLNoiseSubsystem.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Structure/GLStructurePart.h"
#include "Terrain/GLTerrainSubsystem.h"

namespace
{
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

double UGLStructureSubsystem::GroundAt(const FVector2D& At) const
{
	const UGLTerrainSubsystem* Terrain = GetWorld() ? GetWorld()->GetSubsystem<UGLTerrainSubsystem>() : nullptr;
	return Terrain ? Terrain->HeightAt(At) : 0.0;
}

bool UGLStructureSubsystem::SpawnStructure(FName Placement, FName DefId, FName Cell, const FVector& Origin, int32 YawQuarter, bool bDeferPresentation)
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
	const FRotator Yaw(0.0, 90.0 * YawQuarter, 0.0);
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
		Part.Piece.Location = Base + Yaw.RotateVector(FVector(PartDef.Location[0], PartDef.Location[1], PartDef.Location[2]) * 100.0);
		Part.Piece.YawQuarter = (YawQuarter + PartDef.YawQuarter) % 4;
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
		Actor->SetActorTransform(GLCollapseRules::Motion(Falling->Outcome, Clock - Falling->DecidedAt));
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
	auto DistanceSq = [this, &Where](const TPair<FName, FName>& Entry)
	{
		const FGLStructureRuntime* Structure = Structures.Find(Entry.Key);
		const FGLStructurePartRuntime* Part = Structure ? Structure->Parts.FindByPredicate([&Entry](const FGLStructurePartRuntime& P) { return P.Name == Entry.Value; }) : nullptr;
		return Part ? FVector::DistSquared2D(Part->Piece.Location, Where) : 0.0; // stale entries go first (and are dropped)
	};
	while (Pending.Num() > 0)
	{
		int32 Nearest = 0;
		double Best = DistanceSq(Pending[0]);
		for (int32 I = 1; I < Pending.Num(); ++I)
		{
			const double D = DistanceSq(Pending[I]);
			if (D < Best)
			{
				Best = D;
				Nearest = I;
			}
		}
		const bool bNear = Best <= NearCm * NearCm;
		if (!bNear && (BudgetSeconds < 0.0 || (BudgetSeconds > 0.0 && FPlatformTime::Seconds() - Start >= BudgetSeconds)))
		{
			break;
		}
		const TPair<FName, FName> Entry = Pending[Nearest];
		Pending.RemoveAtSwap(Nearest);
		FGLStructureRuntime* Structure = Structures.Find(Entry.Key);
		FGLStructurePartRuntime* Part = Structure ? Structure->Find(Entry.Value) : nullptr;
		if (Part && !Part->Actor.IsValid() && Present(*Structure, *Part))
		{
			++Made;
		}
	}
	return Made;
}

AGLStructurePart* UGLStructureSubsystem::SpawnPart(FGLStructureRuntime& Structure, FGLStructurePartRuntime& Part)
{
	AGLStructurePart* Actor = GetWorld()->SpawnActor<AGLStructurePart>();
	if (!Actor || !Actor->Setup(Part.Piece) || !Actor->GetSalvageable()->Setup(Part.Salvage))
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
		Actor->GetSalvageable()->Setup(Part.Salvage); // debris salvages as the part did (provisional)
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

void UGLStructureSubsystem::Collapse(FGLStructureRuntime& Structure, AActor* By, const FVector& From, bool bSilent)
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
		return;
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
		Entry.Outcome = Outcome;
		Entry.DecidedAt = Clock;
		Entry.Material = MaterialOf(Part->Piece);
		Entry.Instigator = By;
	}
	UE_LOG(LogGridlands, Log, TEXT("Structures: %s lost support: %d part(s) collapse (decided in %.3f ms)"), *Structure.Placement.ToString(), Plan.Outcomes.Num(), (FPlatformTime::Seconds() - Began) * 1000.0);
	if (!bSilent)
	{
		EmitStructureEvent(this, TEXT("Event.Structure.Collapsed"), Structure.Def, By, Plan.Outcomes.Num());
	}
}

void UGLStructureSubsystem::Advance(double Seconds)
{
	Clock += FMath::Max(0.0, Seconds);
	for (FGLActiveCollapse& Collapse : Active)
	{
		const double Since = Clock - Collapse.DecidedAt;
		if (AGLStructurePart* Actor = FindPart(Collapse.Placement, Collapse.Part))
		{
			Actor->SetActorTransform(GLCollapseRules::Motion(Collapse.Outcome, Since));
		}
		if (!Collapse.bImpacted && Since >= Collapse.Outcome.ImpactSeconds)
		{
			Land(Collapse);
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
	Collapse.bImpacted = true;
	FGLImpactRecord& Record = Impacts.AddDefaulted_GetRef();
	Record.Placement = Collapse.Placement;
	Record.Part = Collapse.Part;
	Record.Damage = Collapse.Outcome.Damage;
	// Whatever has health inside the authoritative impact volume is hit, once, by the normal health system.
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		UGLHealthComponent* Health = It->FindComponentByClass<UGLHealthComponent>();
		if (!Health || Health->IsDead())
		{
			continue;
		}
		const FVector Centre = It->GetActorLocation();
		double Radius = 40.0, Half = 90.0;
		if (const ACharacter* Character = Cast<ACharacter>(*It))
		{
			Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
			Half = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		const FVector Up(0.0, 0.0, FMath::Max(0.0, Half - Radius));
		if (Collapse.Outcome.Impact.Touches(Centre, Radius) || Collapse.Outcome.Impact.Touches(Centre + Up, Radius) || Collapse.Outcome.Impact.Touches(Centre - Up, Radius))
		{
			Record.Hit.Add(*It);
			if (Collapse.Outcome.Damage > 0.0)
			{
				Health->ApplyDamage(Collapse.Outcome.Damage, Collapse.Instigator.Get());
			}
		}
	}
	UE_LOG(LogGridlands, Log, TEXT("Structures: %s/%s hit (%.0f damage, %d hit; impact resolved in %.3f ms)"), *Collapse.Placement.ToString(), *Collapse.Part.ToString(), Record.Damage, Record.Hit.Num(), (FPlatformTime::Seconds() - Began) * 1000.0);
	UGLNoiseSubsystem::EmitAction(this, TEXT("Noise.Structure.Collapse"), Collapse.Outcome.Impact.Centre, Collapse.Instigator.Get(), Collapse.Material);
	FGLStructureRuntime* Structure = Structures.Find(Collapse.Placement);
	FGLStructurePartRuntime* Part = Structure ? Structure->Find(Collapse.Part) : nullptr;
	if (Part && Part->State == EGLStructurePartState::Debris)
	{
		MakeDebris(*Structure, *Part);
		if (AGLStructurePart* Actor = Part->Actor.Get())
		{
			Actor->OnPresentationImpact.Broadcast(Actor);
		}
	}
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
		const FName Placement = It.Key();
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
		if (Entry.Value.Cell != Cell)
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

void UGLStructureSubsystem::RestoreCell(FName Cell, const TArray<FGLSavedStructurePart>& Saved, TArray<FString>* OutProblems)
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
		if (Entry.State == EGLStructurePartState::Debris)
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

void UGLStructureSubsystem::CollectFootprints(const FBox2D& Area, double MarginCm, TArray<FBox2D>& Out) const
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
			const FBox Box = PartBox(Part).ExpandBy(FVector(MarginCm, MarginCm, 0.0));
			const FBox2D Footprint(FVector2D(Box.Min), FVector2D(Box.Max));
			if (Footprint.Intersect(Area))
			{
				Out.Add(Footprint);
			}
		}
	}
}

bool UGLStructureSubsystem::IsUnderStructure(const FVector2D& World, double MarginCm) const
{
	TArray<FBox2D> Footprints;
	CollectFootprints(FBox2D(World, World), MarginCm, Footprints);
	return Footprints.Num() > 0;
}

bool UGLStructureSubsystem::Overlaps(const FBox& Box) const
{
	const FBox Shrunk = Box.ExpandBy(-GLStructureRules::OverlapShrinkCm);
	for (const TPair<FName, FGLStructureRuntime>& Entry : Structures)
	{
		for (const FGLStructurePartRuntime& Part : Entry.Value.Parts)
		{
			if (IsPresent(Part.State) && PartBox(Part).Intersect(Shrunk))
			{
				return true;
			}
		}
	}
	return false;
}
