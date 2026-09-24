#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLPlacementSubsystem.generated.h"

class AGLSalvageNode;

/**
 * Spawns a cell's gameplay layer from Data/placement/<cell>/ (ADR-0018). Anchored placements
 * sit at their anchor (from the map's actor if loaded, else the exported anchor record) plus offset.
 * M4 spawns salvage nodes; other kinds arrive with their systems (glitches in M7).
 */
UCLASS()
class GRIDLANDSGAME_API UGLPlacementSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Spawns every supported placement of CellId. Returns how many actors were spawned. */
	int32 SpawnCell(FName CellId);

	AGLSalvageNode* FindSalvageNode(FName PlacementId) const;

private:
	TMap<FName, TWeakObjectPtr<AGLSalvageNode>> SalvageNodes;
};
