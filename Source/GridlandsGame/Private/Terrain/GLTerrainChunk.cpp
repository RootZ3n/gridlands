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
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grid(TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	if (Grid.Succeeded())
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
	Mesh->SetMesh(MoveTemp(Built));
	Mesh->UpdateCollision(false); // synchronous: a trace right after an edit sees the new ground
	if (bNotifyNavigation)
	{
		UNavigationSystemV1::UpdateComponentInNavOctree(*Mesh);
	}
}
