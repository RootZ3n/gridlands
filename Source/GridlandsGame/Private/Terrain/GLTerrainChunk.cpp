#include "Terrain/GLTerrainChunk.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Materials/MaterialInterface.h"
#include "NavigationSystem.h"
#include "PhysicsEngine/BodySetup.h"
#include "Terrain/GLHeightfield.h"
#include "Terrain/GLTerrainCollision.h"
#include "UObject/ConstructorHelpers.h"

AGLTerrainChunk::AGLTerrainChunk()
{
	Mesh = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("Ground"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetComplexAsSimpleCollisionEnabled(true, false);
	// ApplyMesh/ClearForPool cook collision explicitly; SetMesh must not cook it again, synchronously.
	Mesh->SetDeferredCollisionUpdatesEnabled(true, false);
	Mesh->SetCanEverAffectNavigation(true);
	// Temporary ground material (P4): colours come from the vertices (see Rebuild). Falls back to
	// the engine grid if the generated material is missing.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ground(TEXT("/Game/Gridlands/Art/Materials/M_GLTerrain.M_GLTerrain"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grid(TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	if (Ground.Succeeded())
	{
		Mesh->SetMaterial(0, Ground.Object);
	}
	else if (Grid.Succeeded())
	{
		Mesh->SetMaterial(0, Grid.Object);
	}
}

void AGLTerrainChunk::Setup(FIntPoint InFirstVertex, int32 InVertsPerSide)
{
	FirstVertex = InFirstVertex;
	VertsPerSide = InVertsPerSide;
}

bool AGLTerrainChunk::Covers(const FIntRect& Dirty) const
{
	const int32 Last = VertsPerSide - 1;
	return Dirty.Min.X <= FirstVertex.X + Last && Dirty.Max.X >= FirstVertex.X && Dirty.Min.Y <= FirstVertex.Y + Last && Dirty.Max.Y >= FirstVertex.Y;
}

FGLChunkSnapshot AGLTerrainChunk::MakeSnapshot(const FGLHeightfield& Field, FIntPoint First, int32 Verts)
{
	FGLChunkSnapshot Snap;
	Snap.First = First;
	Snap.Verts = Verts;
	Snap.FieldVertsX = Field.GetVertsX();
	Snap.FieldVertsY = Field.GetVertsY();
	Snap.Spacing = Field.GetSpacing();
	const int32 Side = Verts + 2; // one vertex of margin for slopes at the chunk edge
	Snap.Heights.SetNumUninitialized(Side * Side);
	Snap.Base.SetNumUninitialized(Side * Side);
	for (int32 Y = 0; Y < Side; ++Y)
	{
		for (int32 X = 0; X < Side; ++X)
		{
			const int32 FX = FMath::Clamp(First.X + X - 1, 0, Snap.FieldVertsX - 1);
			const int32 FY = FMath::Clamp(First.Y + Y - 1, 0, Snap.FieldVertsY - 1);
			Snap.Heights[Y * Side + X] = Field.VertexHeight(FX, FY);
			Snap.Base[Y * Side + X] = Field.VertexBase(FY * Snap.FieldVertsX + FX);
		}
	}
	return Snap;
}

UE::Geometry::FDynamicMesh3 AGLTerrainChunk::BuildMesh(const FGLChunkSnapshot& Snap)
{
	using namespace UE::Geometry;
	const int32 V = Snap.Verts, Side = V + 2;
	auto H = [&Snap, Side](int32 FX, int32 FY) { return Snap.Heights[(FY - Snap.First.Y + 1) * Side + (FX - Snap.First.X + 1)]; };
	FDynamicMesh3 Built;
	for (int32 Y = 0; Y < V; ++Y)
	{
		for (int32 X = 0; X < V; ++X)
		{
			Built.AppendVertex(FVector3d(X * Snap.Spacing, Y * Snap.Spacing, H(Snap.First.X + X, Snap.First.Y + Y)));
		}
	}
	const bool bMainDiagonal = GLTerrainCollision::RenderSplitsMainDiagonal(); // heightfield spike only
	for (int32 Y = 0; Y + 1 < V; ++Y)
	{
		for (int32 X = 0; X + 1 < V; ++X)
		{
			const int32 A = Y * V + X, B = A + 1, C = A + V, D = C + 1;
			if (bMainDiagonal)
			{
				Built.AppendTriangle(A, C, D); // split A-D, as a Chaos heightfield splits its cells
				Built.AppendTriangle(A, D, B);
			}
			else
			{
				Built.AppendTriangle(A, C, B); // upward-facing in UE's left-handed space
				Built.AppendTriangle(B, C, D);
			}
		}
	}
	Built.EnableAttributes();
	FMeshNormals::QuickComputeVertexNormals(Built);
	FMeshNormals::InitializeOverlayToPerVertexNormals(Built.Attributes()->PrimaryNormals(), true);
	// Vertex colours are MASKS for the stylized ground material (P7, M_GLTerrain): R flatness,
	// G exposed earth where Zenny dug or raised, B per-vertex variation. The palette is the material's.
	Built.Attributes()->EnablePrimaryColors();
	FDynamicMeshColorOverlay* Colours = Built.Attributes()->PrimaryColors();
	TArray<int32> ColourOfVertex;
	ColourOfVertex.SetNum(Built.MaxVertexID());
	for (int32 Vid : Built.VertexIndicesItr())
	{
		const int32 X = Snap.First.X + Vid % V, Y = Snap.First.Y + Vid / V;
		const float Edited = FMath::Abs(Snap.Heights[(Y - Snap.First.Y + 1) * Side + (X - Snap.First.X + 1)] - Snap.Base[(Y - Snap.First.Y + 1) * Side + (X - Snap.First.X + 1)]);
		// Steepness from the heightfield itself (central differences, one-sided at the field edge).
		const int32 X0 = FMath::Max(0, X - 1), X1 = FMath::Min(Snap.FieldVertsX - 1, X + 1);
		const int32 Y0 = FMath::Max(0, Y - 1), Y1 = FMath::Min(Snap.FieldVertsY - 1, Y + 1);
		const double GX = (H(X1, Y) - H(X0, Y)) / ((X1 - X0) * Snap.Spacing);
		const double GY = (H(X, Y1) - H(X, Y0)) / ((Y1 - Y0) * Snap.Spacing);
		const float Up = static_cast<float>(1.0 / FMath::Sqrt(1.0 + GX * GX + GY * GY));
		const uint32 Hash = static_cast<uint32>(X) * 73856093u ^ static_cast<uint32>(Y) * 19349663u;
		const float Jitter = 0.94f + 0.12f * static_cast<float>(Hash % 1000) / 1000.f;
		const float Flat = FMath::SmoothStep(0.72f, 0.95f, Up);
		const float Exposed = FMath::Clamp(Edited / 8.f, 0.f, 1.f);
		ColourOfVertex[Vid] = Colours->AppendElement(FVector4f(Flat, Exposed, (Jitter - 0.94f) / 0.12f, 1.f));
	}
	for (int32 T : Built.TriangleIndicesItr())
	{
		const FIndex3i Tri = Built.GetTriangle(T);
		Colours->SetTriangle(T, FIndex3i(ColourOfVertex[Tri.A], ColourOfVertex[Tri.B], ColourOfVertex[Tri.C]));
	}
	return Built;
}

void AGLTerrainChunk::EnsureSeparateCollision()
{
	if (Collision)
	{
		return;
	}
	// SPIKE modes: the render mesh carries no collision and no navigation; one body does both.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Collision = NewObject<UGLTerrainCollisionComponent>(this, TEXT("GroundCollision"));
	Collision->SetupAttachment(Mesh);
	Collision->RegisterComponent();
}

void AGLTerrainChunk::ApplyMesh(UE::Geometry::FDynamicMesh3&& Built, bool bNotifyNavigation, TSharedPtr<const FGLChunkCollisionGeometry> Geometry)
{
	const EGLTerrainCollisionMode Mode = GLTerrainCollision::GetMode();
	if (Mode != EGLTerrainCollisionMode::ComponentTrimesh)
	{
		const double Start = FPlatformTime::Seconds();
		EnsureSeparateCollision();
		Mesh->SetMesh(MoveTemp(Built));
		++LifetimeMeshes;
		const double Meshed = FPlatformTime::Seconds();
		ensureMsgf(Geometry.IsValid(), TEXT("Terrain: a chunk mesh was applied without its collision geometry"));
		Collision->SetGeometry(MoveTemp(Geometry)); // the body is recreated now: no window without collision
		const double Collided = FPlatformTime::Seconds();
		if (bNotifyNavigation)
		{
			UNavigationSystemV1::UpdateComponentInNavOctree(*Collision);
		}
		MeshSeconds += Meshed - Start;
		CollisionSeconds += Collided - Meshed;
		NavigationSeconds += FPlatformTime::Seconds() - Collided;
		++Rebuilds;
		bBuilt = true;
		return;
	}
	const double Start = FPlatformTime::Seconds();
	Mesh->SetMesh(MoveTemp(Built));
	++LifetimeMeshes;
	const double Meshed = FPlatformTime::Seconds();
	// The one collision cook (collision updates are deferred, so SetMesh did not cook). Synchronous: in this
	// engine an async cook's completion costs the game thread more than the cook itself (~7.5 vs ~5 ms per
	// chunk, measured), and a synchronous cook leaves no window in which the new ground lacks collision.
	Mesh->bUseAsyncCooking = false; // never asynchronous (ADR-0034)
	Mesh->UpdateCollision(false);
	const double Collided = FPlatformTime::Seconds();
	if (bNotifyNavigation)
	{
		UNavigationSystemV1::UpdateComponentInNavOctree(*Mesh);
	}
	MeshSeconds += Meshed - Start;
	CollisionSeconds += Collided - Meshed;
	NavigationSeconds += FPlatformTime::Seconds() - Collided;
	++Rebuilds;
	bBuilt = true;
}

void AGLTerrainChunk::Rebuild(const FGLHeightfield& Field, bool bNotifyNavigation)
{
	ApplySnapshot(MakeSnapshot(Field, FirstVertex, VertsPerSide), bNotifyNavigation);
}

void AGLTerrainChunk::ApplySnapshot(const FGLChunkSnapshot& Snapshot, bool bNotifyNavigation)
{
	double Start = FPlatformTime::Seconds();
	UE::Geometry::FDynamicMesh3 Built = BuildMesh(Snapshot);
	MeshSeconds += FPlatformTime::Seconds() - Start;
	Start = FPlatformTime::Seconds();
	TSharedPtr<const FGLChunkCollisionGeometry> Geometry = GLTerrainCollision::Build(GLTerrainCollision::GetMode(), Snapshot);
	const double Built_ = FPlatformTime::Seconds() - Start;
	CollisionBuildGameThreadSeconds += Built_;
	CollisionSeconds += Built_; // on the game thread here (edits, synchronous loads)
	ApplyMesh(MoveTemp(Built), bNotifyNavigation, MoveTemp(Geometry));
}

bool AGLTerrainChunk::IsEmptyAndInert() const
{
	const UBodySetup* Body = Mesh->GetBodySetup();
	const bool bNoBody = !Body || (Body->TriMeshGeometries.Num() == 0 && Body->AggGeom.GetElementCount() == 0);
	const bool bNoSeparateBody = !Collision || (!Collision->HasGeometry() && !Collision->HasBody());
	return Mesh->GetMesh()->TriangleCount() == 0 && bNoBody && bNoSeparateBody && IsHidden() && !GetActorEnableCollision();
}

void AGLTerrainChunk::ClearForPool()
{
	Mesh->bUseAsyncCooking = false;
	Mesh->SetMesh(UE::Geometry::FDynamicMesh3());
	Mesh->UpdateCollision(false); // synchronous, and it drops any async cook still in flight
	UNavigationSystemV1::UpdateComponentInNavOctree(*Mesh);
	if (Collision)
	{
		Collision->SetGeometry(nullptr); // the body goes now
		UNavigationSystemV1::UpdateComponentInNavOctree(*Collision);
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	bBuilt = false;
	OwnerCell = NAME_None;
	State = EGLChunkState::Pooled;
}

