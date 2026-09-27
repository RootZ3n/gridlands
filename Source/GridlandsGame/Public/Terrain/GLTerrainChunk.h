#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLTerrainChunk.generated.h"

class FGLHeightfield;
class UDynamicMeshComponent;

/** Where a chunk actor is in its life (P5 pool; the terrain pool work after P7.1 made it explicit). */
enum class EGLChunkState : uint8
{
	Live,      // owned by exactly one slot of one cell's ground
	Retiring,  // its cell unloaded: hidden, no collision, waiting to be cleared
	Pooled,    // cleared (no mesh, no collision, no navigation, no owner): the only state that may be reused
};

namespace UE::Geometry { class FDynamicMesh3; }

/** A chunk's heights and base (with one vertex of margin), copied so its mesh can build on a worker thread. */
struct FGLChunkSnapshot
{
	FIntPoint First = FIntPoint::ZeroValue;
	int32 Verts = 0;
	int32 FieldVertsX = 0;
	int32 FieldVertsY = 0;
	double Spacing = 100.0;
	TArray<float> Heights;
	TArray<float> Base;
};

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
	/** Rebuilds mesh and collision from the heightfield now (edits, tools); bNotifyNavigation marks it dirty for navmesh. */
	void Rebuild(const FGLHeightfield& Field, bool bNotifyNavigation);

	/** Streaming (P5): copy what a worker needs, build the mesh anywhere, apply it on the game thread. */
	static FGLChunkSnapshot MakeSnapshot(const FGLHeightfield& Field, FIntPoint First, int32 Verts);
	static UE::Geometry::FDynamicMesh3 BuildMesh(const FGLChunkSnapshot& Snapshot);
	void ApplyMesh(UE::Geometry::FDynamicMesh3&& Built, bool bNotifyNavigation);
	bool IsBuilt() const { return bBuilt; }
	/** A pooled chunk holds nothing: an empty mesh, no collision body, hidden, no collision. */
	bool IsEmptyAndInert() const;
	/** How many meshes this actor has been given in its life (0: its next mesh is its first, the expensive build). */
	int32 GetLifetimeMeshes() const { return LifetimeMeshes; }
	/** Pooling (P5): drops the mesh and collision (freeing them now) so the actor can be reused. */
	void ClearForPool();

	/**
	 * Collision (after P7.1): the component defers collision, so ApplyMesh's single synchronous cook is the
	 * only one. SetMesh used to cook complex collision synchronously (~5 ms per 64 m chunk), and ApplyMesh
	 * then requested an async cook whose completion cost another ~7.5 ms of game thread (measured,
	 * gl.Perf.ChunkApply). One cook, where the streaming budget sees it.
	 */
	EGLChunkState State = EGLChunkState::Live;
	/** The cell ground that owns it while Live (NAME_None otherwise): reuse must never keep it. */
	FName OwnerCell;
	int32 GetVertsPerSide() const { return VertsPerSide; }
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
	bool bBuilt = false;
	int32 LifetimeMeshes = 0;
};
