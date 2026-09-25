#pragma once

#include "CoreMinimal.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "GLCellNavBounds.generated.h"

class UBoxComponent;

/**
 * Navigation bounds that a cell declares from data at runtime, instead of an editor-placed brush
 * (cells and their ground are spawned at runtime, ADR-0022). A box component gives the volume
 * real extent. Components register during spawn, so after SetExtent the owner must call
 * UNavigationSystemV1::OnNavigationBoundsUpdated.
 */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLCellNavBounds : public ANavMeshBoundsVolume
{
	GENERATED_BODY()

public:
	AGLCellNavBounds();
	void SetExtent(const FVector& HalfExtentCm);

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Extent;
};
