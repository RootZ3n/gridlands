#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "GLTerrainCollision.generated.h"

struct FGLChunkSnapshot;
namespace UE::Geometry { class FDynamicMesh3; }

/**
 * SPIKE (terrain heightfield-collision decision, after ADR-0034; NOT canonical): how terrain chunks get
 * their collision. Chosen once per process (`-GLTerrainCollision=N` or `gl.Terrain.CollisionMode`
 * before any chunk exists); the default is the canonical ADR-0034 path.
 */
enum class EGLTerrainCollisionMode : uint8
{
	ComponentTrimesh = 0, // canonical: the dynamic mesh component cooks complex-as-simple, synchronously
	Heightfield = 1,      // spike option B: a Chaos heightfield built off the game thread, attached as a static body
	WorkerTrimesh = 2,    // spike option A: the same triangles built off the game thread, attached the same way
};

namespace GLTerrainCollision
{
	GRIDLANDSGAME_API EGLTerrainCollisionMode GetMode();
	GRIDLANDSGAME_API const TCHAR* ModeName(EGLTerrainCollisionMode Mode);
	/** Heightfield mode: the render mesh splits each quad between (x, y) and (x+1, y+1), as Chaos does. */
	GRIDLANDSGAME_API bool RenderSplitsMainDiagonal();
}

/** Collision geometry built for one chunk (anywhere), attached on the game thread. Opaque outside the .cpp. */
struct FGLChunkCollisionGeometry;

namespace GLTerrainCollision
{
	/** Builds a chunk's collision from its snapshot (thread-safe; nullptr for ComponentTrimesh). */
	GRIDLANDSGAME_API TSharedPtr<const FGLChunkCollisionGeometry> Build(EGLTerrainCollisionMode Mode, const FGLChunkSnapshot& Snapshot);
}

/**
 * SPIKE: a chunk's collision as one static Chaos body (the engine path Landscape uses for its
 * heightfield: FPhysicsInterface::CreateActor, one shape, AddActorsToScene), with its own navigation
 * export. No render state.
 */
UCLASS(NotBlueprintable)
class GRIDLANDSGAME_API UGLTerrainCollisionComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UGLTerrainCollisionComponent();

	/** Replaces the collision (the body is recreated at once); nullptr removes it. */
	void SetGeometry(TSharedPtr<const FGLChunkCollisionGeometry> InGeometry);
	bool HasGeometry() const { return Geometry.IsValid(); }
	bool HasBody() const;
	/** The heightfield or trimesh's local surface height at a local vertex, for agreement tests (NAN if none). */
	double GeometryHeightAtVertex(int32 X, int32 Y) const;

	virtual bool ShouldCreatePhysicsState() const override;
	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool IsNavigationRelevant() const override;
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
	virtual void OnActorEnableCollisionChanged() override;
	virtual UBodySetup* GetBodySetup() override { return nullptr; }

private:
	void RefreshNavigationRelevance();
	TSharedPtr<const FGLChunkCollisionGeometry> Geometry;
};
