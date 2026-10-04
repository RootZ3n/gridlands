// P12 (ADR-0040) playable building, pure rules: the catalogue, the snap the marker shows, the consumption plan the cost
// display shows, and the structured refusal the reason text is generated from.

#include "Building/GLBuildCatalog.h"
#include "Building/GLStructureRules.h"
#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Inventory/GLInventory.h"
#include "Inventory/GLMaterialPool.h"
#include "Knowledge/GLKnowledge.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLPlayableRulesTests
{
	constexpr EAutomationTestFlags PRFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	const FGLContentRegistry& PRContent()
	{
		static FGLContentRegistry Registry;
		static bool bLoaded = Registry.LoadRepository(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		return Registry;
	}

	double PRFlat(const FVector2D&) { return 0.0; }

	FGLItemStackDef PRStack(FName Item, int32 Count)
	{
		FGLItemStackDef Stack;
		Stack.Item = Item;
		Stack.Count = Count;
		return Stack;
	}
}

using namespace GLPlayableRulesTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPRCatalog, "Gridlands.Core.PlayableBuilding.TheCatalogueIsTotalOrderedAndDataDriven", PRFlags)
bool FGLPRCatalog::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = PRContent();
	const TArray<FGLCatalogCategory> Catalog = GLBuildCatalog::Build(Content);
	TestTrue(TEXT("categories exist"), Catalog.Num() >= 6);
	TestFalse(TEXT("valid data leaves nothing in Other"), Catalog.ContainsByPredicate([](const FGLCatalogCategory& C) { return C.Id == GLBuildCatalog::OtherCategory; }));
	for (const FGLCatalogCategory& C : Catalog)
	{
		for (int32 I = 1; I < C.Pieces.Num(); ++I)
		{
			const FString A = Content.Find<FGLBuildPieceDef>(C.Pieces[I - 1])->DisplayName, B = Content.Find<FGLBuildPieceDef>(C.Pieces[I])->DisplayName;
			TestTrue(FString::Printf(TEXT("%s: by display name"), *C.Id.ToString()), A < B || (A == B && C.Pieces[I - 1].LexicalLess(C.Pieces[I])));
		}
	}
	TestEqual(TEXT("an explicit category wins"), GLBuildCatalog::CategoryOf(Content, TEXT("buildpiece.modern.window_wall")), FName(TEXT("buildcategory.building.walls_openings")));
	TestEqual(TEXT("the upper floor falls back by its role"), GLBuildCatalog::CategoryOf(Content, TEXT("buildpiece.modern.upper_floor")), FName(TEXT("buildcategory.building.foundations_floors")));
	TestEqual(TEXT("a world-only piece has no category"), GLBuildCatalog::CategoryOf(Content, TEXT("buildpiece.modern.timber_post")), FName());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPRSnap, "Gridlands.Core.PlayableBuilding.SnapReportsWhatItConnectedAndTheAimChoosesTheRestingSocket", PRFlags)
bool FGLPRSnap::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = PRContent();
	// A floor, stud walls on its east and west edges: an upper floor aimed between them rests on both by its east and
	// west edge midpoints (the aim chooses which of its four bottom sockets meets the nearest wall top).
	const TArray<FGLPlacedPiece> Room = {
		{ 1, TEXT("buildpiece.modern.timber_foundation"), FVector(0, 0, 0), 0 },
		{ 2, TEXT("buildpiece.modern.timber_wall"), FVector(100, 0, 30), GLStructureRules::QuarterTurnSteps },
		{ 3, TEXT("buildpiece.modern.timber_wall"), FVector(-100, 0, 30), GLStructureRules::QuarterTurnSteps } };
	FGLPlacedPiece Upper;
	FGLSnapInfo Info;
	if (!TestTrue(TEXT("the upper floor snaps"), GLStructureRules::Snap(Content, Room, TEXT("buildpiece.modern.upper_floor"), FVector(10, 0, 285), 0, PRFlat, Upper, &Info)))
	{
		return false;
	}
	TestTrue(TEXT("centred over the room, not a metre off by data order"), Upper.Location.Equals(FVector(0, 0, 280), 0.5));
	TestTrue(TEXT("it reports what it connected"), Info.bSnapped && (Info.TargetPieceId == 2 || Info.TargetPieceId == 3) && Info.TargetSocket == FName(TEXT("top")));
	TestTrue(TEXT("by the matching edge"), (Info.TargetPieceId == 2 && Info.OwnSocket == FName(TEXT("rest_e"))) || (Info.TargetPieceId == 3 && Info.OwnSocket == FName(TEXT("rest_w"))));
	TArray<FGLPlacedPiece> WithUpper = Room;
	Upper.Id = 4;
	WithUpper.Add(Upper);
	TestTrue(TEXT("and it stands (resting on both walls)"), GLStructureRules::ComputeSupport(Content, WithUpper, PRFlat).FindRef(4) > 0.0);
	// Aimed at the outside face of one wall (the view from outside the room): it still rests on both walls, not straddling
	// the one aimed at, whatever its yaw.
	const TArray<FGLPlacedPiece> RoomNS = {
		{ 1, TEXT("buildpiece.modern.timber_foundation"), FVector(0, 0, 0), 0 },
		{ 2, TEXT("buildpiece.modern.timber_wall"), FVector(0, 100, 30), 0 },
		{ 3, TEXT("buildpiece.modern.timber_wall"), FVector(0, -100, 30), 0 } };
	for (const int32 Yaw : { 0, GLStructureRules::QuarterTurnSteps, 3 * GLStructureRules::QuarterTurnSteps })
	{
		FGLPlacedPiece Over;
		TestTrue(FString::Printf(TEXT("yaw %d: rests on both walls when aimed at one from outside"), Yaw),
			GLStructureRules::Snap(Content, RoomNS, TEXT("buildpiece.modern.upper_floor"), FVector(-9.6, -109.8, 264.5), Yaw, PRFlat, Over, &Info, FVector(0, 1, -0.1))
			&& Over.Location.Equals(FVector(0, 0, 280), 0.5));
	}
	// A row of walls: resting along the row is supported twice too, so the viewer's direction decides (away from them,
	// over the room), not a centimetre of aim.
	TArray<FGLPlacedPiece> Row = RoomNS;
	Row.Add({ 4, TEXT("buildpiece.modern.timber_foundation"), FVector(200, 0, 0), 0 });
	Row.Add({ 5, TEXT("buildpiece.modern.timber_wall"), FVector(200, -100, 30), 0 });
	FGLPlacedPiece Along;
	TestTrue(TEXT("a wall row: the floor extends away from the viewer, over the room"),
		GLStructureRules::Snap(Content, Row, TEXT("buildpiece.modern.upper_floor"), FVector(-9.6, -109.8, 264.5), 0, PRFlat, Along, &Info, FVector(0, 1, -0.1))
		&& Along.Location.Equals(FVector(0, 0, 280), 0.5));
	// A wall has one bottom socket: its snap is unchanged.
	FGLPlacedPiece Wall;
	TestTrue(TEXT("a wall on the floor's north edge"), GLStructureRules::Snap(Content, Room, TEXT("buildpiece.modern.timber_wall"), FVector(0, 90, 30), 0, PRFlat, Wall, &Info)
		&& Wall.Location.Equals(FVector(0, 100, 30), 0.5) && Info.OwnSocket == FName(TEXT("bottom")) && Info.TargetSocket == FName(TEXT("edge_n")));
	// On the ground: no snap, and the info says so.
	FGLPlacedPiece Free;
	TestTrue(TEXT("a foundation far from anything is on the ground"), GLStructureRules::Snap(Content, Room, TEXT("buildpiece.modern.timber_foundation"), FVector(2000, 2000, 0), 0, PRFlat, Free, &Info) && !Info.bSnapped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPRPlan, "Gridlands.Core.PlayableBuilding.TheConsumptionPlanIsTheConsumption", PRFlags)
bool FGLPRPlan::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = PRContent();
	const FName Stud(TEXT("item.component.stud"));
	FGLInventory CrateA(24), CrateB(24), Personal(32);
	CrateA.Add(Content, Stud, 3);
	CrateB.Add(Content, Stud, 2);
	Personal.Add(Content, Stud, 10);
	FGLMaterialPool Pool({ &CrateA, &CrateB, &Personal });
	const TArray<FGLItemStackDef> Cost = { PRStack(Stud, 7) };
	TArray<TMap<FName, int32>> Plan;
	TestTrue(TEXT("affordable"), Pool.PlanConsume(Cost, Plan));
	TestTrue(TEXT("the plan touched nothing"), CrateA.CountOf(Stud) == 3 && CrateB.CountOf(Stud) == 2 && Personal.CountOf(Stud) == 10);
	TestTrue(TEXT("planned in order: 3, 2, then 2 of yours"), Plan.Num() == 3 && Plan[0].FindRef(Stud) == 3 && Plan[1].FindRef(Stud) == 2 && Plan[2].FindRef(Stud) == 2);
	TestTrue(TEXT("consumed"), Pool.Consume(Cost));
	for (int32 I = 0; I < 3; ++I)
	{
		TestEqual(FString::Printf(TEXT("source %d gave exactly the plan"), I), Pool.LastTaken()[I].FindRef(Stud), Plan[I].FindRef(Stud));
	}
	TestFalse(TEXT("short: the plan says so"), Pool.PlanConsume({ PRStack(Stud, 99) }, Plan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPRRefusals, "Gridlands.Core.PlayableBuilding.RefusalsCarryTheirMachineReadableDetail", PRFlags)
bool FGLPRRefusals::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = PRContent();
	const TArray<FGLPlacedPiece> Post = { { 7, TEXT("buildpiece.modern.porch_post"), FVector(0, 0, 0), 0 } };
	const FGLBuildCheck Overlap = GLStructureRules::CheckPlacement(Content, Post, { 0, TEXT("buildpiece.modern.timber_foundation"), FVector(30, 30, 0), 0 }, PRFlat);
	TestTrue(TEXT("an overlap names the piece in the way"), Overlap.Refusal == EGLBuildRefusal::Overlaps && Overlap.BlockingPieceId == 7);
	FGLKnowledge Knows;
	Knows.Learn(TEXT("knowledge.style.modern_timber_frame"));
	const FGLBuildCheck Short = GLStructureRules::CanPlace(Content, {}, { 0, TEXT("buildpiece.modern.timber_foundation"), FVector(0, 0, 0), 0 }, PRFlat, Knows,
		[](FName) { return 1; });
	TestTrue(TEXT("a shortfall names the item, the need and what is there"), Short.Refusal == EGLBuildRefusal::MissingItems && Short.MissingItem == FName(TEXT("item.material.timber_plank"))
		&& Short.MissingNeeded == 2 && Short.MissingHave == 1);
	const FGLBuildCheck Ok = GLStructureRules::CheckPlacement(Content, {}, { 0, TEXT("buildpiece.modern.timber_foundation"), FVector(0, 0, 0), 0 }, PRFlat);
	TestEqual(TEXT("the material is reported (the LIMIT text names it)"), Ok.Material, FName(TEXT("material.timber.pine")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
