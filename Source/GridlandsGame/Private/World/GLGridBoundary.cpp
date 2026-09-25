#include "World/GLGridBoundary.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AGLGridBoundary::AGLGridBoundary()
{
	Posts = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Posts"));
	SetRootComponent(Posts);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Posts->SetStaticMesh(Cube.Object);
	}
	Posts->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Posts->SetCanEverAffectNavigation(false);
	Posts->SetCastShadow(false);
}

void AGLGridBoundary::Setup(const FVector2D& CentreCm, double SizeMetres)
{
	SetActorLocation(FVector(CentreCm, 0.0));
	const double Half = SizeMetres * 50.0;
	const double Height = 600.0;
	for (int32 Edge = 0; Edge < 4; ++Edge)
	{
		for (double T = -Half; T < Half; T += 400.0)
		{
			const FVector2D At = Edge == 0 ? FVector2D(T, -Half) : Edge == 1 ? FVector2D(Half, T) : Edge == 2 ? FVector2D(-T, Half) : FVector2D(-Half, -T);
			Posts->AddInstance(FTransform(FRotator::ZeroRotator, FVector(At, Height * 0.5 - 200.0), FVector(0.15, 0.15, Height / 100.0)));
		}
		// A rail along the top of each edge.
		const FVector2D Mid = Edge == 0 ? FVector2D(0, -Half) : Edge == 1 ? FVector2D(Half, 0) : Edge == 2 ? FVector2D(0, Half) : FVector2D(-Half, 0);
		const FVector Scale = (Edge % 2 == 0) ? FVector(SizeMetres, 0.1, 0.1) : FVector(0.1, SizeMetres, 0.1);
		Posts->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Mid, Height - 200.0), Scale));
	}
	if (UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* Paint = Posts->CreateDynamicMaterialInstance(0, Shape))
		{
			Paint->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.f, 1.f, 1.f));
		}
	}
}
