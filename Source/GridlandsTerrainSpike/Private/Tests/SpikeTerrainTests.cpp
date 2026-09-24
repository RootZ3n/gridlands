// SPIKE S1 (throwaway): executable evidence for the terrain ADR. Writes Saved/TerrainSpike/metrics.json.

#include "SpikeTerrain.h"

#include "Components/DynamicMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "Components/BrushComponent.h"
#include "Builders/CubeBuilder.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SpikeTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	TSharedRef<FJsonObject>& Metrics()
	{
		static TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		return Object;
	}

	void SaveMetrics()
	{
		FString Text;
		FJsonSerializer::Serialize(Metrics(), TJsonWriterFactory<>::Create(&Text));
		FFileHelper::SaveStringToFile(Text, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TerrainSpike/metrics.json")));
	}

	template <typename F>
	double MillisecondsPerRun(int32 Runs, F&& Body)
	{
		const double Start = FPlatformTime::Seconds();
		for (int32 I = 0; I < Runs; ++I)
		{
			Body();
		}
		return (FPlatformTime::Seconds() - Start) * 1000.0 / Runs;
	}

	struct FTestWorld
	{
		UWorld* World = nullptr;
		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TerrainSpikeWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	UDynamicMeshComponent* SpawnTerrain(UWorld* World, UE::Geometry::FDynamicMesh3&& Mesh)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UDynamicMeshComponent* Component = NewObject<UDynamicMeshComponent>(Actor);
		Actor->SetRootComponent(Component);
		Component->SetCollisionProfileName(TEXT("BlockAll"));
		Component->RegisterComponent();
		Component->SetMesh(MoveTemp(Mesh));
		Component->SetComplexAsSimpleCollisionEnabled(true, true);
		return Component;
	}

	// Spike-only: a real level gets its bounds from an editor-placed NavMeshBoundsVolume.
	struct FNavBoundsAccess : UNavigationSystemV1
	{
		using UNavigationSystemV1::SpawnMissingNavigationData;
	};

	/** Navigation builds tiles asynchronously; tick until done (bounded). Returns ticks taken, -1 on timeout. */
	int32 WaitForNavigation(UNavigationSystemV1* Nav)
	{
		for (int32 Tick = 0; Tick < 600; ++Tick)
		{
			Nav->Tick(0.05f);
			if (!Nav->IsNavigationBuildInProgress())
			{
				return Tick;
			}
			FPlatformProcess::Sleep(0.005f);
		}
		return -1;
	}

	bool Trace(UWorld* World, const FVector& From, const FVector& To, FHitResult& Hit)
	{
		return World->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic);
	}
}

using namespace SpikeTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpikeTerrainPure, "Gridlands.Spike.Terrain.PureOperations", Flags)
bool FSpikeTerrainPure::RunTest(const FString& Parameters)
{
	FSpikeHeightfield Base;
	Base.Init(65, 100.0, 0.f);
	FSpikeHeightfield Dug = Base;
	Dug.ApplyBrush(FVector2D(3200, 3200), 300.0, -150.0);
	TestTrue(TEXT("dig lowers the centre by the brush depth"), FMath::IsNearlyEqual(Dug.HeightAt(FVector2D(3200, 3200)), -150.0, 0.5));
	TestEqual(TEXT("dig leaves far ground alone"), Dug.HeightAt(FVector2D(500, 500)), 0.0);
	Dug.ApplyBrush(FVector2D(1000, 1000), 400.0, 200.0);
	TestTrue(TEXT("raise builds a mound"), Dug.HeightAt(FVector2D(1000, 1000)) > 199.0);
	Dug.Flatten(FVector2D(1000, 1000), 200.0, 50.f);
	TestTrue(TEXT("flatten pulls the foundation toward target"), FMath::IsNearlyEqual(Dug.HeightAt(FVector2D(1000, 1000)), 50.0, 1.0));

	FSpikeHeightfield Restored = Base;
	TestTrue(TEXT("delta applies"), Restored.ApplyDelta(Dug.EncodeDelta(Base)));
	double MaxError = 0.0;
	for (int32 I = 0; I < Dug.Heights.Num(); ++I)
	{
		MaxError = FMath::Max(MaxError, FMath::Abs(double(Dug.Heights[I] - Restored.Heights[I])));
	}
	TestTrue(TEXT("save delta reproduces terrain within 1 cm"), MaxError <= 1.0);
	TestTrue(TEXT("deterministic: same ops, same bytes"), [&]
	{
		FSpikeHeightfield A = Base, B = Base;
		A.ApplyBrush(FVector2D(3200, 3200), 300.0, -150.0);
		B.ApplyBrush(FVector2D(3200, 3200), 300.0, -150.0);
		return A.EncodeDelta(Base) == B.EncodeDelta(Base);
	}());

	FSpikeVoxels Voxels;
	Voxels.InitGround(33, 100.0, 1600.0);
	const int32 Before = Voxels.BuildSurfaceNets().TriangleCount();
	TestTrue(TEXT("voxel ground has a surface"), Before > 0);
	Voxels.ApplySphere(FVector(1600, 1600, 1000), 350.0, false); // cavity fully underground
	TestTrue(TEXT("an underground cavity adds interior surface (heightfields cannot)"), Voxels.BuildSurfaceNets().TriangleCount() > Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpikeTerrainMeasure, "Gridlands.Spike.Terrain.Measurements", Flags)
bool FSpikeTerrainMeasure::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> M = Metrics();
	for (const TPair<int32, double>& Variant : { TPair<int32, double>(65, 100.0), TPair<int32, double>(129, 50.0) })
	{
		FSpikeHeightfield Base;
		Base.Init(Variant.Key, Variant.Value, 0.f);
		FSpikeHeightfield Edited = Base;
		const double Brush = MillisecondsPerRun(50, [&] { Edited = Base; Edited.ApplyBrush(FVector2D(3200, 3200), 300.0, -150.0); });
		int32 Triangles = 0;
		const double Build = MillisecondsPerRun(20, [&] { Triangles = Edited.BuildMesh().TriangleCount(); });
		const TArray<uint8> Delta = Edited.EncodeDelta(Base);
		const FString Key = FString::Printf(TEXT("heightfield_%dx%d_at_%dcm"), Variant.Key, Variant.Key, int32(Variant.Value));
		TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("chunkMetres"), (Variant.Key - 1) * Variant.Value / 100.0);
		Row->SetNumberField(TEXT("triangles"), Triangles);
		Row->SetNumberField(TEXT("brushMs"), Brush);
		Row->SetNumberField(TEXT("meshBuildMs"), Build);
		Row->SetNumberField(TEXT("digDeltaBytes"), Delta.Num());
		Row->SetNumberField(TEXT("digDeltaZlibBytes"), SpikeTerrain::CompressedSize(Delta));
		Row->SetNumberField(TEXT("fullChunkBytesUncompressed"), Base.Heights.Num() * 2);
		M->SetObjectField(Key, Row);
		AddInfo(FString::Printf(TEXT("%s: %d tris, brush %.3f ms, build %.3f ms, dig delta %d B (%d B zlib)"),
			*Key, Triangles, Brush, Build, Delta.Num(), SpikeTerrain::CompressedSize(Delta)));
	}
	{
		FSpikeVoxels Base;
		Base.InitGround(33, 100.0, 1600.0);
		FSpikeVoxels Edited = Base;
		const double Brush = MillisecondsPerRun(20, [&] { Edited = Base; Edited.ApplySphere(FVector(1600, 1600, 1600), 300.0, false); });
		int32 Triangles = 0;
		const double Build = MillisecondsPerRun(10, [&] { Triangles = Edited.BuildSurfaceNets().TriangleCount(); });
		const TArray<uint8> Delta = Edited.EncodeDelta(Base);
		TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("chunkMetres"), 32);
		Row->SetNumberField(TEXT("triangles"), Triangles);
		Row->SetNumberField(TEXT("brushMs"), Brush);
		Row->SetNumberField(TEXT("meshBuildMs"), Build);
		Row->SetNumberField(TEXT("digDeltaBytes"), Delta.Num());
		Row->SetNumberField(TEXT("digDeltaZlibBytes"), SpikeTerrain::CompressedSize(Delta));
		Row->SetNumberField(TEXT("fullChunkBytesUncompressed"), Base.Density.Num() * 2);
		M->SetObjectField(TEXT("voxel_33cubed_at_100cm"), Row);
		AddInfo(FString::Printf(TEXT("voxel 33^3: %d tris, brush %.3f ms, build %.3f ms, dig delta %d B (%d B zlib)"),
			Triangles, Brush, Build, Delta.Num(), SpikeTerrain::CompressedSize(Delta)));
	}
	SaveMetrics();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpikeTerrainCollision, "Gridlands.Spike.Terrain.CollisionFollowsEdits", Flags)
bool FSpikeTerrainCollision::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	UWorld* World = Test.World;

	// (B) Heightfield: dig, rebuild, and the physics surface follows.
	FSpikeHeightfield Base;
	Base.Init(65, 100.0, 0.f);
	UDynamicMeshComponent* Terrain = SpawnTerrain(World, Base.BuildMesh());
	FHitResult Hit;
	const FVector Above(3200, 3200, 1000), Below(3200, 3200, -1000);
	TestTrue(TEXT("trace hits undug ground"), Trace(World, Above, Below, Hit));
	TestTrue(TEXT("at z = 0"), FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 0.0, 1.0));

	FSpikeHeightfield Dug = Base;
	Dug.ApplyBrush(FVector2D(3200, 3200), 300.0, -150.0);
	const double Start = FPlatformTime::Seconds();
	Terrain->SetMesh(Dug.BuildMesh());
	Terrain->UpdateCollision(false);
	const double RebuildMs = (FPlatformTime::Seconds() - Start) * 1000.0;
	TestTrue(TEXT("trace hits dug ground"), Trace(World, Above, Below, Hit));
	TestTrue(FString::Printf(TEXT("collision followed the dig (z = %.1f, expected -150)"), Hit.ImpactPoint.Z), FMath::IsNearlyEqual(Hit.ImpactPoint.Z, -150.0, 2.0));
	Metrics()->SetNumberField(TEXT("heightfield65_meshPlusCollisionRebuildMs"), RebuildMs);
	AddInfo(FString::Printf(TEXT("heightfield 65x65 mesh+collision rebuild: %.2f ms"), RebuildMs));

	// (C) Voxels: carve a closed tunnel under a hill. One vertical line then crosses three surfaces
	// (hill top, tunnel ceiling, tunnel floor), which no heightfield can represent.
	FSpikeVoxels Hill;
	Hill.InitGround(33, 100.0, 2400.0);
	UDynamicMeshComponent* VoxelTerrain = SpawnTerrain(World, Hill.BuildSurfaceNets());
	VoxelTerrain->GetOwner()->SetActorLocation(FVector(10000, 0, 0));
	for (int32 Y = 800; Y <= 2400; Y += 200)
	{
		Hill.ApplySphere(FVector(1600, Y, 1000), 250.0, false);
	}
	const double VoxelStart = FPlatformTime::Seconds();
	VoxelTerrain->SetMesh(Hill.BuildSurfaceNets());
	VoxelTerrain->UpdateCollision(false);
	const double VoxelRebuildMs = (FPlatformTime::Seconds() - VoxelStart) * 1000.0;
	const FVector Column(10000 + 1600, 1600, 0);
	TestTrue(TEXT("hill top is solid above the tunnel"), Trace(World, Column + FVector(0, 0, 3100), Column + FVector(0, 0, 1300), Hit));
	TestTrue(FString::Printf(TEXT("hill top at about 2400 (z = %.1f)"), Hit.ImpactPoint.Z), FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 2400.0, 60.0));
	TestTrue(TEXT("tunnel ceiling above a point inside the tunnel"), Trace(World, Column + FVector(0, 0, 1000), Column + FVector(0, 0, 2000), Hit));
	TestTrue(FString::Printf(TEXT("ceiling near 1250 (z = %.1f)"), Hit.ImpactPoint.Z), FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 1250.0, 60.0));
	TestTrue(TEXT("tunnel floor below it"), Trace(World, Column + FVector(0, 0, 1000), Column + FVector(0, 0, 0), Hit));
	TestTrue(FString::Printf(TEXT("floor near 750 (z = %.1f)"), Hit.ImpactPoint.Z), FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 750.0, 60.0));
	Metrics()->SetNumberField(TEXT("voxel33_meshPlusCollisionRebuildMs"), VoxelRebuildMs);
	AddInfo(FString::Printf(TEXT("voxel 33^3 mesh+collision rebuild: %.2f ms"), VoxelRebuildMs));
	SaveMetrics();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpikeTerrainNavigation, "Gridlands.Spike.Terrain.NavigationFollowsEdits", Flags)
bool FSpikeTerrainNavigation::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	UWorld* World = Test.World;
	FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode);
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!TestNotNull(TEXT("navigation system created"), Nav))
	{
		return false;
	}

	FSpikeHeightfield Base;
	Base.Init(65, 100.0, 0.f);
	UDynamicMeshComponent* Terrain = SpawnTerrain(World, Base.BuildMesh());
	Terrain->SetCanEverAffectNavigation(true);

	// A real NavMeshBoundsVolume with brush geometry, as a level designer would place it.
	ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>(FVector(3200, 3200, 0), FRotator::ZeroRotator);
	UCubeBuilder* Cube = NewObject<UCubeBuilder>();
	Cube->X = 7000.f;
	Cube->Y = 7000.f;
	Cube->Z = 2000.f;
	Cube->Build(World, Volume);
	Volume->GetBrushComponent()->Brush = Volume->Brush;
	Volume->GetBrushComponent()->UpdateBounds();
	Volume->GetBrushComponent()->RecreatePhysicsState();
	AddInfo(FString::Printf(TEXT("volume bounds: %s"), *Volume->GetBrushComponent()->Bounds.GetBox().ToString()));
	Volume->PostEditChange();
	Nav->OnNavigationBoundsUpdated(Volume);
	(Nav->*(&FNavBoundsAccess::SpawnMissingNavigationData))();
	const double BuildStart = FPlatformTime::Seconds();
	Nav->Build();
	const int32 BuildTicks = WaitForNavigation(Nav);
	const double BuildMs = (FPlatformTime::Seconds() - BuildStart) * 1000.0;
	AddInfo(FString::Printf(TEXT("nav data objects: %d, bounds: %d, build ticks: %d"), Nav->NavDataSet.Num(), Nav->GetNavigationBounds().Num(), BuildTicks));

	FNavLocation Projected;
	const bool bOnFlat = Nav->ProjectPointToNavigation(FVector(3200, 3200, 50), Projected, FVector(50, 50, 500));
	TestTrue(TEXT("navmesh exists on undug ground"), bOnFlat);
	if (bOnFlat)
	{
		TestTrue(FString::Printf(TEXT("navmesh at ground level (z = %.1f)"), Projected.Location.Z), FMath::Abs(Projected.Location.Z) < 30.0);
	}

	FSpikeHeightfield Dug = Base;
	Dug.ApplyBrush(FVector2D(3200, 3200), 500.0, -150.0);
	Terrain->SetMesh(Dug.BuildMesh());
	Terrain->UpdateCollision(false);
	UNavigationSystemV1::UpdateComponentInNavOctree(*Terrain);
	const double RebuildStart = FPlatformTime::Seconds();
	Nav->Build();
	const int32 RebuildTicks = WaitForNavigation(Nav);
	const double RebuildMs = (FPlatformTime::Seconds() - RebuildStart) * 1000.0;
	AddInfo(FString::Printf(TEXT("rebuild ticks: %d"), RebuildTicks));
	const bool bInPit = Nav->ProjectPointToNavigation(FVector(3200, 3200, -100), Projected, FVector(50, 50, 500));
	TestTrue(TEXT("navmesh exists in the dug pit"), bInPit);
	if (bInPit)
	{
		TestTrue(FString::Printf(TEXT("navmesh followed the dig (z = %.1f, expected about -150)"), Projected.Location.Z), Projected.Location.Z < -100.0);
	}
	Metrics()->SetNumberField(TEXT("navFullBuildMs"), BuildMs);
	Metrics()->SetNumberField(TEXT("navRebuildAfterDigMs"), RebuildMs);
	AddInfo(FString::Printf(TEXT("navmesh build %.1f ms, rebuild after dig %.1f ms"), BuildMs, RebuildMs));
	SaveMetrics();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
