#include "Storm/GLStormArtifact.h"

#include "Components/SceneComponent.h"
#include "Presentation/GLDerez.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FLinearColor Palette[] = {
		FLinearColor(1.f, 0.f, 1.f), FLinearColor(0.f, 1.f, 1.f), FLinearColor(0.2f, 1.f, 0.2f), FLinearColor(1.f, 1.f, 0.f),
	};
}

AGLStormArtifact::AGLStormArtifact()
{
	PrimaryActorTick.bCanEverTick = false; // the storm subsystem drives every artifact
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AGLStormArtifact::AddBlock(const FVector& Offset, const FVector& Size, const FLinearColor& Colour)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UStaticMeshComponent* Block = NewObject<UStaticMeshComponent>(this);
	Block->SetStaticMesh(Cube);
	Block->SetupAttachment(GetRootComponent());
	Block->SetRelativeLocation(Offset);
	Block->SetRelativeScale3D(Size / 100.0);
	Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Block->SetCanEverAffectNavigation(false);
	Block->SetCastShadow(false);
	Block->RegisterComponent();
	if (Shape)
	{
		if (UMaterialInstanceDynamic* Paint = Block->CreateDynamicMaterialInstance(0, Shape))
		{
			Paint->SetVectorParameterValue(TEXT("Color"), Colour);
		}
	}
	Blocks.Add(Block);
}

void AGLStormArtifact::Setup(FName InKind, int32 Seed, double InGroundZ)
{
	Kind = InKind;
	Random.Initialize(Seed);
	GroundZ = InGroundZ;
	FallSpeed = Random.FRandRange(300.0, 600.0);
	Spin = FRotator(Random.FRandRange(-180, 180), Random.FRandRange(-180, 180), Random.FRandRange(-180, 180));
	const FLinearColor Main = Palette[Random.RandRange(0, 3)];
	const FLinearColor Accent = Palette[Random.RandRange(0, 3)];
	const bool bDog = Kind == TEXT("dog");
	// A few voxels: body, head, ears or snout, tail. Deliberately low-res.
	AddBlock(FVector::ZeroVector, bDog ? FVector(60, 28, 28) : FVector(44, 22, 22), Main);
	AddBlock(FVector(bDog ? 38 : 28, 0, 16), bDog ? FVector(24, 22, 22) : FVector(20, 20, 20), Main);
	if (bDog)
	{
		AddBlock(FVector(54, 0, 12), FVector(12, 12, 10), Accent); // snout
	}
	else
	{
		AddBlock(FVector(30, -7, 30), FVector(6, 6, 10), Accent); // ears
		AddBlock(FVector(30, 7, 30), FVector(6, 6, 10), Accent);
	}
	AddBlock(FVector(bDog ? -36 : -28, 0, 12), FVector(18, 6, 6), Accent); // tail
}

bool AGLStormArtifact::Advance(float DeltaSeconds)
{
	if (!bLanded)
	{
		FallSpeed += 980.0 * DeltaSeconds;
		FVector At = GetActorLocation() - FVector(0, 0, FallSpeed * DeltaSeconds);
		// Pixel jitter: it snaps sideways by a few centimetres, like a broken sprite.
		At += FVector(Random.FRandRange(-4, 4), Random.FRandRange(-4, 4), 0);
		if (At.Z <= GroundZ + 20.0)
		{
			At.Z = GroundZ + 20.0;
			bLanded = true;
		}
		SetActorLocationAndRotation(At, GetActorRotation() + Spin * DeltaSeconds);
		return true;
	}
	DerezLeft -= DeltaSeconds;
	GLDerez::Apply(this, static_cast<float>(DerezLeft / 1.5), Random, FVector::OneVector); // the shared digital dissolve
	return DerezLeft > 0.0;
}
