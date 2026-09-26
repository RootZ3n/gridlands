#include "World/GLPlacementSubsystem.h"

#include "Presentation/GLScatterPatch.h"
#include "Structure/GLStructureSubsystem.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "GridlandsGame.h"
#include "Puzzle/GLPuzzleSite.h"
#include "Combat/GLCreature.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "World/GLAnchorComponent.h"

namespace
{
	FString CellShortName(FName CellId)
	{
		FString Left, Right;
		return CellId.ToString().Split(TEXT("."), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ? Right : CellId.ToString();
	}

	AActor* FindAnchoredActor(UWorld* World, FName AnchorId)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const UGLAnchorComponent* Anchor = It->FindComponentByClass<UGLAnchorComponent>();
			if (Anchor && Anchor->AnchorId == AnchorId)
			{
				return *It;
			}
		}
		return nullptr;
	}
}

void UGLPlacementSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("GLDenseProof")))
	{
		AddDenseProof();
	}
#endif
	// The cell whose definition names this world's map.
	const FString MapPackage = InWorld.GetOutermost()->GetName();
	GLContent::Get().ForEachEntry([this, &MapPackage](const FGLContentEntry& Entry)
	{
		const FGLCellDef* Cell = Entry.Definition.GetPtr<FGLCellDef>();
		if (Cell && Entry.Kind == TEXT("cell") && !Cell->Level.IsEmpty() && MapPackage.EndsWith(FPaths::GetBaseFilename(Cell->Level)))
		{
			SpawnCell(Entry.Id);
		}
	});
}

int32 UGLPlacementSubsystem::SpawnCell(FName CellId, bool bDeferPresentation)
{
	const FGLContentRegistry& Content = GLContent::Get();
	if (SpawnedCells.Contains(CellId))
	{
		UE_LOG(LogGridlands, Warning, TEXT("Placements: %s is already spawned (not spawning twice)"), *CellId.ToString());
		return 0;
	}
	SpawnedCells.Add(CellId);
	CellActors.FindOrAdd(CellId);
	const FString Prefix = FString::Printf(TEXT("placement.%s."), *CellShortName(CellId));
	int32 Spawned = 0;
	TMap<FName, double> KindMs; // where the authoritative frame goes (P7 budget evidence)
	auto Spawn = [&](FName Id, const FGLPlacementDef& Placement)
	{
		const double KindStart = FPlatformTime::Seconds();
		Spawned += SpawnPlacement(CellId, Id, Placement, bDeferPresentation) ? 1 : 0;
		KindMs.FindOrAdd(Placement.Kind) += (FPlatformTime::Seconds() - KindStart) * 1000.0;
	};
	Content.ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLPlacementDef* Placement = Entry.Definition.GetPtr<FGLPlacementDef>();
		if (Placement && Entry.Kind == TEXT("placement") && Entry.Id.ToString().StartsWith(Prefix))
		{
			Spawn(Entry.Id, *Placement);
		}
	});
#if !UE_BUILD_SHIPPING
	if (const TArray<TPair<FName, FGLPlacementDef>>* Proof = ProofPlacements.Find(CellId))
	{
		for (const TPair<FName, FGLPlacementDef>& Entry : *Proof)
		{
			Spawn(Entry.Key, Entry.Value);
		}
	}
#endif
	FString Costs;
	for (const TPair<FName, double>& K : KindMs)
	{
		if (K.Value > 0.05)
		{
			Costs += FString::Printf(TEXT(" %s %.2f ms"), *K.Key.ToString(), K.Value);
		}
	}
	UE_LOG(LogGridlands, Log, TEXT("Placements: spawned %d for %s%s;%s"), Spawned, *CellId.ToString(), bDeferPresentation ? TEXT(" (presentation deferred)") : TEXT(""), *Costs);
	return Spawned;
}

bool UGLPlacementSubsystem::SpawnPlacement(FName CellId, FName Id, const FGLPlacementDef& Placement, bool bDeferPresentation)
{
	UWorld* World = GetWorld();
	const FGLContentRegistry& Content = GLContent::Get();
	if (Placement.Kind != TEXT("salvage_node") && Placement.Kind != TEXT("glitch") && Placement.Kind != TEXT("puzzle_site")
		&& Placement.Kind != TEXT("spawn") && Placement.Kind != TEXT("discovery") && Placement.Kind != TEXT("structure") && Placement.Kind != TEXT("scatter"))
	{
		return false;
	}
	// Placement coordinates are cell-local (P3, ADR-0026): offset by the cell's world centre.
	const FGLCellDef* Cell = Content.Find<FGLCellDef>(CellId);
	const FVector CellCentre = Cell ? FVector(Cell->CentreCm(), 0.0) : FVector::ZeroVector;
	TArray<TWeakObjectPtr<AActor>>& Owned = CellActors.FindOrAdd(CellId);
	FVector Location = FVector::ZeroVector;
	double Yaw = 0.0;
	AActor* Visual = nullptr;
	if (Placement.IsAnchored())
	{
		Visual = FindAnchoredActor(World, Placement.Anchor);
		const FGLAnchorRecord* Record = Content.FindAnchor(Placement.Anchor);
		if (!Visual && !Record)
		{
			UE_LOG(LogGridlands, Error, TEXT("%s: anchor %s not found"), *Id.ToString(), *Placement.Anchor.ToString());
			return false;
		}
		Location = Visual ? Visual->GetActorLocation() : Record->Location + CellCentre;
		Yaw = Visual ? Visual->GetActorRotation().Yaw : Record->Yaw;
		if (Placement.Offset.Num() == 3)
		{
			Location += FRotator(0.0, Yaw, 0.0).RotateVector(FVector(Placement.Offset[0], Placement.Offset[1], Placement.Offset[2]));
		}
	}
	else if (Placement.Transform.Location.Num() == 3)
	{
		Location = CellCentre + FVector(Placement.Transform.Location[0], Placement.Transform.Location[1], Placement.Transform.Location[2]);
		Yaw = Placement.Transform.Yaw;
	}
	if (Placement.Kind == TEXT("spawn"))
	{
		// The only place creatures come from (ADR-0014: threat from place; architecture rule).
		AGLCreature* Creature = World->SpawnActor<AGLCreature>(Location + FVector(0, 0, 70), FRotator(0.0, Yaw, 0.0));
		if (!Creature || !Creature->Setup(Placement.Definition, Id))
		{
			UE_LOG(LogGridlands, Error, TEXT("%s: could not spawn creature %s"), *Id.ToString(), *Placement.Definition.ToString());
			return false;
		}
		Creatures.Add(Id, Creature);
		Owned.Add(Creature);
		return true;
	}
	if (Placement.Kind == TEXT("structure"))
	{
		// Authored structures (P6) live in the structure subsystem, which owns their part states. With
		// deferred presentation only that authoritative model is made now; part actors follow (P7).
		const int32 YawQuarter = ((FMath::RoundToInt(Yaw / 90.0) % 4) + 4) % 4;
		return World->GetSubsystem<UGLStructureSubsystem>()->SpawnStructure(Id, Placement.Definition, CellId, Location, YawQuarter, bDeferPresentation);
	}
	if (Placement.Kind == TEXT("scatter"))
	{
		// Vegetation (P7): pure presentation that follows the ground and structures.
		if (bDeferPresentation)
		{
			PendingScatter.Add({ CellId, Id, Placement.Definition, Location, Placement.Radius * 100.0, Placement.Count });
			return true;
		}
		return SpawnScatter({ CellId, Id, Placement.Definition, Location, Placement.Radius * 100.0, Placement.Count });
	}
	if (Placement.Kind == TEXT("discovery"))
	{
		Discoveries.Add({ Id, Placement.Definition, Location, (Placement.Radius > 0.0 ? Placement.Radius : 8.0) * 100.0, CellId });
		return true;
	}
	if (Placement.Kind == TEXT("puzzle_site"))
	{
		AGLPuzzleSite* Site = World->SpawnActor<AGLPuzzleSite>(Location, FRotator(0.0, Yaw, 0.0));
		if (!Site || !Site->Setup(Placement.Definition))
		{
			UE_LOG(LogGridlands, Error, TEXT("%s: could not spawn puzzle site %s"), *Id.ToString(), *Placement.Definition.ToString());
			return false;
		}
		Owned.Add(Site);
		return true;
	}
	if (Placement.Kind == TEXT("glitch"))
	{
		AGLGlitch* Glitch = World->SpawnActor<AGLGlitch>(Location, FRotator(0.0, Yaw, 0.0));
		if (!Glitch || !Glitch->GetGlitch()->Setup(Placement.Definition, Id, Placement.Bindings))
		{
			UE_LOG(LogGridlands, Error, TEXT("%s: could not spawn glitch %s"), *Id.ToString(), *Placement.Definition.ToString());
			return false;
		}
		World->GetSubsystem<UGLGlitchSubsystem>()->Register(Glitch);
		Owned.Add(Glitch);
		return true;
	}
	AGLSalvageNode* Node = World->SpawnActor<AGLSalvageNode>(Location, FRotator(0.0, Yaw, 0.0));
	if (!Node || !Node->GetSalvageable()->Setup(Placement.Definition, Visual))
	{
		UE_LOG(LogGridlands, Error, TEXT("%s: could not spawn salvage node for %s"), *Id.ToString(), *Placement.Definition.ToString());
		return false;
	}
	Node->PlacementId = Id;
	SalvageNodes.Add(Id, Node);
	Owned.Add(Node);
	return true;
}

bool UGLPlacementSubsystem::SpawnScatter(const FGLPendingScatter& Scatter)
{
	AGLScatterPatch* Patch = GetWorld()->SpawnActor<AGLScatterPatch>(Scatter.Location, FRotator::ZeroRotator);
	if (!Patch || !Patch->Setup(Scatter.Placement, Scatter.Visual, Scatter.RadiusCm, Scatter.Count))
	{
		UE_LOG(LogGridlands, Error, TEXT("%s: could not scatter %s"), *Scatter.Placement.ToString(), *Scatter.Visual.ToString());
		if (Patch)
		{
			Patch->Destroy();
		}
		return false;
	}
	CellActors.FindOrAdd(Scatter.Cell).Add(Patch);
	return true;
}

int32 UGLPlacementSubsystem::PumpPresentation(const FVector& Where, double BudgetSeconds, double NearCm)
{
	const double Start = FPlatformTime::Seconds();
	int32 Made = GetWorld()->GetSubsystem<UGLStructureSubsystem>()->PumpPresentation(Where, BudgetSeconds, NearCm);
	while (RetiringScatter.Num() > 0 && BudgetSeconds >= 0.0 && (BudgetSeconds == 0.0 || FPlatformTime::Seconds() - Start < BudgetSeconds))
	{
		if (AActor* Patch = RetiringScatter.Pop(EAllowShrinking::No).Get())
		{
			Patch->Destroy();
		}
	}
	// Vegetation after the structures it grows around, nearest first, within what is left of the budget.
	while (PendingScatter.Num() > 0 && BudgetSeconds >= 0.0 && (BudgetSeconds == 0.0 || FPlatformTime::Seconds() - Start < BudgetSeconds))
	{
		int32 Nearest = 0;
		for (int32 I = 1; I < PendingScatter.Num(); ++I)
		{
			if (FVector::DistSquared2D(PendingScatter[I].Location, Where) < FVector::DistSquared2D(PendingScatter[Nearest].Location, Where))
			{
				Nearest = I;
			}
		}
		const FGLPendingScatter Next = PendingScatter[Nearest];
		PendingScatter.RemoveAtSwap(Nearest);
		Made += SpawnScatter(Next) ? 1 : 0;
	}
	return Made;
}

bool UGLPlacementSubsystem::IsCellPresented(FName CellId) const
{
	return !PendingScatter.ContainsByPredicate([CellId](const FGLPendingScatter& S) { return S.Cell == CellId; })
		&& GetWorld()->GetSubsystem<UGLStructureSubsystem>()->IsCellPresented(CellId);
}

int32 UGLPlacementSubsystem::PendingPresentation() const
{
	return PendingScatter.Num() + GetWorld()->GetSubsystem<UGLStructureSubsystem>()->PendingPresentation();
}

int32 UGLPlacementSubsystem::DespawnCell(FName CellId)
{
	if (!SpawnedCells.Remove(CellId))
	{
		return 0;
	}
	const double Start = FPlatformTime::Seconds();
	int32 Removed = 0;
	TArray<TWeakObjectPtr<AActor>> Owned;
	CellActors.RemoveAndCopyValue(CellId, Owned);
	for (const TWeakObjectPtr<AActor>& Actor : Owned)
	{
		if (AActor* Live = Actor.Get())
		{
			if (AGLScatterPatch* Patch = Cast<AGLScatterPatch>(Live))
			{
				Patch->SetActorHiddenInGame(true); // pure presentation: retired now, destroyed within the budget
				RetiringScatter.Add(Patch);
			}
			else
			{
				Live->Destroy(); // gameplay actors (creatures, glitches, salvage nodes, sites) leave at once
			}
			++Removed;
		}
	}
	const FString Prefix = FString::Printf(TEXT("placement.%s."), *CellShortName(CellId));
	auto OfCell = [&Prefix](FName Id) { return Id.ToString().StartsWith(Prefix); };
	for (auto It = SalvageNodes.CreateIterator(); It; ++It) { if (OfCell(It.Key())) { It.RemoveCurrent(); } }
	for (auto It = Creatures.CreateIterator(); It; ++It) { if (OfCell(It.Key())) { It.RemoveCurrent(); } }
	Discoveries.RemoveAll([CellId](const FGLDiscoverySite& Site) { return Site.Cell == CellId; });
	PendingScatter.RemoveAll([CellId](const FGLPendingScatter& S) { return S.Cell == CellId; }); // cancelled presentation
	Removed += GetWorld()->GetSubsystem<UGLStructureSubsystem>()->RemoveCell(CellId);
	GetWorld()->GetSubsystem<UGLGlitchSubsystem>()->Compact();
	UE_LOG(LogGridlands, Log, TEXT("Placements: despawned %d for %s (%.2f ms)"), Removed, *CellId.ToString(), (FPlatformTime::Seconds() - Start) * 1000.0);
	return Removed;
}

#if !UE_BUILD_SHIPPING
AGLCreature* UGLPlacementSubsystem::SpawnProofCreature(FName Def, const FVector& Location, double Yaw, FName Cell, FName VisualOverride, bool bPosed)
{
	AGLCreature* Creature = GetWorld()->SpawnActor<AGLCreature>(Location + FVector(0, 0, 70), FRotator(0.0, Yaw, 0.0));
	if (!Creature || !Creature->Setup(Def, TEXT("placement.proof.creature"), VisualOverride))
	{
		return nullptr;
	}
	if (bPosed)
	{
		Creature->SetActorTickEnabled(false); // a style proof holds still (no behaviour)
	}
	CellActors.FindOrAdd(Cell).Add(Creature);
	UE_LOG(LogGridlands, Log, TEXT("Placements: DEV proof creature %s at %s"), *Def.ToString(), *Location.ToCompactString());
	return Creature;
}
#endif

bool UGLPlacementSubsystem::IsPlacementOfCell(FName PlacementId, FName CellId)
{
	return PlacementId.ToString().StartsWith(FString::Printf(TEXT("placement.%s."), *CellShortName(CellId)));
}

AGLSalvageNode* UGLPlacementSubsystem::FindSalvageNode(FName PlacementId) const
{
	const TWeakObjectPtr<AGLSalvageNode>* Node = SalvageNodes.Find(PlacementId);
	return Node ? Node->Get() : nullptr;
}
