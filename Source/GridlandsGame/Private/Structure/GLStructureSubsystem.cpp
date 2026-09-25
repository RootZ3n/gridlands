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

bool UGLStructureSubsystem::SpawnStructure(FName Placement, FName DefId, FName Cell, const FVector& Origin, int32 YawQuarter)
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
		SpawnPart(Structure, Part);
	}
	return true;
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
	UE_LOG(LogGridlands, Log, TEXT("Structures: %s lost support: %d part(s) collapse"), *Structure.Placement.ToString(), Plan.Outcomes.Num());
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
	UE_LOG(LogGridlands, Log, TEXT("Structures: %s/%s hit (%.0f damage, %d hit)"), *Collapse.Placement.ToString(), *Collapse.Part.ToString(), Record.Damage, Record.Hit.Num());
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
				Actor->Destroy();
			}
		}
		const FName Placement = It.Key();
		Active.RemoveAll([Placement](const FGLActiveCollapse& Collapse) { return Collapse.Placement == Placement; });
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
			MakeDebris(*Structure, *Part);
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
				if (FGLStructurePartRuntime* Part = Structure.Find(Active[I].Part))
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

bool UGLStructureSubsystem::IsUnderStructure(const FVector2D& World, double MarginCm) const
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
			if (World.X >= Box.Min.X && World.X <= Box.Max.X && World.Y >= Box.Min.Y && World.Y <= Box.Max.Y)
			{
				return true;
			}
		}
	}
	return false;
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
