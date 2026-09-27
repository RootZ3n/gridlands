#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GLTerrainChunk.generated.h"

class FGLHeightfield;
class UDynamicMeshComponent;
class UGLTerrainCollisionComponent;
struct FGLChunkCollisionGeometry;

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
	/** Builds mesh and collision geometry from a snapshot on this thread (edits, synchronous loads), then applies them. */
	void ApplySnapshot(const FGLChunkSnapshot& Snapshot, bool bNotifyNavigation);
	/**
	 * Collision (ADR-0035) is the heightfield geometry built with the mesh (on a worker when streaming). The
	 * measurement-only component-trimesh mode instead cooks the dynamic mesh's own collision.
	 */
	void ApplyMesh(UE::Geometry::FDynamicMesh3&& Built, bool bNotifyNavigation, TSharedPtr<const FGLChunkCollisionGeometry> Collision = nullptr);
	bool IsBuilt() const { return bBuilt; }
	/** A pooled chunk holds nothing: an empty mesh, no collision body, hidden, no collision. */
	bool IsEmptyAndInert() const;
	/** How many meshes this actor has been given in its life (0: its next mesh is its first, the expensive build). */
	int32 GetLifetimeMeshes() const { return LifetimeMeshes; }
	/** Pooling (P5): drops the mesh and collision (freeing them now) so the actor can be reused. */
	void ClearForPool();

	/**
	 * Collision (ADR-0035): a heightfield body, built with the mesh, attached in ~0.01 ms. The render mesh
	 * never cooks (it defers collision; ADR-0034 found SetMesh cooking ~5 ms per 64 m chunk).
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
	/** The collision body (ADR-0035; null only in the measurement-only component-trimesh mode). */
	UGLTerrainCollisionComponent* GetCollisionComponent() const { return Collision; }
	/** Game-thread seconds spent building collision geometry (edits, sync loads) and worker microseconds (streaming). */
	static inline double CollisionBuildGameThreadSeconds = 0.0;
	static inline std::atomic<int64> CollisionBuildWorkerMicros{0};

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDynamicMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UGLTerrainCollisionComponent> Collision;
	void EnsureSeparateCollision();
	FIntPoint FirstVertex = FIntPoint::ZeroValue;
	int32 VertsPerSide = 0;
	bool bBuilt = false;
	int32 LifetimeMeshes = 0;
};
