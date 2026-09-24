#include "Commandlets/GLBuildBlockoutCommandlet.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "GridlandsEditor.h"
#include "Interaction/GLDebugInteractable.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "World/GLAnchorComponent.h"
#include "World/GLGameMode.h"

namespace GLBlockout
{
	const TCHAR* MapPackage = TEXT("/Game/Gridlands/Maps/L_Origin");
	constexpr double M = 100.0; // cm per metre

	struct FBuilder
	{
		UWorld* World;
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		UMaterialInterface* Grid = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));

		void Anchor(AActor* Actor, const TCHAR* Name)
		{
			UGLAnchorComponent* Anchor = NewObject<UGLAnchorComponent>(Actor, TEXT("Anchor"));
			Anchor->AnchorId = FName(FString::Printf(TEXT("anchor.origin.%s"), Name));
			Actor->AddInstanceComponent(Anchor);
			Anchor->RegisterComponent();
		}

		/** A mesh sized in metres, positioned by its centre (cm). The basic shapes are 1 m across. */
		AStaticMeshActor* Shape(UStaticMesh* Mesh, const TCHAR* Label, const FVector& CentreCm, const FVector& SizeM, double Yaw = 0.0, UMaterialInterface* Material = nullptr)
		{
			AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(CentreCm, FRotator(0.0, Yaw, 0.0));
			Actor->SetActorLabel(Label);
			Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
			Actor->SetActorScale3D(SizeM);
			if (Material)
			{
				Actor->GetStaticMeshComponent()->SetMaterial(0, Material);
			}
			return Actor;
		}

		AStaticMeshActor* Box(const TCHAR* Label, const FVector& CentreCm, const FVector& SizeM, double Yaw = 0.0, UMaterialInterface* Material = nullptr)
		{
			return Shape(Cube, Label, CentreCm, SizeM, Yaw, Material);
		}

		/** A modern two-storey house: body + flat roof slab, anchored on the body. */
		void House(int32 Index, const FVector& FootCm, double Yaw)
		{
			const FString Name = FString::Printf(TEXT("house_%02d"), Index);
			AStaticMeshActor* Body = Box(*FString::Printf(TEXT("House_%02d"), Index), FootCm + FVector(0, 0, 3.0 * M), FVector(12, 10, 6), Yaw);
			Anchor(Body, *Name);
			Box(*FString::Printf(TEXT("House_%02d_Roof"), Index), FootCm + FVector(0, 0, 6.2 * M), FVector(13, 11, 0.4), Yaw);
			Box(*FString::Printf(TEXT("House_%02d_Garage"), Index), FootCm + FVector(8.5 * M, 0, 1.5 * M).RotateAngleAxis(Yaw, FVector::UpVector), FVector(5, 6, 3), Yaw);
		}
	};
}

int32 UGLBuildBlockoutCommandlet::Main(const FString& Params)
{
	using namespace GLBlockout;

	UPackage* Package = CreatePackage(MapPackage);
	Package->FullyLoad();
	UWorld* World = UWorld::CreateWorld(EWorldType::Inactive, false, TEXT("L_Origin"), Package);
	World->SetFlags(RF_Public | RF_Standalone);
	FBuilder B{ World };
	if (!B.Cube || !B.Cylinder || !B.Grid)
	{
		UE_LOG(LogGridlandsEditor, Error, TEXT("BuildBlockout: engine basic shapes or materials missing"));
		return 1;
	}

	// Ground: 250 m x 250 m, top face at z = 0.
	B.Box(TEXT("Ground"), FVector(0, 0, -0.5 * M), FVector(250, 250, 1), 0.0, B.Grid);
	// Streets: a main street along X and a cross street along Y.
	B.Box(TEXT("Street_Main"), FVector(0, 0, 0.02 * M), FVector(250, 10, 0.04));
	B.Box(TEXT("Street_Cross"), FVector(60 * M, 0, 0.02 * M), FVector(10, 250, 0.04));

	// Modern suburbia: eight houses facing the main street.
	const double RowY = 25.0 * M;
	const double Xs[] = { -100 * M, -70 * M, -40 * M, -10 * M };
	int32 Index = 1;
	for (const double X : Xs)
	{
		B.House(Index++, FVector(X, -RowY, 0), 0.0);
		B.House(Index++, FVector(X, RowY, 0), 180.0);
	}
	// Backyard fences: future gameplay salvage anchors.
	for (int32 Fence = 1; Fence <= 4; ++Fence)
	{
		AStaticMeshActor* Panel = B.Box(*FString::Printf(TEXT("Fence_%02d"), Fence), FVector(Xs[Fence - 1], -RowY - 9.0 * M, 0.9 * M), FVector(10, 0.15, 1.8));
		B.Anchor(Panel, *FString::Printf(TEXT("fence_%02d"), Fence));
	}

	// The conspicuous 1950s fragment: a diner that should not be here.
	{
		const FVector Lot(95 * M, -28 * M, 0);
		AStaticMeshActor* Diner = B.Box(TEXT("Fifties_Diner"), Lot + FVector(0, 0, 2.25 * M), FVector(20, 8, 4.5));
		B.Anchor(Diner, TEXT("fragment_fifties_diner"));
		B.Shape(B.Cylinder, TEXT("Fifties_Diner_EndCap_A"), Lot + FVector(-10 * M, 0, 2.25 * M), FVector(8, 8, 4.5));
		B.Shape(B.Cylinder, TEXT("Fifties_Diner_EndCap_B"), Lot + FVector(10 * M, 0, 2.25 * M), FVector(8, 8, 4.5));
		B.Shape(B.Cylinder, TEXT("Fifties_Diner_SignPole"), Lot + FVector(0, 7 * M, 5 * M), FVector(0.4, 0.4, 10));
		B.Box(TEXT("Fifties_Diner_Sign"), Lot + FVector(0, 7 * M, 10 * M), FVector(6, 0.4, 2.5));
	}
	// The smaller Roman fragment: a colonnade stub in a backyard.
	{
		const FVector Yard(-100 * M, 45 * M, 0);
		AStaticMeshActor* Architrave = B.Box(TEXT("Roman_Architrave"), Yard + FVector(0, 0, 6.3 * M), FVector(11, 1.6, 0.6));
		B.Anchor(Architrave, TEXT("fragment_roman_columns"));
		for (int32 Column = 0; Column < 4; ++Column)
		{
			B.Shape(B.Cylinder, *FString::Printf(TEXT("Roman_Column_%d"), Column + 1), Yard + FVector((-4.5 + Column * 3.0) * M, 0, 3.0 * M), FVector(0.8, 0.8, 6));
		}
	}
	// The storm-drain entrance: a culvert mouth at the cross street's edge.
	{
		AStaticMeshActor* Culvert = B.Box(TEXT("StormDrain_Culvert"), FVector(60 * M, 70 * M, 0.5 * M), FVector(3, 4, 2), 90.0);
		B.Anchor(Culvert, TEXT("storm_drain_entrance"));
		B.Box(TEXT("StormDrain_Grate"), FVector(60 * M, 67.8 * M, 0.02 * M), FVector(2.4, 0.4, 0.05));
	}

	// Two debug interactables near the start, for manual interaction checks.
	World->SpawnActor<AGLDebugInteractable>(FVector(3 * M, -6 * M, 0.5 * M), FRotator::ZeroRotator)->SetActorLabel(TEXT("Debug_Interactable_A"));
	World->SpawnActor<AGLDebugInteractable>(FVector(-3 * M, -6 * M, 0.5 * M), FRotator::ZeroRotator)->SetActorLabel(TEXT("Debug_Interactable_B"));

	World->SpawnActor<APlayerStart>(FVector(0, -12 * M, 1.0 * M), FRotator(0.0, 90.0, 0.0));

	// Movable lights: no lighting build needed for a blockout.
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 50 * M), FRotator(-45.0, 30.0, 0.0));
	Sun->GetComponent()->SetMobility(EComponentMobility::Movable);
	Sun->GetComponent()->SetAtmosphereSunLight(true);
	ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0, 0, 40 * M), FRotator::ZeroRotator);
	Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sky->GetLightComponent()->bRealTimeCapture = true;
	World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator);

	World->GetWorldSettings()->DefaultGameMode = AGLGameMode::StaticClass();

	const FString Filename = FPackageName::LongPackageNameToFilename(MapPackage, FPackageName::GetMapPackageExtension());
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const bool bSaved = UPackage::SavePackage(Package, World, *Filename, Args);
	UE_LOG(LogGridlandsEditor, Display, TEXT("BuildBlockout: %s %s"), bSaved ? TEXT("saved") : TEXT("FAILED to save"), *Filename);
	return bSaved ? 0 : 1;
}
