#include "Presentation/GLVisuals.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "GridlandsGame.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	FVector Vec(const TArray<double>& V, const FVector& Default = FVector::ZeroVector)
	{
		return V.Num() >= 3 ? FVector(V[0], V[1], V[2]) : Default;
	}

	UStaticMeshComponent* NewMeshComponent(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner);
		Component->SetStaticMesh(Mesh);
		Component->SetupAttachment(Parent);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetGenerateOverlapEvents(false);
		return Component;
	}
}

UStaticMesh* GLVisuals::LoadMesh(const FString& Name)
{
	return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), MeshesPath, *Name, *Name));
}

void GLVisuals::SetOutlined(UPrimitiveComponent* Component, bool bOutlined)
{
	if (Component)
	{
		Component->SetRenderCustomDepth(bOutlined);
		Component->SetCustomDepthStencilValue(bOutlined ? 1 : 0);
	}
}

UStaticMeshComponent* GLVisuals::Attach(AActor* Owner, USceneComponent* Parent, FName VisualId, const FTransform& Local)
{
	const FGLVisualDef* Def = VisualId.IsNone() ? nullptr : GLContent::Get().Find<FGLVisualDef>(VisualId);
	UStaticMesh* Mesh = Def ? LoadMesh(Def->Mesh) : nullptr;
	if (!Owner || !Parent || !Mesh)
	{
		if (Def)
		{
			UE_LOG(LogGridlands, Warning, TEXT("Visuals: %s: mesh %s is not imported"), *VisualId.ToString(), *Def->Mesh);
		}
		return nullptr;
	}
	UStaticMeshComponent* Main = NewMeshComponent(Owner, Parent, Mesh);
	const FTransform Placement(FRotator(0.0, Def->Yaw, 0.0), Vec(Def->Offset) * 100.0, FVector(Def->Scale));
	Main->SetRelativeTransform(Placement * Local);
	Main->SetCastShadow(Def->CastShadow);
	if (Def->Tint.Num() >= 3 && !(Def->Tint[0] == 1.0 && Def->Tint[1] == 1.0 && Def->Tint[2] == 1.0))
	{
		// A data-driven variant of the master material: one parameter, no new asset.
		for (int32 Slot = 0; Slot < Main->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Variant = Main->CreateDynamicMaterialInstance(Slot))
			{
				Variant->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Def->Tint[0], Def->Tint[1], Def->Tint[2]));
			}
		}
	}
	SetOutlined(Main, Def->Outline);
	Main->RegisterComponent();

	// NICE's corruption: engine cubes (mathematically precise) in the protected material, sparse by rule.
	if (Def->Corruption.Num() > 0)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UMaterialInterface* Corrupt = LoadObject<UMaterialInterface>(nullptr, CorruptionMaterialPath);
		for (const FGLCorruptionDef& C : Def->Corruption)
		{
			UStaticMeshComponent* Box = NewMeshComponent(Owner, Main, Cube);
			const FVector Rot = Vec(C.Rotation);
			// The engine cube is 1 m, centred: scale it to Size and put its centre at Offset (mesh space).
			Box->SetRelativeTransform(FTransform(FRotator(Rot.X, Rot.Y, Rot.Z), Vec(C.Offset) * 100.0 / FMath::Max(0.01, Def->Scale), FVector(C.Size / FMath::Max(0.01, Def->Scale))));
			Box->SetMaterial(0, Corrupt);
			Box->SetCastShadow(false);
			SetOutlined(Box, false); // precise, not illustrated: no graphic outline
			Box->RegisterComponent();
		}
	}
	if (Def->Light.Intensity > 0.0)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Owner);
		Light->SetupAttachment(Main);
		Light->SetRelativeLocation(Vec(Def->Light.Offset) * 100.0);
		Light->SetLightColor(FLinearColor(Vec(Def->Light.Color, FVector::OneVector)));
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(Def->Light.Intensity);
		Light->SetAttenuationRadius(Def->Light.Radius * 100.0);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
	}
	return Main;
}

int32 GLVisuals::CorruptionCount(FName VisualId)
{
	const FGLVisualDef* Def = GLContent::Get().Find<FGLVisualDef>(VisualId);
	return Def ? Def->Corruption.Num() : 0;
}
