#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "GLTerrainCollision.generated.h"

struct FGLChunkSnapshot;
namespace UE::Geometry { class FDynamicMesh3; }

/**
 * How terrain chunks get their collision (ADR-0035). The heightfield is canonical. The others remain for
 * measurement and as the documented fallback, chosen once per process (`-GLTerrainCollision=N` or
 * `gl.Terrain.CollisionMode` before any chunk exists).
 */
enum class EGLTerrainCollisionMode : uint8
{
	ComponentTrimesh = 0, // measurement only: ADR-0034's dynamic-mesh cook (synchronous, ~5 ms per chunk)
	Heightfield = 1,      // CANONICAL: a Chaos heightfield built off the game thread, attached as one static body
	WorkerTrimesh = 2,    // the documented fallback (measurement only): the same triangles built off the game thread
};

namespace GLTerrainCollision
{
	GRIDLANDSGAME_API EGLTerrainCollisionMode GetMode();
	GRIDLANDSGAME_API const TCHAR* ModeName(EGLTerrainCollisionMode Mode);
}

/** Collision geometry built for one chunk (anywhere), attached on the game thread. Opaque outside the .cpp. */
struct FGLChunkCollisionGeometry;

namespace GLTerrainCollision
{
	/** Builds a chunk's collision from its snapshot (thread-safe; nullptr for ComponentTrimesh). */
	GRIDLANDSGAME_API TSharedPtr<const FGLChunkCollisionGeometry> Build(EGLTerrainCollisionMode Mode, const FGLChunkSnapshot& Snapshot);
}

/**
 * A chunk's collision as one static Chaos body (the engine path Landscape uses for its
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
