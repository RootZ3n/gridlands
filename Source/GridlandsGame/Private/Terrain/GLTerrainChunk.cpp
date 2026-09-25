#include "Terrain/GLTerrainChunk.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "Materials/MaterialInterface.h"
#include "NavigationSystem.h"
#include "Terrain/GLHeightfield.h"
#include "UObject/ConstructorHelpers.h"

AGLTerrainChunk::AGLTerrainChunk()
{
	Mesh = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("Ground"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetComplexAsSimpleCollisionEnabled(true, false);
	Mesh->SetCanEverAffectNavigation(true);
	// Temporary ground material (P4): colours come from the vertices (see Rebuild). Falls back to
	// the engine grid if the generated material is missing.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ground(TEXT("/Game/Gridlands/Materials/M_TerrainVertexColor.M_TerrainVertexColor"));
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

void AGLTerrainChunk::Rebuild(const FGLHeightfield& Field, bool bNotifyNavigation)
{
	using namespace UE::Geometry;
	const double Start = FPlatformTime::Seconds();
	FDynamicMesh3 Built;
	const FVector2D ChunkOrigin = Field.VertexLocation(FirstVertex.X, FirstVertex.Y);
	for (int32 Y = 0; Y < VertsPerSide; ++Y)
	{
		for (int32 X = 0; X < VertsPerSide; ++X)
		{
			const FVector2D At = Field.VertexLocation(FirstVertex.X + X, FirstVertex.Y + Y) - ChunkOrigin;
			Built.AppendVertex(FVector3d(At.X, At.Y, Field.VertexHeight(FirstVertex.X + X, FirstVertex.Y + Y)));
		}
	}
	for (int32 Y = 0; Y + 1 < VertsPerSide; ++Y)
	{
		for (int32 X = 0; X + 1 < VertsPerSide; ++X)
		{
			const int32 A = Y * VertsPerSide + X, B = A + 1, C = A + VertsPerSide, D = C + 1;
			Built.AppendTriangle(A, C, B); // upward-facing in UE's left-handed space
			Built.AppendTriangle(B, C, D);
		}
	}
	Built.EnableAttributes();
	FMeshNormals::QuickComputeVertexNormals(Built);
	FMeshNormals::InitializeOverlayToPerVertexNormals(Built.Attributes()->PrimaryNormals(), true);
	// Vertex colours for the temporary material: lawn on the flat, rock where it is steep, dirt
	// where the ground was dug or raised (so edits read at a glance), with a little variation.
	Built.Attributes()->EnablePrimaryColors();
	FDynamicMeshColorOverlay* Colours = Built.Attributes()->PrimaryColors();
	TArray<int32> ColourOfVertex;
	ColourOfVertex.SetNum(Built.MaxVertexID());
	for (int32 V : Built.VertexIndicesItr())
	{
		const int32 X = FirstVertex.X + V % VertsPerSide, Y = FirstVertex.Y + V / VertsPerSide;
		const int32 Index = Y * Field.GetVertsX() + X;
		const float Edited = FMath::Abs(Field.VertexHeight(X, Y) - Field.VertexBase(Index));
		// Steepness from the heightfield itself (central differences).
		const int32 X0 = FMath::Max(0, X - 1), X1 = FMath::Min(Field.GetVertsX() - 1, X + 1);
		const int32 Y0 = FMath::Max(0, Y - 1), Y1 = FMath::Min(Field.GetVertsY() - 1, Y + 1);
		const double GX = (Field.VertexHeight(X1, Y) - Field.VertexHeight(X0, Y)) / ((X1 - X0) * Field.GetSpacing());
		const double GY = (Field.VertexHeight(X, Y1) - Field.VertexHeight(X, Y0)) / ((Y1 - Y0) * Field.GetSpacing());
		const float Up = static_cast<float>(1.0 / FMath::Sqrt(1.0 + GX * GX + GY * GY));
		const uint32 Hash = static_cast<uint32>(X) * 73856093u ^ static_cast<uint32>(Y) * 19349663u;
		const float Jitter = 0.94f + 0.12f * static_cast<float>(Hash % 1000) / 1000.f;
		const FLinearColor Lawn(0.20f, 0.30f, 0.12f), Rock(0.36f, 0.34f, 0.31f), Dirt(0.34f, 0.24f, 0.15f);
		FLinearColor C = FMath::Lerp(Rock, Lawn, FMath::SmoothStep(0.72f, 0.9f, Up));
		C = FMath::Lerp(C, Dirt, FMath::Clamp(Edited / 25.f, 0.f, 1.f));
		ColourOfVertex[V] = Colours->AppendElement(FVector4f(C.R * Jitter, C.G * Jitter, C.B * Jitter, 1.f));
	}
	for (int32 T : Built.TriangleIndicesItr())
	{
		const UE::Geometry::FIndex3i Tri = Built.GetTriangle(T);
		Colours->SetTriangle(T, UE::Geometry::FIndex3i(ColourOfVertex[Tri.A], ColourOfVertex[Tri.B], ColourOfVertex[Tri.C]));
	}
	Mesh->SetMesh(MoveTemp(Built));
	const double Meshed = FPlatformTime::Seconds();
	Mesh->UpdateCollision(false); // synchronous: a trace right after an edit sees the new ground
	const double Collided = FPlatformTime::Seconds();
	if (bNotifyNavigation)
	{
		UNavigationSystemV1::UpdateComponentInNavOctree(*Mesh);
	}
	MeshSeconds += Meshed - Start;
	CollisionSeconds += Collided - Meshed;
	NavigationSeconds += FPlatformTime::Seconds() - Collided;
	++Rebuilds;
}
