#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLGridBoundary.generated.h"

class UInstancedStaticMeshComponent;

/**
 * TEMPORARY development marker for a Grid cell's edges (P3): tall cyan posts and a rail, no
 * collision, so crossings are easy to see. Not Grid art.
 */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLGridBoundary : public AActor
{
	GENERATED_BODY()

public:
	AGLGridBoundary();
	void Setup(const FVector2D& CentreCm, double SizeMetres);

private:
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Posts;
};
