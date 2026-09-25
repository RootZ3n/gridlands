#include "AIController.h"
#include "GameFramework/WorldSettings.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Building/GLBuildingSubsystem.h"
#include "Terrain/GLTerrainChunk.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLTerrainNavTests
{
	// A 128 m x 64 m field of two chunks. A and B sit on the straight line y = 32 m, either side of
	// the chunk seam at x = 64 m, where the ridge will be raised.
	const FVector A(1000, 3200, 0);
	const FVector B(11800, 3200, 0);
	constexpr double SeamX = 6400.0;
	constexpr double GapFromY = 5000.0; // the ridge runs from y = 0 to y = 50 m; the gap is 50..64 m

	struct FNavScene
	{
		GLTestUtils::FTestWorld Test;
		UGLTerrainSubsystem* Terrain = nullptr;
		UNavigationSystemV1* Nav = nullptr;

		/**
		 * The AI module creates its crowd manager when the navigation system starts, before any
		 * navmesh exists in a bare test world (the cell's runtime bounds come later) and warns once.
		 */
		static void ExpectCrowdWarning(FAutomationTestBase& Owner)
		{
			Owner.AddExpectedMessagePlain(TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager"),
				ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		}

		FNavScene(const TCHAR* Name, FAutomationTestBase& Owner) : Test(Name)
		{
			ExpectCrowdWarning(Owner);
			FNavigationSystem::AddNavigationSystemToWorld(*Test.World, FNavigationSystemRunMode::GameMode);
			Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Test.World);
			Terrain = Test.World->GetSubsystem<UGLTerrainSubsystem>();
			Terrain->Setup(FVector2D::ZeroVector, 2, 1, 65, 100.0, 0.f, 400.0, 400.0);
		}

		~FNavScene()
		{
			if (Test.World->HasBegunPlay())
			{
				Test.World->EndPlay(EEndPlayReason::Quit); // pair the walker test's begin-play
			}
		}

		/** Ticks navigation until no build is pending (async tiles), bounded. Returns false on timeout. */
		bool Settle()
		{
			for (int32 Tick = 0; Tick < 2000; ++Tick)
			{
				Nav->Tick(0.05f);
				if (Tick > 2 && !Nav->IsNavigationBuildInProgress() && !Nav->HasDirtyAreasQueued())
				{
					return true;
				}
				FPlatformProcess::Sleep(0.002f);
			}
			return false;
		}

		TArray<FVector> Path(const FVector& From, const FVector& To, bool* bPartial = nullptr)
		{
			UNavigationPath* Found = Nav->FindPathToLocationSynchronously(Test.World, From, To);
			if (bPartial)
			{
				*bPartial = !Found || !Found->IsValid() || Found->IsPartial();
			}
			return Found && Found->IsValid() ? Found->PathPoints : TArray<FVector>();
		}

		void RaiseRidge()
		{
			// 3 m of earth in strokes every 1.5 m along x = 64 m, from y = 0 to the gap.
			for (double Y = 0.0; Y <= GapFromY - 150.0; Y += 150.0)
			{
				FGLTerrainEdit Raise;
				Raise.Op = EGLTerrainOp::Raise;
				Raise.Centre = FVector2D(SeamX, Y);
				Raise.RadiusCm = 200.0;
				Raise.AmountCm = 300.0;
				Terrain->ApplyEdit(Raise);
			}
		}

		void FlattenRidge()
		{
			for (double Y = 0.0; Y <= GapFromY + 150.0; Y += 100.0)
			{
				FGLTerrainEdit Flatten;
				Flatten.Op = EGLTerrainOp::Flatten;
				Flatten.Centre = FVector2D(SeamX, Y);
				Flatten.RadiusCm = 400.0;
				Flatten.TargetHeightCm = 0.0;
				Terrain->ApplyEdit(Flatten);
				Terrain->ApplyEdit(Flatten);
			}
		}
	};

	/** Where a polyline crosses x = LineX (the y of the first crossing), or -1e9 if it never does. */
	double CrossingY(const TArray<FVector>& Points, double LineX = SeamX)
	{
		for (int32 I = 1; I < Points.Num(); ++I)
		{
			const FVector& P = Points[I - 1];
			const FVector& Q = Points[I];
			if ((P.X - LineX) * (Q.X - LineX) <= 0.0 && P.X != Q.X)
			{
				const double T = (LineX - P.X) / (Q.X - P.X);
				return FMath::Lerp(P.Y, Q.Y, T);
			}
		}
		return -1.0e9; // never crosses
	}

	double MaxY(const TArray<FVector>& Points)
	{
		double Result = -1e12;
		for (const FVector& P : Points)
		{
			Result = FMath::Max(Result, P.Y);
		}
		return Result;
	}
}

using namespace GLTerrainNavTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNavFollowsEdits, "Gridlands.Game.Terrain.NavigationFollowsRuntimeEdits", GLTestUtils::Flags)
bool FGLNavFollowsEdits::RunTest(const FString& Parameters)
{
	// M10 HARD GATE (ADR-0022 open proof 1): paths follow runtime terrain edits.
	FNavScene Scene(TEXT("GLTerrainNavWorld"), *this);
	if (!TestNotNull(TEXT("navigation system"), Scene.Nav) || !TestTrue(TEXT("initial navmesh builds"), Scene.Settle()))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("navigable bounds from the cell: %s"), *Scene.Nav->GetNavigableWorldBounds().ToString()));
	FNavLocation Projected;
	TestTrue(TEXT("the cell's runtime bounds produced a navmesh on the ground"), Scene.Nav->ProjectPointToNavigation(A, Projected, FVector(50, 50, 300)));

	bool bPartial = true;
	const TArray<FVector> Before = Scene.Path(A, B, &bPartial);
	TestFalse(TEXT("before: a complete path"), bPartial);
	const double BeforeCrossing = CrossingY(Before);
	TestTrue(FString::Printf(TEXT("before: the path goes straight across (crosses at y = %.0f)"), BeforeCrossing), FMath::Abs(BeforeCrossing - 3200.0) < 200.0);

	Scene.RaiseRidge();
	TestTrue(TEXT("a 3 m ridge now stands on the old route"), Scene.Terrain->HeightAt(FVector2D(SeamX, 3200)) > 250.0);
	TestTrue(TEXT("navigation rebuilds after the edit"), Scene.Settle());
	const TArray<FVector> After = Scene.Path(A, B, &bPartial);
	TestFalse(TEXT("after: still reachable"), bPartial);
	const double AfterCrossing = CrossingY(After);
	TestTrue(FString::Printf(TEXT("after: the path detours through the gap (crosses at y = %.0f, gap from %.0f)"), AfterCrossing, GapFromY), AfterCrossing > GapFromY - 50.0);
	TestFalse(TEXT("after: the old straight route is not offered"), FMath::Abs(AfterCrossing - 3200.0) < 1000.0);

	AddInfo(FString::Printf(TEXT("path crossings of x = 64 m: before y = %.0f, after the ridge y = %.0f"), BeforeCrossing, AfterCrossing));
	Scene.FlattenRidge();
	TestTrue(TEXT("the ridge is gone"), FMath::Abs(Scene.Terrain->HeightAt(FVector2D(SeamX, 3200))) < 10.0);
	TestTrue(TEXT("navigation rebuilds after flattening"), Scene.Settle());
	const TArray<FVector> Restored = Scene.Path(A, B, &bPartial);
	TestTrue(FString::Printf(TEXT("flattened: the direct route is back (crosses at y = %.0f)"), CrossingY(Restored)), !bPartial && FMath::Abs(CrossingY(Restored) - 3200.0) < 200.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNavAgentWalks, "Gridlands.Game.Terrain.AIWalksAroundARaisedRidge", GLTestUtils::Flags)
bool FGLNavAgentWalks::RunTest(const FString& Parameters)
{
	// An AI-controlled character actually walks the new route; it never goes through the old one.
	FNavScene Scene(TEXT("GLTerrainAgentWorld"), *this);
	TestTrue(TEXT("initial navmesh"), Scene.Settle());
	Scene.RaiseRidge();
	TestTrue(TEXT("rebuilt after the ridge"), Scene.Settle());

	ACharacter* Walker = Scene.Test.World->SpawnActor<ACharacter>(A + FVector(0, 0, 100), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("walker"), Walker))
	{
		return false;
	}
	Walker->AIControllerClass = AAIController::StaticClass();
	Walker->SpawnDefaultController();
	AAIController* Brain = Cast<AAIController>(Walker->GetController());
	if (!TestNotNull(TEXT("AI controller"), Brain))
	{
		return false;
	}
	// Begin play without a game mode (Gridlands' mode would load the player's save).
	Scene.Test.World->GetWorldSettings()->NotifyBeginPlay();
	// This bare automation world does not dispatch actor ticks, so step the real components
	// directly: the AI's path following and the character's movement, in the engine's order.
	UPathFollowingComponent* Follow = Brain->GetPathFollowingComponent();
	UCharacterMovementComponent* Move = Walker->GetCharacterMovement();
	auto Step = [&](float Dt)
	{
		Scene.Test.World->Tick(LEVELTICK_TimeOnly, Dt);
		Follow->TickComponent(Dt, LEVELTICK_All, &Follow->PrimaryComponentTick);
		Move->TickComponent(Dt, LEVELTICK_All, &Move->PrimaryComponentTick);
	};
	for (int32 Tick = 0; Tick < 20; ++Tick)
	{
		Step(0.05f); // settle on the ground
	}
	const EPathFollowingRequestResult::Type Request = Brain->MoveToLocation(B, 100.f, false, true, false, true);
	TestEqual(TEXT("the move is accepted"), static_cast<int32>(Request), static_cast<int32>(EPathFollowingRequestResult::RequestSuccessful));

	TArray<FVector> Trail;
	for (int32 Tick = 0; Tick < 1600 && Brain->GetMoveStatus() == EPathFollowingStatus::Moving; ++Tick)
	{
		Step(0.05f);
		Trail.Add(Walker->GetActorLocation());
	}
	const double Remaining = FVector::Dist2D(Walker->GetActorLocation(), B);
	TestTrue(FString::Printf(TEXT("the walker arrives (%.0f cm from B after %d ticks)"), Remaining, Trail.Num()), Remaining < 200.0);
	const double Crossing = CrossingY(Trail);
	TestTrue(FString::Printf(TEXT("it crossed the ridge line through the gap (y = %.0f)"), Crossing), Crossing > GapFromY - 100.0);
	TestTrue(TEXT("it went out of its way to do so"), MaxY(Trail) > GapFromY - 100.0);
	double HighestZ = -1e12;
	for (const FVector& P : Trail)
	{
		HighestZ = FMath::Max(HighestZ, P.Z);
	}
	TestTrue(FString::Printf(TEXT("it never climbed the ridge (highest z %.0f)"), HighestZ), HighestZ < 200.0);
	AddInfo(FString::Printf(TEXT("walker: arrived %.0f cm from B in %d ticks, crossed x = 64 m at y = %.0f, max y %.0f, max z %.0f"),
		Remaining, Trail.Num(), Crossing, MaxY(Trail), HighestZ));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNavStaleControl, "Gridlands.Game.Terrain.StaleNavigationIsDetectedControl", GLTestUtils::Flags)
bool FGLNavStaleControl::RunTest(const FString& Parameters)
{
	// Control: if edits do NOT tell navigation, the old straight route over the ridge is still
	// offered. This is what the acceptance tests above would see if navigation were not updated,
	// so their "detours through the gap" assertions can fail.
	FNavScene Scene(TEXT("GLTerrainNavControlWorld"), *this);
	TestTrue(TEXT("initial navmesh"), Scene.Settle());
	Scene.Terrain->bNotifyNavigation = false;
	Scene.RaiseRidge();
	TestTrue(TEXT("the ridge is really there (collision/height)"), Scene.Terrain->HeightAt(FVector2D(SeamX, 3200)) > 250.0);
	Scene.Settle();
	bool bPartial = true;
	const double Crossing = CrossingY(Scene.Path(A, B, &bPartial));
	TestTrue(FString::Printf(TEXT("stale navigation still offers the straight route over the ridge (y = %.0f)"), Crossing), !bPartial && FMath::Abs(Crossing - 3200.0) < 200.0);
	AddInfo(FString::Printf(TEXT("control (navigation not told): path crosses x = 64 m at y = %.0f, straight over the ridge"), Crossing));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNavAroundWalls, "Gridlands.Game.Building.PiecesBlockNavigation", GLTestUtils::Flags)
bool FGLNavAroundWalls::RunTest(const FString& Parameters)
{
	// Built walls are obstacles too: a wall line across the route, with a gap, forces a detour.
	FNavScene Scene(TEXT("GLBuildNavWorld"), *this);
	TestTrue(TEXT("initial navmesh"), Scene.Settle());
	TArray<FGLPlacedPiece> Line;
	int32 Id = 1;
	for (double Y = 100.0; Y < GapFromY; Y += 200.0)
	{
		Line.Add({ Id++, TEXT("buildpiece.modern.timber_foundation"), FVector(SeamX, Y, 0.0), 0 });
		Line.Add({ Id++, TEXT("buildpiece.modern.timber_wall"), FVector(SeamX + 100.0, Y, 30.0), 1 });
	}
	Scene.Test.World->GetSubsystem<UGLBuildingSubsystem>()->Restore(Line, Id);
	TestTrue(TEXT("navigation rebuilds around the pieces"), Scene.Settle());
	bool bPartial = true;
	const double WallX = SeamX + 100.0; // the walls stand on the floors' east edge
	const double Crossing = CrossingY(Scene.Path(A, B, &bPartial), WallX);
	TestTrue(FString::Printf(TEXT("the path passes the wall line through the gap (crosses at y = %.0f)"), Crossing), !bPartial && Crossing > GapFromY - 50.0);
	AddInfo(FString::Printf(TEXT("wall line with a gap: path crosses the wall line x = 65 m at y = %.0f"), Crossing));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
