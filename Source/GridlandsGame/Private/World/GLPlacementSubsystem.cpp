#include "World/GLPlacementSubsystem.h"

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

int32 UGLPlacementSubsystem::SpawnCell(FName CellId)
{
	UWorld* World = GetWorld();
	const FGLContentRegistry& Content = GLContent::Get();
	if (SpawnedCells.Contains(CellId))
	{
		UE_LOG(LogGridlands, Warning, TEXT("Placements: %s is already spawned (not spawning twice)"), *CellId.ToString());
		return 0;
	}
	SpawnedCells.Add(CellId);
	// Placement coordinates are cell-local (P3, ADR-0026): offset by the cell's world centre.
	const FGLCellDef* Cell = Content.Find<FGLCellDef>(CellId);
	const FVector CellCentre = Cell ? FVector(Cell->CentreCm(), 0.0) : FVector::ZeroVector;
	TArray<TWeakObjectPtr<AActor>>& Owned = CellActors.FindOrAdd(CellId);
	const FString Prefix = FString::Printf(TEXT("placement.%s."), *CellShortName(CellId));
	int32 Spawned = 0;
	Content.ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLPlacementDef* Placement = Entry.Definition.GetPtr<FGLPlacementDef>();
		if (!Placement || Entry.Kind != TEXT("placement") || !Entry.Id.ToString().StartsWith(Prefix)
			|| (Placement->Kind != TEXT("salvage_node") && Placement->Kind != TEXT("glitch") && Placement->Kind != TEXT("puzzle_site")
				&& Placement->Kind != TEXT("spawn") && Placement->Kind != TEXT("discovery") && Placement->Kind != TEXT("structure")))
		{
			return;
		}
		FVector Location = FVector::ZeroVector;
		double Yaw = 0.0;
		AActor* Visual = nullptr;
		if (Placement->IsAnchored())
		{
			Visual = FindAnchoredActor(World, Placement->Anchor);
			const FGLAnchorRecord* Record = Content.FindAnchor(Placement->Anchor);
			if (!Visual && !Record)
			{
				UE_LOG(LogGridlands, Error, TEXT("%s: anchor %s not found"), *Entry.Id.ToString(), *Placement->Anchor.ToString());
				return;
			}
			Location = Visual ? Visual->GetActorLocation() : Record->Location + CellCentre;
			Yaw = Visual ? Visual->GetActorRotation().Yaw : Record->Yaw;
			if (Placement->Offset.Num() == 3)
			{
				Location += FRotator(0.0, Yaw, 0.0).RotateVector(FVector(Placement->Offset[0], Placement->Offset[1], Placement->Offset[2]));
			}
		}
		else if (Placement->Transform.Location.Num() == 3)
		{
			Location = CellCentre + FVector(Placement->Transform.Location[0], Placement->Transform.Location[1], Placement->Transform.Location[2]);
			Yaw = Placement->Transform.Yaw;
		}
		if (Placement->Kind == TEXT("spawn"))
		{
			// The only place creatures come from (ADR-0014: threat from place; architecture rule).
			AGLCreature* Creature = World->SpawnActor<AGLCreature>(Location + FVector(0, 0, 70), FRotator(0.0, Yaw, 0.0));
			if (!Creature || !Creature->Setup(Placement->Definition, Entry.Id))
			{
				UE_LOG(LogGridlands, Error, TEXT("%s: could not spawn creature %s"), *Entry.Id.ToString(), *Placement->Definition.ToString());
				return;
			}
			Creatures.Add(Entry.Id, Creature);
			Owned.Add(Creature);
			++Spawned;
			return;
		}
		if (Placement->Kind == TEXT("structure"))
		{
			// Authored structures (P6) live in the structure subsystem, which owns their part states.
			const int32 YawQuarter = ((FMath::RoundToInt(Yaw / 90.0) % 4) + 4) % 4;
			if (World->GetSubsystem<UGLStructureSubsystem>()->SpawnStructure(Entry.Id, Placement->Definition, CellId, Location, YawQuarter))
			{
				++Spawned;
			}
			return;
		}
		if (Placement->Kind == TEXT("discovery"))
		{
			Discoveries.Add({ Entry.Id, Placement->Definition, Location, (Placement->Radius > 0.0 ? Placement->Radius : 8.0) * 100.0, CellId });
			++Spawned;
			return;
		}
		if (Placement->Kind == TEXT("puzzle_site"))
		{
			AGLPuzzleSite* Site = World->SpawnActor<AGLPuzzleSite>(Location, FRotator(0.0, Yaw, 0.0));
			if (!Site || !Site->Setup(Placement->Definition))
			{
				UE_LOG(LogGridlands, Error, TEXT("%s: could not spawn puzzle site %s"), *Entry.Id.ToString(), *Placement->Definition.ToString());
				return;
			}
			Owned.Add(Site);
			++Spawned;
			return;
		}
		if (Placement->Kind == TEXT("glitch"))
		{
			AGLGlitch* Glitch = World->SpawnActor<AGLGlitch>(Location, FRotator(0.0, Yaw, 0.0));
			if (!Glitch || !Glitch->GetGlitch()->Setup(Placement->Definition, Entry.Id, Placement->Bindings))
			{
				UE_LOG(LogGridlands, Error, TEXT("%s: could not spawn glitch %s"), *Entry.Id.ToString(), *Placement->Definition.ToString());
				return;
			}
			World->GetSubsystem<UGLGlitchSubsystem>()->Register(Glitch);
			Owned.Add(Glitch);
			++Spawned;
			return;
		}
		AGLSalvageNode* Node = World->SpawnActor<AGLSalvageNode>(Location, FRotator(0.0, Yaw, 0.0));
		if (!Node || !Node->GetSalvageable()->Setup(Placement->Definition, Visual))
		{
			UE_LOG(LogGridlands, Error, TEXT("%s: could not spawn salvage node for %s"), *Entry.Id.ToString(), *Placement->Definition.ToString());
			return;
		}
		Node->PlacementId = Entry.Id;
		SalvageNodes.Add(Entry.Id, Node);
		Owned.Add(Node);
		++Spawned;
	});
	UE_LOG(LogGridlands, Log, TEXT("Placements: spawned %d for %s"), Spawned, *CellId.ToString());
	return Spawned;
}

int32 UGLPlacementSubsystem::DespawnCell(FName CellId)
{
	if (!SpawnedCells.Remove(CellId))
	{
		return 0;
	}
	int32 Removed = 0;
	TArray<TWeakObjectPtr<AActor>> Owned;
	CellActors.RemoveAndCopyValue(CellId, Owned);
	for (const TWeakObjectPtr<AActor>& Actor : Owned)
	{
		if (AActor* Live = Actor.Get())
		{
			Live->Destroy();
			++Removed;
		}
	}
	const FString Prefix = FString::Printf(TEXT("placement.%s."), *CellShortName(CellId));
	auto OfCell = [&Prefix](FName Id) { return Id.ToString().StartsWith(Prefix); };
	for (auto It = SalvageNodes.CreateIterator(); It; ++It) { if (OfCell(It.Key())) { It.RemoveCurrent(); } }
	for (auto It = Creatures.CreateIterator(); It; ++It) { if (OfCell(It.Key())) { It.RemoveCurrent(); } }
	Discoveries.RemoveAll([CellId](const FGLDiscoverySite& Site) { return Site.Cell == CellId; });
	Removed += GetWorld()->GetSubsystem<UGLStructureSubsystem>()->RemoveCell(CellId);
	GetWorld()->GetSubsystem<UGLGlitchSubsystem>()->Compact();
	UE_LOG(LogGridlands, Log, TEXT("Placements: despawned %d for %s"), Removed, *CellId.ToString());
	return Removed;
}

#if !UE_BUILD_SHIPPING
AGLCreature* UGLPlacementSubsystem::SpawnProofCreature(FName Def, const FVector& Location, double Yaw, FName Cell)
{
	AGLCreature* Creature = GetWorld()->SpawnActor<AGLCreature>(Location + FVector(0, 0, 70), FRotator(0.0, Yaw, 0.0));
	if (!Creature || !Creature->Setup(Def, TEXT("placement.proof.creature")))
	{
		return nullptr;
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
