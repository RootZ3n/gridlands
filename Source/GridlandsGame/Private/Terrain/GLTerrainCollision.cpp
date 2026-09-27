#include "Terrain/GLTerrainCollision.h"

#include "AI/NavigationSystemHelpers.h"
#include "Chaos/HeightField.h"
#include "Chaos/ShapeInstance.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "AI/NavigationSystemBase.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Physics/PhysicsFiltering.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceScene.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "Terrain/GLTerrainChunk.h"

namespace
{
	TAutoConsoleVariable<int32> CVarCollisionMode(
		TEXT("gl.Terrain.CollisionMode"), 1,
		TEXT("Terrain chunk collision (ADR-0035). 1 = heightfield (canonical); for measurement only: 0 = the ADR-0034 component trimesh cook, 2 = worker-built trimesh (the documented fallback). Read when the first chunk is made."),
		ECVF_Default);
}

EGLTerrainCollisionMode GLTerrainCollision::GetMode()
{
	static const EGLTerrainCollisionMode Mode = []
	{
		int32 Value = CVarCollisionMode.GetValueOnAnyThread();
		FParse::Value(FCommandLine::Get(), TEXT("-GLTerrainCollision="), Value);
		return static_cast<EGLTerrainCollisionMode>(FMath::Clamp(Value, 0, 2)); // 1 unless asked otherwise
	}();
	return Mode;
}

bool GLTerrainCollision::RenderSplitsMainDiagonal()
{
	return GetMode() == EGLTerrainCollisionMode::Heightfield;
}

const TCHAR* GLTerrainCollision::ModeName(EGLTerrainCollisionMode Mode)
{
	switch (Mode)
	{
	case EGLTerrainCollisionMode::Heightfield: return TEXT("heightfield");
	case EGLTerrainCollisionMode::WorkerTrimesh: return TEXT("worker-trimesh");
	default: return TEXT("component-trimesh");
	}
}

/**
 * The geometry, in the chunk's local space (the chunk actor sits at its first vertex, Z = 0):
 * - Heightfield (canonical, ADR-0035): a V x V Chaos heightfield at 1 unit per sample, scaled to the
 *   spacing; sample [Row][Col] is local vertex X = Col, Y = Row. Chaos splits each cell between (x, y) and
 *   (x+1, y+1), and the render mesh is split the same way (RenderSplitsMainDiagonal), so what is seen is
 *   what collides. Left unrotated: a turned (transformed) heightfield made Chaos sweeps miss contacts lying
 *   exactly on chunk seams (measured in the spike).
 * - WorkerTrimesh (the documented fallback, measurement only): the render mesh's own triangles, winding
 *   flipped as the engine cook flips them.
 */
struct FGLChunkCollisionGeometry
{
	EGLTerrainCollisionMode Mode = EGLTerrainCollisionMode::Heightfield;
	Chaos::FImplicitObjectPtr Implicit;      // what the body holds (already in local space)
	Chaos::FHeightFieldPtr Heightfield;      // Heightfield (scaled for the body; the navigation export reads indices)
	FTransform HeightfieldToLocal;           // Heightfield: scale, quarter turn, offset
	Chaos::FTriangleMeshImplicitObjectPtr Trimesh; // WorkerTrimesh
	int32 Verts = 0;
	double Spacing = 100.0;
	FBox LocalBounds = FBox(ForceInit);
};

TSharedPtr<const FGLChunkCollisionGeometry> GLTerrainCollision::Build(EGLTerrainCollisionMode Mode, const FGLChunkSnapshot& Snap)
{
	if (Mode == EGLTerrainCollisionMode::ComponentTrimesh)
	{
		return nullptr;
	}
	const int32 V = Snap.Verts, Side = V + 2;
	auto H = [&Snap, Side](int32 X, int32 Y) { return static_cast<double>(Snap.Heights[(Y + 1) * Side + (X + 1)]); };
	TSharedRef<FGLChunkCollisionGeometry> G = MakeShared<FGLChunkCollisionGeometry>();
	G->Mode = Mode;
	G->Verts = V;
	G->Spacing = Snap.Spacing;
	for (int32 Y = 0; Y < V; ++Y)
	{
		for (int32 X = 0; X < V; ++X)
		{
			G->LocalBounds += FVector(X * Snap.Spacing, Y * Snap.Spacing, H(X, Y));
		}
	}
	if (Mode == EGLTerrainCollisionMode::Heightfield)
	{
		TArray<Chaos::FReal> Heights;
		Heights.SetNumUninitialized(V * V);
		for (int32 Row = 0; Row < V; ++Row)
		{
			for (int32 Col = 0; Col < V; ++Col)
			{
				Heights[Row * V + Col] = H(Col, Row);
			}
		}
		// One material index PER CELL, never the single "default" entry: with one entry, Chaos's
		// GetMaterialIndex fails its bounds check for every cell but the first and returns 255, which IsHole
		// reads as a hole. The navigation export then skipped every cell, and overlap/sweep queries skip holes.
		TArray<uint8> Materials;
		Materials.SetNumZeroed((V - 1) * (V - 1));
		G->Heightfield = Chaos::FHeightFieldPtr(new Chaos::FHeightField(MoveTemp(Heights), MoveTemp(Materials), V, V, Chaos::FVec3(1)));
		// Unrotated: a turned (transformed) heightfield made Chaos sweeps miss contacts exactly on chunk seams.
		G->HeightfieldToLocal = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(Snap.Spacing, Snap.Spacing, 1.0));
		// One object serves both: the body sees it scaled to the spacing (as Landscape scales its geometry);
		// the navigation export reads raw sample indices and carries the scale in its own transform.
		G->Heightfield->SetScale(Chaos::FVec3(Snap.Spacing, Snap.Spacing, 1.0));
		G->Implicit = Chaos::FImplicitObjectPtr(G->Heightfield);
	}
	else
	{
		Chaos::FTriangleMeshImplicitObject::ParticlesType Particles;
		Particles.AddParticles(V * V);
		for (int32 Y = 0; Y < V; ++Y)
		{
			for (int32 X = 0; X < V; ++X)
			{
				Particles.SetX(Y * V + X, Chaos::FVec3f(X * Snap.Spacing, Y * Snap.Spacing, H(X, Y)));
			}
		}
		TArray<Chaos::TVec3<uint16>> Triangles;
		Triangles.Reserve((V - 1) * (V - 1) * 2);
		for (int32 Y = 0; Y + 1 < V; ++Y)
		{
			for (int32 X = 0; X + 1 < V; ++X)
			{
				const uint16 A = Y * V + X, B = A + 1, C = A + V, D = C + 1;
				// Render triangles (A, C, B) and (B, C, D), flipped as the engine cook flips them (bFlipNormals).
				Triangles.Add(Chaos::TVec3<uint16>(C, A, B));
				Triangles.Add(Chaos::TVec3<uint16>(C, B, D));
			}
		}
		TArray<uint16> Materials;
		G->Trimesh = Chaos::FTriangleMeshImplicitObjectPtr(new Chaos::FTriangleMeshImplicitObject(MoveTemp(Particles), MoveTemp(Triangles), MoveTemp(Materials)));
		G->Implicit = Chaos::FImplicitObjectPtr(G->Trimesh);
	}
	return G;
}

UGLTerrainCollisionComponent::UGLTerrainCollisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionProfileName(TEXT("BlockAll"));
	SetCanEverAffectNavigation(true);
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;
	SetGenerateOverlapEvents(false);
	bHiddenInGame = true;
	SetVisibility(false);
}

void UGLTerrainCollisionComponent::SetGeometry(TSharedPtr<const FGLChunkCollisionGeometry> InGeometry)
{
	Geometry = MoveTemp(InGeometry);
	UpdateBounds();
	if (IsRegistered())
	{
		RecreatePhysicsState();
		RefreshNavigationRelevance();
	}
}

void UGLTerrainCollisionComponent::RefreshNavigationRelevance()
{
	// Navigation caches relevance at registration, when a chunk's collision has no geometry yet.
	const bool bWas = bNavigationRelevant;
	bNavigationRelevant = IsNavigationRelevant();
	if (bWas != bNavigationRelevant)
	{
		bNavigationRelevant ? FNavigationSystem::OnComponentRegistered(*this) : FNavigationSystem::OnComponentUnregistered(*this);
	}
}

bool UGLTerrainCollisionComponent::HasBody() const
{
	return FPhysicsInterface::IsValid(BodyInstance.GetPhysicsActor());
}

double UGLTerrainCollisionComponent::GeometryHeightAtVertex(int32 X, int32 Y) const
{
	if (!Geometry)
	{
		return NAN;
	}
	const int32 V = Geometry->Verts;
	if (Geometry->Heightfield)
	{
		return Geometry->Heightfield->GetHeight(X, Y); // GetHeight(X = column, Y = row)
	}
	if (Geometry->Trimesh)
	{
		return Geometry->Trimesh->Particles().GetX(Y * V + X).Z;
	}
	return NAN;
}

bool UGLTerrainCollisionComponent::ShouldCreatePhysicsState() const
{
	return Geometry.IsValid() && IsRegistered() && GetWorld() && GetWorld()->GetPhysicsScene() && IsCollisionEnabled();
}

void UGLTerrainCollisionComponent::OnCreatePhysicsState()
{
	USceneComponent::OnCreatePhysicsState(); // skip UPrimitiveComponent's BodySetup path (as Landscape does)
	if (!Geometry || BodyInstance.IsValidBodyInstance())
	{
		return;
	}
	FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
	FActorCreationParams Params;
	Params.InitialTM = GetComponentTransform();
	Params.InitialTM.SetScale3D(FVector::OneVector);
	Params.bQueryOnly = false;
	Params.bStatic = true;
	Params.Scene = PhysScene;
	FPhysicsActorHandle Handle;
	FPhysicsInterface::CreateActor(Params, Handle);
	Chaos::FRigidBodyHandle_External& Body = Handle->GetGameThreadAPI();

	Chaos::FShapesArray Shapes;
	TUniquePtr<Chaos::FPerShapeData> Shape = Chaos::FShapeInstanceProxy::Make(0, Geometry->Implicit);
	FPhysicsFilterBuilder Builder;
	Builder.SetOwnerID(GetOwner() ? GetOwner()->GetUniqueID() : 0);
	Builder.SetComponentID(GetUniqueID());
	Builder.SetCollisionChannelIndex(GetCollisionObjectType());
	Builder.SetResponses(GetCollisionResponseToChannels());
	Builder.SetFlags(Chaos::EFilterFlags::SimpleCollision, true); // one geometry serves simple and complex queries
	Builder.SetFlags(Chaos::EFilterFlags::ComplexCollision, true);
	Builder.SetFlags(Chaos::EFilterFlags::StaticShape, true);
	Shape->SetShapeFilterData(Builder.BuildShapeFilterData());
	Shape->SetFilterInstanceData(Builder.BuildInstanceData());
	Shapes.Emplace(MoveTemp(Shape));
	Body.SetGeometry(Geometry->Implicit);
	for (TUniquePtr<Chaos::FPerShapeData>& Each : Shapes)
	{
		Each->UpdateShapeBounds(Chaos::FRigidTransform3(Body.X(), Body.R()));
	}
	Body.MergeShapesArray(MoveTemp(Shapes));

	BodyInstance.PhysicsUserData = FPhysicsUserData(&BodyInstance);
	BodyInstance.OwnerComponent = this;
	BodyInstance.SetPhysicsActor(Handle);
	Body.SetUserData(&BodyInstance.PhysicsUserData);

	FPhysicsCommand::ExecuteWrite(PhysScene, [&Handle, PhysScene]()
	{
		TArray<FPhysicsActorHandle> Actors = { Handle };
		PhysScene->AddActorsToScene_AssumesLocked(Actors, /*bImmediateAccelStructureInsertion*/ true);
	});
	PhysScene->AddToComponentMaps(this, Handle);
}

void UGLTerrainCollisionComponent::OnDestroyPhysicsState()
{
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			const FPhysicsActorHandle Handle = BodyInstance.GetPhysicsActor();
			if (FPhysicsInterface::IsValid(Handle))
			{
				PhysScene->RemoveFromComponentMaps(Handle);
			}
		}
	}
	Super::OnDestroyPhysicsState(); // terminates the body (releases the actor from the scene)
}

FBoxSphereBounds UGLTerrainCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!Geometry || !Geometry->LocalBounds.IsValid)
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0);
	}
	return FBoxSphereBounds(Geometry->LocalBounds.ExpandBy(1.0).TransformBy(LocalToWorld));
}

bool UGLTerrainCollisionComponent::IsNavigationRelevant() const
{
	return Geometry.IsValid() && Super::IsNavigationRelevant();
}

bool UGLTerrainCollisionComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	if (Geometry && Geometry->Heightfield)
	{
		GeomExport.ExportChaosHeightField(Geometry->Heightfield.GetReference(), Geometry->HeightfieldToLocal * GetComponentTransform());
	}
	else if (Geometry && Geometry->Trimesh)
	{
		GeomExport.ExportChaosTriMesh(Geometry->Trimesh.GetReference(), GetComponentTransform());
	}
	return false; // nothing else to export
}

void UGLTerrainCollisionComponent::OnActorEnableCollisionChanged()
{
	// Retiring chunks turn collision off at the actor: the body must go at once, not linger until cleared.
	if (IsRegistered())
	{
		RecreatePhysicsState();
		RefreshNavigationRelevance();
	}
}
