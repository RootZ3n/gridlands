#include "Presentation/GLDerez.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

void GLDerez::Apply(AActor* Target, float Alpha, FRandomStream& Random, const FVector& BaseScale)
{
	if (!Target)
	{
		return;
	}
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	// Flicker: more frames missing as it goes; thin on a random axis like a broken sprite.
	const bool bVisible = A > 0.f && Random.FRand() < 0.35f + 0.65f * A;
	Target->ForEachComponent<UPrimitiveComponent>(false, [bVisible](UPrimitiveComponent* P) { P->SetVisibility(bVisible, false); });
	const float Thin = Random.FRand() < 0.5f ? 1.f : FMath::Lerp(0.15f, 1.f, A);
	Target->SetActorScale3D(BaseScale * FVector(FMath::Lerp(0.2f, 1.f, A), Thin * FMath::Lerp(0.2f, 1.f, A), FMath::Lerp(0.6f, 1.f, A)));
}

UGLDerezComponent::UGLDerezComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UGLDerezComponent::Start(float Seconds)
{
	Duration = FMath::Max(0.1f, Seconds);
	Left = Duration;
	BaseScale = GetOwner()->GetActorScale3D();
	SetComponentTickEnabled(true);
}

void UGLDerezComponent::Shed()
{
	// A few pixels drift up off it (no collision; they clean themselves up).
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UWorld* World = GetWorld();
	if (!Cube || !World)
	{
		return;
	}
	const FVector At = GetOwner()->GetActorLocation() + FVector(Random.FRandRange(-40, 40), Random.FRandRange(-40, 40), Random.FRandRange(0, 60));
	AStaticMeshActor* Pixel = World->SpawnActor<AStaticMeshActor>(At, FRotator::ZeroRotator);
	if (!Pixel)
	{
		return;
	}
	Pixel->SetMobility(EComponentMobility::Movable);
	Pixel->GetStaticMeshComponent()->SetStaticMesh(Cube);
	Pixel->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pixel->GetStaticMeshComponent()->SetCastShadow(false);
	Pixel->SetActorScale3D(FVector(0.08f));
	Pixel->SetLifeSpan(0.8f);
	if (UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* Paint = Pixel->GetStaticMeshComponent()->CreateDynamicMaterialInstance(0, Shape))
		{
			Paint->SetVectorParameterValue(TEXT("Color"), Random.FRand() < 0.5f ? FLinearColor(1.f, 0.f, 1.f) : FLinearColor(0.f, 1.f, 1.f));
		}
	}
	Pixels.Add(Pixel);
}

bool UGLDerezComponent::Advance(float DeltaSeconds)
{
	if (Left <= 0.f)
	{
		return false;
	}
	Left = FMath::Max(0.f, Left - DeltaSeconds);
	ShedTimer -= DeltaSeconds;
	if (ShedTimer <= 0.f)
	{
		ShedTimer = 0.08f;
		Shed();
	}
	for (AActor* Pixel : Pixels)
	{
		if (Pixel && IsValid(Pixel))
		{
			Pixel->AddActorWorldOffset(FVector(0, 0, 120.0 * DeltaSeconds));
		}
	}
	GLDerez::Apply(GetOwner(), Left / Duration, Random, BaseScale);
	if (Left <= 0.f)
	{
		GetOwner()->SetActorHiddenInGame(true);
		SetComponentTickEnabled(false);
		return false;
	}
	return true;
}

void UGLDerezComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Advance(DeltaTime);
}
