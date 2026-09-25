#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLTerrainChunk.generated.h"

class FGLHeightfield;
class UDynamicMeshComponent;

/**
 * One square of a cell's runtime ground (ADR-0022): a dynamic mesh with complex-as-simple
 * collision that navigation reads. Rebuilt from the heightfield when an edit touches it.
 */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLTerrainChunk : public AActor
{
	GENERATED_BODY()

public:
	AGLTerrainChunk();

	/** Covers heightfield vertices [First, First + Verts - 1] on each axis. */
	void Setup(FIntPoint InFirstVertex, int32 InVertsPerSide);
	/** Rebuilds mesh and collision from the heightfield; bNotifyNavigation marks it dirty for navmesh. */
	void Rebuild(const FGLHeightfield& Field, bool bNotifyNavigation);
	bool Covers(const FIntRect& DirtyVertices) const;

	FIntPoint GetFirstVertex() const { return FirstVertex; }

	/** Cumulative rebuild cost across all chunks (performance harness). */
	static inline double MeshSeconds = 0.0;
	static inline double CollisionSeconds = 0.0;
	static inline double NavigationSeconds = 0.0;
	static inline int32 Rebuilds = 0;
	UDynamicMeshComponent* GetMesh() const { return Mesh; }

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDynamicMeshComponent> Mesh;
	FIntPoint FirstVertex = FIntPoint::ZeroValue;
	int32 VertsPerSide = 0;
};
