#include "World/GLPlacementSubsystem.h"

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
	const FString Prefix = FString::Printf(TEXT("placement.%s."), *CellShortName(CellId));
	int32 Spawned = 0;
	Content.ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLPlacementDef* Placement = Entry.Definition.GetPtr<FGLPlacementDef>();
		if (!Placement || Entry.Kind != TEXT("placement") || !Entry.Id.ToString().StartsWith(Prefix)
			|| (Placement->Kind != TEXT("salvage_node") && Placement->Kind != TEXT("glitch") && Placement->Kind != TEXT("puzzle_site")
				&& Placement->Kind != TEXT("spawn") && Placement->Kind != TEXT("discovery")))
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
			Location = Visual ? Visual->GetActorLocation() : Record->Location;
			Yaw = Visual ? Visual->GetActorRotation().Yaw : Record->Yaw;
			if (Placement->Offset.Num() == 3)
			{
				Location += FRotator(0.0, Yaw, 0.0).RotateVector(FVector(Placement->Offset[0], Placement->Offset[1], Placement->Offset[2]));
			}
		}
		else if (Placement->Transform.Location.Num() == 3)
		{
			Location = FVector(Placement->Transform.Location[0], Placement->Transform.Location[1], Placement->Transform.Location[2]);
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
			++Spawned;
			return;
		}
		if (Placement->Kind == TEXT("discovery"))
		{
			Discoveries.Add({ Entry.Id, Placement->Definition, Location, (Placement->Radius > 0.0 ? Placement->Radius : 8.0) * 100.0 });
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
		++Spawned;
	});
	UE_LOG(LogGridlands, Log, TEXT("Placements: spawned %d for %s"), Spawned, *CellId.ToString());
	return Spawned;
}

AGLSalvageNode* UGLPlacementSubsystem::FindSalvageNode(FName PlacementId) const
{
	const TWeakObjectPtr<AGLSalvageNode>* Node = SalvageNodes.Find(PlacementId);
	return Node ? Node->Get() : nullptr;
}
