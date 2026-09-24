#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLPlacementSubsystem.generated.h"

class AGLGlitch;
class AGLSalvageNode;

/**
 * Spawns a cell's gameplay layer from Data/placement/<cell>/ (ADR-0018). Anchored placements
 * sit at their anchor (from the map's actor if loaded, else the exported anchor record) plus offset.
 * Spawns salvage nodes (M4) and glitches (M7); other kinds arrive with their systems.
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
