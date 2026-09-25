// Localized navigation (ADR-0029): at canonical 1 km cells, navigation tiles exist only around
// navigation invokers (Zenny, each creature), follow them, follow terrain edits, cross cell
// boundaries, and leave nothing behind when a cell unloads.

#include "Character/GLCharacter.h"
#include "Combat/GLCreature.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Terrain/GLCellNavBounds.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLGridSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLLocalNavTests
{
	const FVector NInOrigin(0, -1200, 100);
	const FVector NAtBoundary(51100, 0, 100);  // origin side of x = 512 m, both cells loaded
	const FVector NDeepInLots(120000, 0, 100); // the origin unloads
	constexpr double NCellCm = 102400.0;

	/** The real Zenny class (with its invoker) in a Grid world with a navigation system. */
	struct FNavGridScene
	{
		GLTestUtils::FTestWorld Test;
		UNavigationSystemV1* Nav = nullptr;
		UGLGridSubsystem* Grid = nullptr;
		UGLTerrainSubsystem* Terrain = nullptr;
		AGLCharacter* Zenny = nullptr;

		FNavGridScene(const TCHAR* Name, FAutomationTestBase& Owner) : Test(Name)
		{
			Owner.AddExpectedMessagePlain(TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager"),
				ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			FNavigationSystem::AddNavigationSystemToWorld(*Test.World, FNavigationSystemRunMode::GameMode);
			Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Test.World);
			Zenny = Test.World->SpawnActor<AGLCharacter>(NInOrigin, FRotator::ZeroRotator);
			Grid = Test.World->GetSubsystem<UGLGridSubsystem>();
			Grid->bShowBoundaries = false;
			Grid->Enable(false); // the runtime layer: ground, placements, nav bounds
			Terrain = Test.World->GetSubsystem<UGLTerrainSubsystem>();
			GoTo(NInOrigin);
		}

		void GoTo(const FVector& Where)
		{
			Zenny->SetActorLocation(Where);
			Grid->Advance(Where);
			Grid->FlushAll();
		}

		/**
		 * One navigation tick. Invoker updates are scheduled on world time (every 0.5 s), and a bare
		 * test world never advances its clock, so the test does what the running game does.
		 */
		void TickNav()
		{
			Test.World->TimeSeconds += 0.05;
			Nav->Tick(0.05f);
		}

		/** Ticks navigation until nothing is pending. False on timeout. */
		bool Settle()
		{
			for (int32 Tick = 0; Tick < 4000; ++Tick)
			{
				TickNav();
				if (Tick > 24 && !Nav->IsNavigationBuildInProgress() && !Nav->HasDirtyAreasQueued())
				{
					return true;
				}
				FPlatformProcess::Sleep(0.002f);
			}
			return false;
		}

		ARecastNavMesh* Recast() const
		{
			return Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate));
		}

		int32 ActiveTiles() const
		{
			const ARecastNavMesh* R = Recast();
			return R ? R->GetNumActiveTiles() : 0;
		}

		/** Tiles a whole 1 km cell would need. */
		double WholeCellTiles() const
		{
			const ARecastNavMesh* R = Recast();
			const double Tile = R ? R->GetTileSizeUU() : 1000.0;
			return FMath::Square(FMath::CeilToDouble(NCellCm / Tile));
		}

		bool OnNav(const FVector2D& At, FVector* OutPoint = nullptr) const
		{
			FNavLocation Found;
			const bool bFound = Nav->ProjectPointToNavigation(FVector(At, Terrain->HeightAt(At)), Found, FVector(50, 50, 400));
			if (bFound && OutPoint)
			{
				*OutPoint = Found.Location;
			}
			return bFound;
		}

		bool CompletePath(const FVector2D& From, const FVector2D& To) const
		{
			UNavigationPath* Path = Nav->FindPathToLocationSynchronously(Test.World,
				FVector(From, Terrain->HeightAt(From) + 50.0), FVector(To, Terrain->HeightAt(To) + 50.0));
			return Path && Path->IsValid() && !Path->IsPartial();
		}

		int32 NavBoundsActors() const
		{
			int32 Count = 0;
			for (TActorIterator<AGLCellNavBounds> It(Test.World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					++Count;
				}
			}
			return Count;
		}
	};
}

using GLLocalNavTests::FNavGridScene;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNavLocalOnly, "Gridlands.Game.Navigation.BuiltOnlyAroundInvokersAndFollowsEdits", GLTestUtils::Flags)
bool FGLNavLocalOnly::RunTest(const FString& Parameters)
{
	FNavGridScene S(TEXT("GLLocalNavWorld"), *this);
	if (!TestNotNull(TEXT("navigation system"), S.Nav) || !TestTrue(TEXT("navigation settles around Zenny"), S.Settle()))
	{
		return false;
	}
	const int32 Tiles = S.ActiveTiles();
	AddInfo(FString::Printf(TEXT("active tiles around Zenny: %d (a whole 1 km cell: %.0f)"), Tiles, S.WholeCellTiles()));
	TestTrue(TEXT("navigation exists around Zenny"), Tiles > 0 && S.OnNav(FVector2D(1500, -1200)));
	TestTrue(TEXT("no whole-cell navigation: under a tenth of the cell's tiles"), Tiles < S.WholeCellTiles() / 10.0);
	TestFalse(TEXT("nothing 300 m away"), S.OnNav(FVector2D(30000, -1200)));

	// Zenny walks 300 m: navigation arrives with Zenny and leaves the old place.
	S.GoTo(FVector(30000, -1200, 100));
	TestTrue(TEXT("settles after the walk"), S.Settle());
	TestTrue(TEXT("navigation where Zenny now is"), S.OnNav(FVector2D(31500, -1200)));
	TestFalse(TEXT("removed where Zenny was (beyond the removal radius)"), S.OnNav(FVector2D(0, -1200)));
	TestTrue(TEXT("still bounded after moving"), S.ActiveTiles() < S.WholeCellTiles() / 10.0);

	// A 3 m mound raised 20 m from Zenny: navigation follows the edit.
	const FVector2D Mound(32000, -1200);
	FVector Before, After;
	TestTrue(TEXT("the spot is navigable before"), S.OnNav(Mound, &Before));
	for (int32 I = 0; I < 3; ++I)
	{
		FGLTerrainEdit Raise;
		Raise.Op = EGLTerrainOp::Raise;
		Raise.Centre = Mound;
		Raise.RadiusCm = 600.0;
		Raise.AmountCm = 100.0;
		S.Terrain->ApplyEdit(Raise);
	}
	TestTrue(TEXT("settles after the edit"), S.Settle());
	TestTrue(TEXT("the mound is navigable"), S.OnNav(Mound, &After));
	AddInfo(FString::Printf(TEXT("navmesh at the mound: %.0f -> %.0f cm (ground %.0f)"), Before.Z, After.Z, S.Terrain->HeightAt(Mound)));
	// Recast approximates a steep 6 m-radius mound (cell height, detail sampling), so the navmesh sits
	// below the peak; a stale navmesh would still be at the old ground.
	TestTrue(TEXT("the navmesh rose with the ground (well over a metre)"), After.Z - Before.Z > 120.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNavCreatureOwn, "Gridlands.Game.Navigation.CreaturesPathFarFromThePlayer", GLTestUtils::Flags)
bool FGLNavCreatureOwn::RunTest(const FString& Parameters)
{
	FNavGridScene S(TEXT("GLCreatureNavWorld"), *this);
	// A creature 420 m from Zenny, in the same loaded cell.
	const FVector2D Home(-30000, 30000);
	AGLCreature* Creature = S.Test.World->SpawnActor<AGLCreature>(FVector(Home, S.Terrain->HeightAt(Home) + 100.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("creature"), Creature) || !TestTrue(TEXT("settles"), S.Settle()))
	{
		return false;
	}
	TestTrue(TEXT("navigation around the creature, far from Zenny"), S.OnNav(Home));
	TestTrue(TEXT("the creature can path across its reach (16 m)"), S.CompletePath(Home, Home + FVector2D(1600, 0)));
	TestFalse(TEXT("but not a tile between it and Zenny"), S.OnNav(FVector2D(-15000, 15000)));

	// When it goes (killed and de-rezzed), its navigation goes with it.
	Creature->Destroy();
	TestTrue(TEXT("settles after it goes"), S.Settle());
	TestFalse(TEXT("no navigation left where it was"), S.OnNav(Home));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNavAcrossCells, "Gridlands.Game.Navigation.CrossesBoundariesAndUnloadsCleanly", GLTestUtils::Flags)
bool FGLNavAcrossCells::RunTest(const FString& Parameters)
{
	FNavGridScene S(TEXT("GLCellNavWorld"), *this);
	const FName Origin(TEXT("cell.home.origin"));
	const FName Lots(TEXT("cell.outer.diner_lots"));

	// At the boundary, both cells have ground and one continuous navmesh crosses it.
	S.GoTo(GLLocalNavTests::NAtBoundary);
	TestTrue(TEXT("settles at the boundary"), S.Settle());
	TestTrue(TEXT("both cells are loaded"), S.Grid->IsLoaded(Origin) && S.Grid->IsLoaded(Lots));
	TestEqual(TEXT("one set of nav bounds per loaded cell"), S.NavBoundsActors(), 2);
	TestTrue(TEXT("a path crosses the boundary"), S.CompletePath(FVector2D(49000, 0), FVector2D(54000, 0)));

	// Deep in the lots, streamed frame by frame while navigation ticks: the origin unloads.
	for (int32 Frame = 0; Frame < 400 && !(S.Grid->IsLoaded(Lots) && !S.Grid->IsLoaded(Origin) && S.Terrain->IsCellComplete(Lots)); ++Frame)
	{
		S.Zenny->SetActorLocation(GLLocalNavTests::NDeepInLots);
		S.Grid->Advance(GLLocalNavTests::NDeepInLots);
		S.TickNav();
	}
	TestTrue(TEXT("the origin unloaded while streaming"), !S.Grid->IsLoaded(Origin) && S.Grid->IsLoaded(Lots));
	TestTrue(TEXT("settles deep in the lots"), S.Settle());
	TestTrue(TEXT("navigation under Zenny in the lots"), S.OnNav(FVector2D(121500, 0)));
	TestEqual(TEXT("the unloaded cell's nav bounds are gone"), S.NavBoundsActors(), 1);
	TestFalse(TEXT("no navigation left at the old boundary"), S.OnNav(FVector2D(50000, 0)));
	TestFalse(TEXT("the navigable world no longer includes the origin"),
		S.Nav->GetNavigableWorldBounds().IsInsideXY(FVector(0, 0, 0)));
	TestTrue(TEXT("bounded in the lots"), S.ActiveTiles() < S.WholeCellTiles() / 10.0);

	// Back home: rebuilt there, nothing duplicated.
	S.GoTo(GLLocalNavTests::NInOrigin);
	TestTrue(TEXT("settles back home"), S.Settle());
	TestTrue(TEXT("navigation at home again"), S.OnNav(FVector2D(1500, -1200)));
	TestEqual(TEXT("one set of nav bounds again"), S.NavBoundsActors(), 1);
	TestFalse(TEXT("nothing left in the lots"), S.OnNav(FVector2D(121500, 0)));
	return true;
}

#endif
