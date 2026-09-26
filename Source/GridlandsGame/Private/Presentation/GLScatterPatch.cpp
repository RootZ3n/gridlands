#include "Presentation/GLScatterPatch.h"

#include "Building/GLBuildingSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Presentation/GLVisuals.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"

namespace
{
	/** Ground moved further than this reads as bare earth: nothing grows there. */
	constexpr double ExposedCm = 5.0;
}

AGLScatterPatch::AGLScatterPatch()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Instances"));
	Instances->SetupAttachment(GetRootComponent());
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetCastShadow(false);
}

bool AGLScatterPatch::Setup(FName InPlacementId, FName InVisual, double InRadiusCm, int32 InCount)
{
	const FGLVisualDef* Def = GLContent::Get().Find<FGLVisualDef>(InVisual);
	UStaticMesh* Mesh = Def ? GLVisuals::LoadMesh(Def->Mesh) : nullptr;
	if (!Mesh)
	{
		return false;
	}
	PlacementId = InPlacementId;
	Visual = InVisual;
	RadiusCm = InRadiusCm;
	Count = InCount;
	Instances->SetStaticMesh(Mesh);
	Instances->SetCastShadow(Def->CastShadow);
	GLVisuals::SetOutlined(Instances, Def->Outline);
	if (UGLEventSubsystem* Bus = GetWorld()->GetSubsystem<UGLEventSubsystem>())
	{
		for (const TCHAR* Tag : { TEXT("Event.Terrain.Edited"), TEXT("Event.Structure.Collapsed"), TEXT("Event.Building.Placed"), TEXT("Event.Building.Demolished") })
		{
			Subscriptions.Add(Bus->Subscribe(UGameplayTagsManager::Get().RequestGameplayTag(Tag), FGLGameplayEventDelegate::CreateUObject(this, &AGLScatterPatch::HandleChange)));
		}
	}
	Rebuild();
	return true;
}

void AGLScatterPatch::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UGLEventSubsystem* Bus = GetWorld() ? GetWorld()->GetSubsystem<UGLEventSubsystem>() : nullptr)
	{
		for (FDelegateHandle& Handle : Subscriptions)
		{
			Bus->Unsubscribe(Handle);
		}
	}
	Super::EndPlay(Reason);
}

void AGLScatterPatch::HandleChange(const FGLGameplayEvent& Event)
{
	// Only changes near the patch matter (events carry their instigator; the patch is small).
	const AActor* By = Event.Instigator.Get();
	if (!By || FVector::Dist2D(By->GetActorLocation(), GetActorLocation()) < RadiusCm + 3000.0)
	{
		Rebuild();
	}
}

void AGLScatterPatch::Rebuild()
{
	const FGLVisualDef* Def = GLContent::Get().Find<FGLVisualDef>(Visual);
	UWorld* World = GetWorld();
	const UGLTerrainSubsystem* Terrain = World ? World->GetSubsystem<UGLTerrainSubsystem>() : nullptr;
	if (!Def || !Terrain)
	{
		return;
	}
	const UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
	const UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>();
	Instances->ClearInstances();
	FRandomStream Random(static_cast<int32>(GetTypeHash(PlacementId)));
	const FVector2D Centre(GetActorLocation());
	TArray<FTransform> Planted;
	Planted.Reserve(Count);
	for (int32 I = 0; I < Count; ++I)
	{
		// Every candidate draws the same numbers whether or not it is planted: stable under edits.
		const double Angle = Random.FRandRange(0.0, UE_TWO_PI);
		const double Distance = RadiusCm * FMath::Sqrt(Random.FRand());
		const double Yaw = Random.FRandRange(0.0, 360.0);
		const double Scale = Def->Scale * Random.FRandRange(0.75, 1.3);
		const FVector2D At = Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
		if (Terrain->EditedAt(At) > ExposedCm || (Structures && Structures->IsUnderStructure(At, 10.0)) || (Building && Building->IsUnderStructure(At)))
		{
			continue;
		}
		const FVector World3(At, Terrain->HeightAt(At));
		Planted.Add(FTransform(FRotator(0.0, Yaw, 0.0), World3 - GetActorLocation(), FVector(Scale)));
	}
	Instances->AddInstances(Planted, false);
}

int32 AGLScatterPatch::GetInstanceCount() const
{
	return Instances->GetInstanceCount();
}
