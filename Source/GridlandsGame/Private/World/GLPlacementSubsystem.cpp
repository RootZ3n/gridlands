#include "World/GLPlacementSubsystem.h"
#include "Mechanism/GLMechanism.h"
#include "Mechanism/GLMechanismSubsystem.h"
#include "World/GLNavRegionSubsystem.h"

#include "Presentation/GLScatterPatch.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"

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
	if (FParse::Param(FCommandLine::Get(), TEXT("GLTownBlock")))
	{
		AddTownBlock();
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("GLDungeonProof")))
	{
		AddDungeonProof();
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
		&& Placement.Kind != TEXT("spawn") && Placement.Kind != TEXT("discovery") && Placement.Kind != TEXT("structure") && Placement.Kind != TEXT("scatter")
		&& Placement.Kind != TEXT("mechanism") && Placement.Kind != TEXT("nav_region"))
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
	if (Placement.Kind == TEXT("spawn") || Placement.Kind == TEXT("salvage_node"))
	{
		// P8: the model now (what saves read and restore); the actor is presentation made from it.
		// Creatures come only from here (ADR-0014: threat from place; architecture rule).
		const bool bCreature = Placement.Kind == TEXT("spawn");
		if (bCreature ? !Content.Find<FGLCreatureDef>(Placement.Definition) : !Content.Find<FGLSalvageDef>(Placement.Definition))
		{
			UE_LOG(LogGridlands, Error, TEXT("%s: unknown %s %s"), *Id.ToString(), bCreature ? TEXT("creature") : TEXT("salvage"), *Placement.Definition.ToString());
			return false;
		}
		FGLActorPlacement& Model = ActorModels.Add(Id);
		Model.Placement = Id;
		Model.Kind = Placement.Kind;
		Model.Definition = Placement.Definition;
		Model.Cell = CellId;
		Model.Location = Location;
		Model.Yaw = Yaw;
		Model.LinkedVisual = Visual;
		if (bCreature)
		{
			// P9: its gameplay facts start at its home, calm, full health; patrol points are cell-local like its transform.
			Model.Creature.Home = Location;
			Model.Creature.Yaw = Yaw;
			Model.Creature.StampWorldSeconds = World->GetTimeSeconds();
			const UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			for (const FGLPlacementPointDef& Point : Placement.Patrol)
			{
				if (Point.Location.Num() == 3)
				{
					FVector At = CellCentre + FVector(Point.Location[0], Point.Location[1], Point.Location[2]);
					if (Terrain && Terrain->HasGround())
					{
						At.Z = Terrain->HeightAt(FVector2D(At));
					}
					Model.Creature.Patrol.Add(At);
				}
			}
		}
		const FGLPendingActor Unit{ CellId, Id, Placement.Kind, Location };
		if (bDeferPresentation)
		{
			PendingActors.Add(Unit);
			return true;
		}
		return MakeActor(Unit);
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
	if (Placement.Kind == TEXT("mechanism"))
	{
		// P9 (ADR-0037): its authoritative record now; its actor is presentation made from it.
		if (!World->GetSubsystem<UGLMechanismSubsystem>()->AddRecord(Id, Placement.Definition, CellId, Location, Yaw))
		{
			UE_LOG(LogGridlands, Error, TEXT("%s: unknown mechanism %s"), *Id.ToString(), *Placement.Definition.ToString());
			return false;
		}
		const FGLPendingActor Unit{ CellId, Id, Placement.Kind, Location };
		if (bDeferPresentation)
		{
			PendingActors.Add(Unit);
			return true;
		}
		return MakeActor(Unit);
	}
	if (Placement.Kind == TEXT("nav_region"))
	{
		// P9 (ADR-0029 as amended): data only; its invoker exists only while gameplay needs it.
		return World->GetSubsystem<UGLNavRegionSubsystem>()->AddRegion(Id, Placement.Definition, CellId, Location);
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
	// Glitch (P8): its authoritative record in the glitch subsystem now; its actor made from it.
	if (!World->GetSubsystem<UGLGlitchSubsystem>()->AddRecord(Id, Placement.Definition, CellId, Location, Yaw, Placement.Bindings))
	{
		UE_LOG(LogGridlands, Error, TEXT("%s: could not record glitch %s"), *Id.ToString(), *Placement.Definition.ToString());
		return false;
	}
	const FGLPendingActor Unit{ CellId, Id, Placement.Kind, Location };
	if (bDeferPresentation)
	{
		PendingActors.Add(Unit);
		return true;
	}
	return MakeActor(Unit);
}

bool UGLPlacementSubsystem::MakeActor(const FGLPendingActor& Pending)
{
	UWorld* World = GetWorld();
	if (Pending.Kind == TEXT("mechanism"))
	{
		AGLMechanism* Mechanism = World->GetSubsystem<UGLMechanismSubsystem>()->Present(Pending.Placement);
		if (Mechanism)
		{
			CellActors.FindOrAdd(Pending.Cell).Add(Mechanism);
		}
		return Mechanism != nullptr;
	}
	if (Pending.Kind == TEXT("glitch"))
	{
		AGLGlitch* Glitch = World->GetSubsystem<UGLGlitchSubsystem>()->Present(Pending.Placement);
		if (Glitch)
		{
			CellActors.FindOrAdd(Pending.Cell).Add(Glitch);
		}
		return Glitch != nullptr;
	}
	FGLActorPlacement* Model = ActorModels.Find(Pending.Placement);
	if (!Model || Model->Actor.IsValid() || Model->bSalvaged || Model->Creature.Outcome == EGLCreatureOutcome::Defeated)
	{
		return false; // gone, already made, or never made: a salvaged node or defeated creature has no presentation
	}
	const FRotator Rotation(0.0, Model->Yaw, 0.0);
	if (Model->Kind == TEXT("spawn"))
	{
		AGLCreature* Creature = World->SpawnActor<AGLCreature>(Model->Location + FVector(0, 0, 70), Rotation);
		if (!Creature || !Creature->Setup(Model->Definition, Model->Placement))
		{
			UE_LOG(LogGridlands, Error, TEXT("%s: could not present creature %s"), *Model->Placement.ToString(), *Model->Definition.ToString());
			if (Creature)
			{
				Creature->Destroy();
			}
			return false;
		}
		Model->Actor = Creature;
		CellActors.FindOrAdd(Model->Cell).Add(Creature);
		// P9: made from its model as it is now (a neutralized one is presented held, inert).
		Creature->RestoreFromModel(Model->Creature, World->GetTimeSeconds());
		return true;
	}
	AGLSalvageNode* Node = World->SpawnActor<AGLSalvageNode>(Model->Location, Rotation);
	if (!Node || !Node->GetSalvageable()->Setup(Model->Definition, Model->LinkedVisual.Get()))
	{
		UE_LOG(LogGridlands, Error, TEXT("%s: could not present salvage node for %s"), *Model->Placement.ToString(), *Model->Definition.ToString());
		if (Node)
		{
			Node->Destroy();
		}
		return false;
	}
	Node->PlacementId = Model->Placement;
	// The model follows the node: salvaged is a fact of the placement, whatever becomes of the actor.
	Node->GetSalvageable()->OnSalvaged.AddWeakLambda(this, [this, Placement = Model->Placement](AActor*)
	{
		if (FGLActorPlacement* Salvaged = ActorModels.Find(Placement))
		{
			Salvaged->bSalvaged = true;
		}
	});
	Model->Actor = Node;
	CellActors.FindOrAdd(Model->Cell).Add(Node);
	return true;
}

void UGLPlacementSubsystem::Retire(AActor* Actor)
{
	if (AGLCreature* Creature = Cast<AGLCreature>(Actor))
	{
		Creature->Retire();
	}
	else if (AGLSalvageNode* Node = Cast<AGLSalvageNode>(Actor))
	{
		Node->GetSalvageable()->OnSalvaged.RemoveAll(this);
		Node->GetSalvageable()->Retire();
	}
	else
	{
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
		Actor->SetActorTickEnabled(false);
	}
	RetiringActors.Add(Actor);
}

bool UGLPlacementSubsystem::PresentActor(FName PlacementId)
{
	const int32 Index = PendingActors.IndexOfByPredicate([PlacementId](const FGLPendingActor& P) { return P.Placement == PlacementId; });
	if (Index == INDEX_NONE)
	{
		return false;
	}
	const FGLPendingActor Unit = PendingActors[Index];
	PendingActors.RemoveAt(Index);
	MakeActor(Unit);
	return true;
}

bool UGLPlacementSubsystem::RestoreSalvaged(FName PlacementId)
{
	FGLActorPlacement* Model = ActorModels.Find(PlacementId);
	if (!Model || Model->Kind != TEXT("salvage_node"))
	{
		return false;
	}
	Model->bSalvaged = true;
	if (AGLSalvageNode* Node = Cast<AGLSalvageNode>(Model->Actor.Get()))
	{
		Node->GetSalvageable()->RestoreSalvaged(); // presented already: hidden, and its linked visual with it
	}
	else if (AActor* Linked = Model->LinkedVisual.Get())
	{
		Linked->SetActorHiddenInGame(true); // the authored actor it stands for is world geometry: gone now, not when presented
		Linked->SetActorEnableCollision(false);
	}
	return true;
}

bool UGLPlacementSubsystem::RestoreDefeated(FName PlacementId)
{
	FGLActorPlacement* Model = ActorModels.Find(PlacementId);
	if (!Model || Model->Kind != TEXT("spawn"))
	{
		return false;
	}
	Model->Creature.Outcome = EGLCreatureOutcome::Defeated; // silently: a restore never resolves an encounter again
	Model->Creature.State = EGLCreatureState::Defeated;
	if (AGLCreature* Creature = Cast<AGLCreature>(Model->Actor.Get()))
	{
		Creature->RestoreDefeated();
	}
	return true;
}

AGLCreature* UGLPlacementSubsystem::FindCreature(FName PlacementId) const
{
	const FGLActorPlacement* Model = ActorModels.Find(PlacementId);
	return Model ? Cast<AGLCreature>(Model->Actor.Get()) : nullptr;
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
	auto HasBudget = [&]() { return BudgetSeconds >= 0.0 && (BudgetSeconds == 0.0 || FPlatformTime::Seconds() - Start < BudgetSeconds); };
	// P8: gameplay actors of unloaded cells (already inert) go within the budget, before anything new is made.
	while (RetiringActors.Num() > 0 && HasBudget())
	{
		if (AActor* Actor = RetiringActors.Pop(EAllowShrinking::No).Get())
		{
			Actor->Destroy();
		}
	}
	// Gameplay actors (glitches, salvage nodes, creatures) from their models, nearest first: within NearCm
	// always (what Zenny can reach is never missing), the rest within the budget.
	while (PendingActors.Num() > 0)
	{
		int32 Nearest = 0;
		for (int32 I = 1; I < PendingActors.Num(); ++I)
		{
			if (FVector::DistSquared2D(PendingActors[I].Location, Where) < FVector::DistSquared2D(PendingActors[Nearest].Location, Where))
			{
				Nearest = I;
			}
		}
		const bool bNear = FVector::DistSquared2D(PendingActors[Nearest].Location, Where) <= NearCm * NearCm;
		if (!bNear && !HasBudget())
		{
			break;
		}
		const FGLPendingActor Next = PendingActors[Nearest];
		PendingActors.RemoveAtSwap(Nearest);
		Made += MakeActor(Next) ? 1 : 0;
	}
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
		&& !PendingActors.ContainsByPredicate([CellId](const FGLPendingActor& P) { return P.Cell == CellId; })
		&& GetWorld()->GetSubsystem<UGLStructureSubsystem>()->IsCellPresented(CellId);
}

int32 UGLPlacementSubsystem::PendingPresentation() const
{
	return PendingScatter.Num() + PendingActors.Num() + GetWorld()->GetSubsystem<UGLStructureSubsystem>()->PendingPresentation();
}

int32 UGLPlacementSubsystem::DespawnCell(FName CellId)
{
	if (!SpawnedCells.Remove(CellId))
	{
		return 0;
	}
	const double Start = FPlatformTime::Seconds();
	int32 Removed = 0;
	// The models go at once (saves have already stowed the cell from them); glitches leave every query now.
	for (auto It = ActorModels.CreateIterator(); It; ++It)
	{
		if (It.Value().Cell == CellId)
		{
			It.RemoveCurrent();
		}
	}
	GetWorld()->GetSubsystem<UGLGlitchSubsystem>()->RemoveCell(CellId);
	GetWorld()->GetSubsystem<UGLMechanismSubsystem>()->RemoveCell(CellId); // P9: records go now; actors retire below
	GetWorld()->GetSubsystem<UGLNavRegionSubsystem>()->RemoveCell(CellId);
	PendingActors.RemoveAll([CellId](const FGLPendingActor& P) { return P.Cell == CellId; }); // cancelled presentation
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
			else if (Live->IsA<AGLPuzzleSite>())
			{
				Live->Destroy(); // stateless and few: leaves at once
			}
			else
			{
				Retire(Live); // P8: gameplay actors are inert now, destroyed within the budget
			}
			++Removed;
		}
	}
	Discoveries.RemoveAll([CellId](const FGLDiscoverySite& Site) { return Site.Cell == CellId; });
	PendingScatter.RemoveAll([CellId](const FGLPendingScatter& S) { return S.Cell == CellId; }); // cancelled presentation
	Removed += GetWorld()->GetSubsystem<UGLStructureSubsystem>()->RemoveCell(CellId);
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
	const FGLActorPlacement* Model = ActorModels.Find(PlacementId);
	return Model ? Cast<AGLSalvageNode>(Model->Actor.Get()) : nullptr;
}
