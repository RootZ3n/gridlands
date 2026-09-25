#include "Building/GLStructureRules.h"
#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Inventory/GLInventory.h"
#include "Knowledge/GLKnowledge.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Terrain/GLHeightfield.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLStructureTests
{
	constexpr EAutomationTestFlags StructureFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	const FGLContentRegistry& StructureContent()
	{
		static FGLContentRegistry Registry;
		static bool bLoaded = Registry.LoadRepository(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		return Registry;
	}

	const FName Floor(TEXT("buildpiece.modern.timber_foundation"));
	const FName Wall(TEXT("buildpiece.modern.timber_wall"));
	const FName Door(TEXT("buildpiece.modern.timber_doorway"));
	const FName Roof(TEXT("buildpiece.modern.timber_roof"));

	double FlatGround(const FVector2D&) { return 0.0; }

	FGLPlacedPiece Piece(int32 Id, FName Def, FVector At, int32 Yaw = 0) { return { Id, Def, At, Yaw }; }

	/** A 4 m x 4 m timber shelter: 4 floors, 7 walls and a doorway, 4 roof slopes. Ids: floors 1-4, walls 11-18, roofs 21-24. */
	TArray<FGLPlacedPiece> Shelter()
	{
		TArray<FGLPlacedPiece> P;
		P.Add(Piece(1, Floor, FVector(-100, -100, 0)));
		P.Add(Piece(2, Floor, FVector(100, -100, 0)));
		P.Add(Piece(3, Floor, FVector(-100, 100, 0)));
		P.Add(Piece(4, Floor, FVector(100, 100, 0)));
		P.Add(Piece(11, Wall, FVector(-100, -200, 30)));
		P.Add(Piece(12, Door, FVector(100, -200, 30)));
		P.Add(Piece(13, Wall, FVector(-100, 200, 30)));
		P.Add(Piece(14, Wall, FVector(100, 200, 30)));
		P.Add(Piece(15, Wall, FVector(-200, -100, 30), 1));
		P.Add(Piece(16, Wall, FVector(-200, 100, 30), 1));
		P.Add(Piece(17, Wall, FVector(200, -100, 30), 1));
		P.Add(Piece(18, Wall, FVector(200, 100, 30), 1));
		P.Add(Piece(21, Roof, FVector(-100, -100, 280)));
		P.Add(Piece(22, Roof, FVector(100, -100, 280)));
		P.Add(Piece(23, Roof, FVector(-100, 100, 280), 2));
		P.Add(Piece(24, Roof, FVector(100, 100, 280), 2));
		return P;
	}
}

using namespace GLStructureTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStructureShelter, "Gridlands.Core.Building.ShelterStandsAndEveryPlacementIsValid", StructureFlags)
bool FGLStructureShelter::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = StructureContent();
	const TArray<FGLPlacedPiece> All = Shelter();
	// Built in order, every piece passes the placement check against what came before.
	TArray<FGLPlacedPiece> Built;
	for (const FGLPlacedPiece& Next : All)
	{
		const FGLBuildCheck Check = GLStructureRules::CheckPlacement(Content, Built, Next, FlatGround);
		TestTrue(FString::Printf(TEXT("piece %d placeable (%s)"), Next.Id, *Check.Reason), Check.IsAllowed());
		Built.Add(Next);
	}
	const TMap<int32, double> Support = GLStructureRules::ComputeSupport(Content, All, FlatGround);
	TestEqual(TEXT("floors rest on the ground: full pine strength"), Support.FindRef(1), 3.0);
	TestEqual(TEXT("walls lose one vertical step (3/4)"), Support.FindRef(11), 2.25);
	TestEqual(TEXT("the doorway is a wall structurally"), Support.FindRef(12), 2.25);
	TestEqual(TEXT("roofs lose another"), Support.FindRef(21), 1.5);
	TestEqual(TEXT("mirrored roofs too"), Support.FindRef(24), 1.5);
	TestEqual(TEXT("a second floor in the same place overlaps"), GLStructureRules::CheckPlacement(Content, All, Piece(99, Floor, FVector(-100, -100, 0)), FlatGround).Refusal, EGLBuildRefusal::Overlaps);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStructureLimits, "Gridlands.Core.Building.SupportWeakensUpAndOut", StructureFlags)
bool FGLStructureLimits::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = StructureContent();
	// Upward: pine maxStack 4 -> a floor and three walls stand, a fourth wall has no support left.
	TArray<FGLPlacedPiece> Tower = { Piece(1, Floor, FVector(0, 0, 0)) };
	for (int32 Level = 0; Level < 3; ++Level)
	{
		const FGLPlacedPiece Next = Piece(10 + Level, Wall, FVector(0, 100, 30 + 250 * Level));
		TestTrue(FString::Printf(TEXT("wall level %d stands"), Level + 1), GLStructureRules::CheckPlacement(Content, Tower, Next, FlatGround).IsAllowed());
		Tower.Add(Next);
	}
	const FGLBuildCheck TooHigh = GLStructureRules::CheckPlacement(Content, Tower, Piece(20, Wall, FVector(0, 100, 30 + 750)), FlatGround);
	TestEqual(TEXT("the fourth wall up is unsupported"), TooHigh.Refusal, EGLBuildRefusal::Unsupported);

	// Outward: over a drop, a floor can hang one 2 m step off a grounded floor (3 - 3*2/3 = 1), not two.
	auto Cliff = [](const FVector2D& At) { return At.X < 210.0 ? 0.0 : -300.0; };
	TArray<FGLPlacedPiece> Deck = { Piece(1, Floor, FVector(100, 0, 0)) };
	const FGLBuildCheck One = GLStructureRules::CheckPlacement(Content, Deck, Piece(2, Floor, FVector(300, 0, 0)), Cliff);
	TestTrue(TEXT("one step out over the drop holds"), One.IsAllowed());
	TestEqual(TEXT("with reduced support"), One.Support, 1.0);
	Deck.Add(Piece(2, Floor, FVector(300, 0, 0)));
	TestEqual(TEXT("two steps out does not"), GLStructureRules::CheckPlacement(Content, Deck, Piece(3, Floor, FVector(500, 0, 0)), Cliff).Refusal, EGLBuildRefusal::Unsupported);
	TestEqual(TEXT("and a floor floating alone is unsupported"), GLStructureRules::CheckPlacement(Content, {}, Piece(4, Floor, FVector(900, 0, 0)), Cliff).Refusal, EGLBuildRefusal::Unsupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStructureCollapse, "Gridlands.Core.Building.RemovingASupportCollapsesWhatItHeld", StructureFlags)
bool FGLStructureCollapse::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = StructureContent();
	const TArray<FGLPlacedPiece> All = Shelter();
	TestEqual(TEXT("removing a gable wall drops nothing"), GLStructureRules::CollapsesAfterRemoving(Content, All, 15, FlatGround).Num(), 0);
	const TArray<int32> Falls = GLStructureRules::CollapsesAfterRemoving(Content, All, 11, FlatGround);
	TestEqual(TEXT("removing the wall under a roof slope drops exactly that slope"), Falls, TArray<int32>{ 21 });
	// Removing a floor: the two walls that stood on it hang on to their neighbours sideways
	// (2.25 - 3 * 2 m / 3 m = 0.25), too weak to carry the roof slope, which falls.
	TArray<FGLPlacedPiece> NoFloor = All;
	NoFloor.RemoveAll([](const FGLPlacedPiece& P) { return P.Id == 1; });
	const TMap<int32, double> Weak = GLStructureRules::ComputeSupport(Content, NoFloor, FlatGround);
	TestTrue(TEXT("the corner walls hold on sideways, weakly"), FMath::IsNearlyEqual(Weak.FindRef(11), 0.25) && FMath::IsNearlyEqual(Weak.FindRef(15), 0.25));
	TestEqual(TEXT("and only the roof slope above falls"), GLStructureRules::CollapsesAfterRemoving(Content, All, 1, FlatGround), TArray<int32>{ 21 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStructureGround, "Gridlands.Core.Building.GroundDecidesWhereFloorsGo", StructureFlags)
bool FGLStructureGround::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = StructureContent();
	FGLHeightfield Ground;
	Ground.Init(FVector2D(-3200, -3200), 65, 65, 100.0, 0.f, 400.0, 400.0);
	auto Height = [&Ground](const FVector2D& At) { return Ground.HeightAt(At); };
	// A mound: the corners of a floor are at different heights -> not firm, level ground.
	Ground.Apply({ EGLTerrainOp::Raise, FVector2D(0, 0), 250.0, 120.0, 0.0 });
	FGLPlacedPiece OnMound;
	TestTrue(TEXT("snaps onto the ground"), GLStructureRules::Snap(Content, {}, Floor, FVector(100, 0, 0), 0, Height, OnMound));
	const FGLBuildCheck Rough = GLStructureRules::CheckPlacement(Content, {}, OnMound, Height);
	TestFalse(FString::Printf(TEXT("refused on the mound's slope (%s)"), *Rough.Reason), Rough.IsAllowed());
	// Flatten, then the same spot works.
	FGLTerrainEdit Flatten;
	Flatten.Op = EGLTerrainOp::Flatten;
	Flatten.Centre = FVector2D(100, 0);
	Flatten.RadiusCm = 400.0;
	Flatten.TargetHeightCm = 0.0;
	Ground.Apply(Flatten);
	Ground.Apply(Flatten);
	TestTrue(TEXT("snaps again"), GLStructureRules::Snap(Content, {}, Floor, FVector(100, 0, 0), 0, Height, OnMound));
	const FGLBuildCheck Level = GLStructureRules::CheckPlacement(Content, {}, OnMound, Height);
	TestTrue(FString::Printf(TEXT("allowed once flattened (%s)"), *Level.Reason), Level.IsAllowed());
	// Buried: a floor forced below the ground is refused.
	TestEqual(TEXT("buried"), GLStructureRules::CheckPlacement(Content, {}, Piece(5, Floor, FVector(100, 0, -100)), Height).Refusal, EGLBuildRefusal::Buried);
	// The ground under a floor is protected from terraforming.
	const TArray<FGLPlacedPiece> Placed = { OnMound };
	TestTrue(TEXT("under the floor is protected"), GLStructureRules::IsUnderStructure(Content, Placed, FVector2D(150, 50)));
	TestFalse(TEXT("away from it is not"), GLStructureRules::IsUnderStructure(Content, Placed, FVector2D(600, 600)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStructureGates, "Gridlands.Core.Building.KnowledgeItemsAndSnapping", StructureFlags)
bool FGLStructureGates::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = StructureContent();
	FGLKnowledge Knowledge;
	FGLInventory Inventory;
	const TArray<FGLPlacedPiece> Base = { Piece(1, Floor, FVector(0, 0, 0)) };
	const FGLPlacedPiece WallOnEdge = Piece(2, Wall, FVector(0, 100, 30));
	FGLBuildCheck Check = GLStructureRules::CanPlace(Content, Base, WallOnEdge, FlatGround, Knowledge, Inventory);
	TestEqual(TEXT("unknown style is refused"), Check.Refusal, EGLBuildRefusal::NotKnown);
	TestEqual(TEXT("names what is missing"), Check.MissingKnowledge, TArray<FName>{ TEXT("knowledge.style.modern_timber_frame") });
	Knowledge.Learn(TEXT("knowledge.style.modern_timber_frame"));
	TestEqual(TEXT("no planks, no wall"), GLStructureRules::CanPlace(Content, Base, WallOnEdge, FlatGround, Knowledge, Inventory).Refusal, EGLBuildRefusal::MissingItems);
	Inventory.Add(Content, TEXT("item.material.timber_plank"), 2);
	TestTrue(TEXT("known and paid for"), GLStructureRules::CanPlace(Content, Base, WallOnEdge, FlatGround, Knowledge, Inventory).IsAllowed());

	FGLPlacedPiece Snapped;
	TestTrue(TEXT("a wall aimed near the north edge snaps"), GLStructureRules::Snap(Content, Base, Wall, FVector(20, 90, 60), 0, FlatGround, Snapped));
	TestEqual(TEXT("onto the edge socket exactly"), Snapped.Location, FVector(0, 100, 30));
	FGLPlacedPiece Nowhere;
	TestFalse(TEXT("a wall aimed at empty ground has nothing to snap to"), GLStructureRules::Snap(Content, {}, Wall, FVector(900, 900, 0), 0, FlatGround, Nowhere));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
