// P11 (ADR-0039) Building v1 rules: fine yaw through the structural lifecycle, oriented geometry, socket facings,
// the GREEN / YELLOW / RED preview, construction phases, salvage paths, material pools (shared base storage), claims,
// plans, and the v2 -> v3 save migration. Pure Core, real repository content.

#include "Building/GLClaimRules.h"
#include "Building/GLCollapseRules.h"
#include "Building/GLConstructionRules.h"
#include "Building/GLPlanRules.h"
#include "Building/GLStructureRules.h"
#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Inventory/GLInventory.h"
#include "Inventory/GLMaterialPool.h"
#include "Knowledge/GLKnowledge.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Save/GLWorldSave.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLBuildingV1Tests
{
	constexpr EAutomationTestFlags V1Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	const FGLContentRegistry& V1Content()
	{
		static FGLContentRegistry Registry;
		static bool bLoaded = Registry.LoadRepository(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		return Registry;
	}

	double V1Flat(const FVector2D&) { return 0.0; }

	const FName V1BayWall(TEXT("buildpiece.modern.bay_wall"));
	const FName V1StudWall(TEXT("buildpiece.modern.timber_wall"));
	const FName V1Floor(TEXT("buildpiece.modern.timber_foundation"));
	const FName V1LogWall(TEXT("buildpiece.modern.log_wall"));
	const FName V1Clapboard(TEXT("finish.victorian.clapboard"));
	const FName V1Stud(TEXT("item.component.stud"));
	const FName V1Scrap(TEXT("item.material.scrap_timber"));
	const FName V1Plank(TEXT("item.material.timber_plank"));

	const FGLWorldSocket* V1SocketNamed(const FGLContentRegistry& Content, const FGLPlacedPiece& Piece, const TCHAR* Name, FGLWorldSocket& Out)
	{
		const FGLBuildPieceDef* Def = Content.Find<FGLBuildPieceDef>(Piece.Def);
		for (int32 I = 0; Def && I < Def->Sockets.Num(); ++I)
		{
			if (Def->Sockets[I].Name == Name)
			{
				Out = GLStructureRules::Sockets(*Def, Piece)[I];
				return &Out;
			}
		}
		return nullptr;
	}

	/**
	 * A closed polygon built only by snapping: an angle post, then a bay wall snapped to its out socket (yaw from the
	 * data), then the next post snapped to the wall's end, Units times. Returns every piece; OutGap is the distance between
	 * the last wall's end and the first post's in socket (the polygon closes when it is ~0).
	 */
	TArray<FGLPlacedPiece> V1Polygon(FName Post, int32 Units, int32 StartYaw, const FVector& Origin, double& OutGap, TArray<int32>* OutWallYaws = nullptr)
	{
		const FGLContentRegistry& Content = V1Content();
		TArray<FGLPlacedPiece> Pieces;
		FGLPlacedPiece First;
		First.Id = 1;
		First.Def = Post;
		First.Location = Origin;
		First.YawStep = StartYaw;
		Pieces.Add(First);
		int32 NextId = 2;
		for (int32 Unit = 0; Unit < Units; ++Unit)
		{
			FGLWorldSocket Out;
			V1SocketNamed(Content, Pieces.Last(), TEXT("out"), Out);
			FGLPlacedPiece Wall;
			if (!GLStructureRules::Snap(Content, Pieces, V1BayWall, Out.Location, 0, V1Flat, Wall))
			{
				break;
			}
			Wall.Id = NextId++;
			Pieces.Add(Wall);
			if (OutWallYaws)
			{
				OutWallYaws->Add(Wall.YawStep);
			}
			if (Unit == Units - 1)
			{
				break;
			}
			FGLWorldSocket End;
			V1SocketNamed(Content, Wall, TEXT("right"), End);
			FGLPlacedPiece Next;
			if (!GLStructureRules::Snap(Content, Pieces, Post, End.Location, 0, V1Flat, Next))
			{
				break;
			}
			Next.Id = NextId++;
			Pieces.Add(Next);
		}
		FGLWorldSocket LastEnd, FirstIn;
		V1SocketNamed(Content, Pieces.Last(), TEXT("right"), LastEnd);
		V1SocketNamed(Content, Pieces[0], TEXT("in"), FirstIn);
		OutGap = FVector::Dist(LastEnd.Location, FirstIn.Location);
		return Pieces;
	}
}

using namespace GLBuildingV1Tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1YawUnits, "Gridlands.Core.BuildingV1.YawIsAnIntegerIn2p5DegreeSteps", V1Flags)
bool FGLV1YawUnits::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("144 steps per turn"), GLStructureRules::YawSteps, 144);
	TestEqual(TEXT("90 = 36 (legacy quarter turns map exactly)"), GLStructureRules::YawStepFromDegrees(90.0), 36);
	TestEqual(TEXT("60 = 24 (hexagons)"), GLStructureRules::YawStepFromDegrees(60.0), 24);
	TestEqual(TEXT("45 = 18 (bays, octagons)"), GLStructureRules::YawStepFromDegrees(45.0), 18);
	TestEqual(TEXT("22.5 = 9 (16-sided towers)"), GLStructureRules::YawStepFromDegrees(22.5), 9);
	TestEqual(TEXT("-45 wraps to 315"), GLStructureRules::YawStepFromDegrees(-45.0), 126);
	TestTrue(TEXT("2.5 is a whole step"), GLStructureRules::IsWholeYawStep(7.5));
	TestFalse(TEXT("37.3 is not (no freeform yaw)"), GLStructureRules::IsWholeYawStep(37.3));
	TestTrue(TEXT("a quarter turn is bit-exact"), GLStructureRules::RotateXY(FVector2D(100.0, 0.0), 36) == FVector2D(0.0, 100.0));
	const FVector2D Diagonal = GLStructureRules::RotateXY(FVector2D(100.0, 0.0), 18);
	TestTrue(TEXT("45 degrees rotates by 45 degrees"), Diagonal.Equals(FVector2D(70.7106781, 70.7106781), 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Polygons, "Gridlands.Core.BuildingV1.AngledPolygonsCloseBySnappingAlone", V1Flags)
bool FGLV1Polygons::RunTest(const FString& Parameters)
{
	// Octagon (45), hexagon (60), 16-sided tower (22.5): every wall's yaw comes from the socket data, the polygon closes
	// exactly, nothing overlaps, and every wall stands (supported laterally by its posts).
	struct FCase { const TCHAR* Post; int32 Units; int32 Turn; };
	for (const FCase& Case : { FCase{ TEXT("buildpiece.modern.angle_post_l45"), 8, 18 }, FCase{ TEXT("buildpiece.modern.angle_post_l60"), 6, 24 },
		FCase{ TEXT("buildpiece.modern.angle_post_l225"), 16, 9 } })
	{
		double Gap = 1e9;
		TArray<int32> WallYaws;
		const TArray<FGLPlacedPiece> Pieces = V1Polygon(Case.Post, Case.Units, 0, FVector(1000, 2000, 0), Gap, &WallYaws);
		TestEqual(FString::Printf(TEXT("%s: %d posts and %d walls"), Case.Post, Case.Units, Case.Units), Pieces.Num(), Case.Units * 2);
		TestTrue(FString::Printf(TEXT("%s: the polygon closes (gap %.4f cm)"), Case.Post, Gap), Gap < GLStructureRules::SocketToleranceCm);
		for (int32 I = 0; I < WallYaws.Num(); ++I)
		{
			TestEqual(FString::Printf(TEXT("%s: wall %d turned exactly by the data"), Case.Post, I), WallYaws[I], GLStructureRules::NormalizeYawStep(Case.Turn * (I + 1)));
		}
		const TMap<int32, double> Support = GLStructureRules::ComputeSupport(V1Content(), Pieces, V1Flat);
		for (const FGLPlacedPiece& Piece : Pieces)
		{
			TestTrue(FString::Printf(TEXT("%s: piece %d stands (support %.2f)"), Case.Post, Piece.Id, Support.FindRef(Piece.Id)), Support.FindRef(Piece.Id) > 0.0);
		}
		for (int32 I = 0; I < Pieces.Num(); ++I)
		{
			TArray<FGLPlacedPiece> Others = Pieces;
			Others.RemoveAt(I);
			TestNotEqual(FString::Printf(TEXT("%s: piece %d overlaps nothing"), Case.Post, Pieces[I].Id),
				GLStructureRules::CheckPlacement(V1Content(), Others, Pieces[I], V1Flat).Refusal, EGLBuildRefusal::Overlaps);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Oriented, "Gridlands.Core.BuildingV1.OverlapAndFootprintsAreOriented", V1Flags)
bool FGLV1Oriented::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = V1Content();
	const FGLBuildPieceDef* Wall = Content.Find<FGLBuildPieceDef>(V1StudWall);
	// Two 45 degree walls side by side, 40 cm apart across their faces: their enclosing boxes overlap heavily, the walls do not.
	FGLPlacedPiece A{ 1, V1StudWall, FVector(0, 0, 0), 18 };
	const FVector2D Across = GLStructureRules::RotateXY(FVector2D(0.0, 40.0), 18);
	FGLPlacedPiece B{ 2, V1StudWall, FVector(Across, 0.0), 18 };
	TestTrue(TEXT("their enclosing boxes intersect"), GLStructureRules::Bounds(*Wall, A).Intersect(GLStructureRules::Bounds(*Wall, B)));
	TestFalse(TEXT("but the oriented walls do not overlap"), GLStructureRules::Footprint(*Wall, A).Overlaps(GLStructureRules::Footprint(*Wall, B), GLStructureRules::OverlapShrinkCm));
	FGLPlacedPiece C{ 3, V1StudWall, FVector(0, 0, 0), 0 }; // crossing A at 45 degrees
	TestTrue(TEXT("a wall crossing it at 45 degrees does overlap"), GLStructureRules::Footprint(*Wall, A).Overlaps(GLStructureRules::Footprint(*Wall, C), GLStructureRules::OverlapShrinkCm));
	// Terrain protection follows the oriented footprint of a grounded piece (a floor at 45 degrees).
	FGLPlacedPiece Floor{ 4, V1Floor, FVector(5000, 0, 0), 18 };
	const TArray<FGLPlacedPiece> Grounded = { Floor };
	TestTrue(TEXT("its centre is protected"), GLStructureRules::IsUnderStructure(Content, Grounded, FVector2D(5000, 0), 0.0));
	TestTrue(TEXT("a point inside its rotated square is protected"), GLStructureRules::IsUnderStructure(Content, Grounded, FVector2D(5000, 130), 0.0));
	TestFalse(TEXT("a corner of its enclosing box is not (it is outside the rotated square)"), GLStructureRules::IsUnderStructure(Content, Grounded, FVector2D(5130, 130), 0.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Facings, "Gridlands.Core.BuildingV1.FacingSocketsLinkOnlyWhenTheyFaceEachOther", V1Flags)
bool FGLV1Facings::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = V1Content();
	FGLPlacedPiece Post{ 1, TEXT("buildpiece.modern.angle_post_l45"), FVector(0, 0, 0), 0 };
	FGLWorldSocket Out;
	V1SocketNamed(Content, Post, TEXT("out"), Out);
	FGLPlacedPiece Snapped;
	TestTrue(TEXT("a bay wall snaps to the post's out socket"), GLStructureRules::Snap(Content, { Post }, V1BayWall, Out.Location, 0, V1Flat, Snapped));
	TestEqual(TEXT("taking its yaw from the data (45), not from the requested yaw"), Snapped.YawStep, 18);
	Snapped.Id = 2;
	TestTrue(TEXT("it stands, held by the post"), GLStructureRules::ComputeSupport(Content, { Post, Snapped }, V1Flat).FindRef(2) > 0.0);
	// The same wall at the same socket point but turned a quarter turn: the facings do not face each other, so no link.
	FGLPlacedPiece Wrong = Snapped;
	Wrong.YawStep = GLStructureRules::NormalizeYawStep(Snapped.YawStep + 36);
	Wrong.Location = Out.Location - (GLStructureRules::ToWorld(Wrong, { 0.5, 0, 1.25 }) - Wrong.Location); // its right socket on the point
	FGLWorldSocket WrongRight;
	V1SocketNamed(Content, Wrong, TEXT("right"), WrongRight);
	TestTrue(TEXT("its socket coincides with the post's"), FVector::Dist(WrongRight.Location, Out.Location) < 0.01);
	TestTrue(TEXT("but it does not link, so nothing holds it"), GLStructureRules::ComputeSupport(Content, { Post, Wrong }, V1Flat).FindRef(2) <= 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Preview, "Gridlands.Core.BuildingV1.PreviewIsTheCommitsOwnRule", V1Flags)
bool FGLV1Preview::RunTest(const FString& Parameters)
{
	// Pine: strength 3, maxStack 4 (step 0.75). Floor 3, walls 2.25, 1.5, 0.75 (YELLOW: at its limit), a 4th refused (RED).
	const FGLContentRegistry& Content = V1Content();
	TArray<FGLPlacedPiece> Stack = { { 1, V1Floor, FVector(0, 0, 0) } };
	const FGLBuildCheck Floor = GLStructureRules::CheckPlacement(Content, {}, Stack[0], V1Flat);
	TestEqual(TEXT("floor on the ground: GREEN"), Floor.Preview, EGLPreview::Green);
	const EGLPreview Expected[] = { EGLPreview::Green, EGLPreview::Green, EGLPreview::Yellow, EGLPreview::Red };
	for (int32 Storey = 0; Storey < 4; ++Storey)
	{
		const FGLPlacedPiece Wall{ 10 + Storey, V1StudWall, FVector(0, -100, 30 + 250 * Storey) };
		const FGLBuildCheck Check = GLStructureRules::CheckPlacement(Content, Stack, Wall, V1Flat);
		TestEqual(FString::Printf(TEXT("wall %d (support %.2f, step %.2f)"), Storey + 1, Check.Support, Check.VerticalStep), Check.Preview, Expected[Storey]);
		TestEqual(TEXT("the colour is exactly the check's (PreviewOf)"), GLStructureRules::PreviewOf(Check), Check.Preview);
		TestEqual(TEXT("RED exactly when refused"), Check.Preview == EGLPreview::Red, !Check.IsAllowed());
		if (Check.IsAllowed())
		{
			Stack.Add(Wall);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Phases, "Gridlands.Core.BuildingV1.FrameThenFinishAndElectricalIsRegisteredNotBuilt", V1Flags)
bool FGLV1Phases::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = V1Content();
	FGLPlacedPiece Wall{ 1, V1StudWall, FVector(0, -100, 30) };
	const FGLPlacedPiece Floor{ 2, V1Floor, FVector(0, 0, 0) };
	TestTrue(TEXT("a placed stud wall shows its frame"), GLConstructionRules::ShowsFrame(Content, Wall));
	TestTrue(TEXT("clapboard goes on a stud wall"), GLConstructionRules::CanInstall(Content, Wall, V1Clapboard).IsAllowed());
	const double Before = GLStructureRules::ComputeSupport(Content, { Floor, Wall }, V1Flat).FindRef(1);
	const FGLPlacedPiece Finished = GLConstructionRules::WithLayer(Content, Wall, V1Clapboard);
	TestFalse(TEXT("finished, it no longer shows its frame"), GLConstructionRules::ShowsFrame(Content, Finished));
	TestEqual(TEXT("a finish never changes support"), GLStructureRules::ComputeSupport(Content, { Floor, Finished }, V1Flat).FindRef(1), Before);
	TestEqual(TEXT("one finish per piece"), GLConstructionRules::CanInstall(Content, Finished, TEXT("finish.modern.timber_board_wall")).Refusal, EGLInstallRefusal::AlreadyInstalled);
	TestEqual(TEXT("a log wall is complete as built"), GLConstructionRules::CanInstall(Content, { 3, V1LogWall, FVector::ZeroVector }, V1Clapboard).Refusal, EGLInstallRefusal::FormRefusesPhase);
	TestEqual(TEXT("shingles do not fit a wall"), GLConstructionRules::CanInstall(Content, Wall, TEXT("finish.modern.shingles")).Refusal, EGLInstallRefusal::RoleMismatch);
	const FGLPhaseDef* Electrical = Content.Find<FGLPhaseDef>(TEXT("phase.construction.electrical"));
	const FGLPhaseDef* Finish = Content.Find<FGLPhaseDef>(TEXT("phase.construction.finish"));
	TestTrue(TEXT("electrical is registered, before finish, and not implemented"), Electrical && Finish && Electrical->Order < Finish->Order && !Electrical->Implemented);
	TestTrue(TEXT("a stud wall already accepts electrical in canonical order (no schema change to add it)"),
		Content.Find<FGLBuildPieceDef>(V1StudWall)->Layers == TArray<FName>({ TEXT("phase.construction.electrical"), TEXT("phase.construction.finish") }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1SalvagePaths, "Gridlands.Core.BuildingV1.CarefulBeatsDestructiveBeatsCollapse", V1Flags)
bool FGLV1SalvagePaths::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = V1Content();
	const FGLPlacedPiece Wall = GLConstructionRules::WithLayer(Content, { 1, V1StudWall, FVector::ZeroVector }, V1Clapboard);
	auto Count = [](const TArray<FGLSalvageYieldDef>& Yields, FName Item) { int32 N = 0; for (const FGLSalvageYieldDef& Y : Yields) { N += Y.Item == Item ? Y.Count : 0; } return N; };
	const TArray<FGLSalvageYieldDef> Careful = GLConstructionRules::PieceYields(Content, Wall, EGLSalvagePath::Careful);
	const TArray<FGLSalvageYieldDef> Destructive = GLConstructionRules::PieceYields(Content, Wall, EGLSalvagePath::Destructive);
	const TArray<FGLSalvageYieldDef> Collapse = GLConstructionRules::PieceYields(Content, Wall, EGLSalvagePath::Collapse);
	TestTrue(TEXT("careful dismantling returns intact studs"), Count(Careful, V1Stud) >= 5);
	TestTrue(TEXT("smashing returns fewer studs"), Count(Destructive, V1Stud) < Count(Careful, V1Stud));
	TestEqual(TEXT("a collapse returns no pristine studs"), Count(Collapse, V1Stud), 0);
	TestTrue(TEXT("but scrap"), Count(Collapse, V1Scrap) > Count(Careful, V1Scrap));
	TestTrue(TEXT("the finish comes back by the same path (boards when careful)"), Count(Careful, V1Plank) > Count(Collapse, V1Plank));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Pool, "Gridlands.Core.BuildingV1.MaterialPoolIsOrderedAndAllOrNothing", V1Flags)
bool FGLV1Pool::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = V1Content();
	FGLInventory Near(4), Far(4), Personal(4);
	Near.Add(Content, V1Stud, 3);
	Far.Add(Content, V1Stud, 10);
	Personal.Add(Content, V1Stud, 10);
	FGLMaterialPool Pool({ &Near, &Far, &Personal }, { 2, 0, 1 });
	TestTrue(TEXT("6 studs are taken"), Pool.Consume({ { V1Stud, 6 } }));
	TestEqual(TEXT("the nearest storage first"), Near.CountOf(V1Stud), 0);
	TestEqual(TEXT("then the next storage"), Far.CountOf(V1Stud), 7);
	TestEqual(TEXT("Zenny's own studs untouched while storage has them"), Personal.CountOf(V1Stud), 10);
	TestFalse(TEXT("more than everything: refused"), Pool.Consume({ { V1Stud, 18 } }));
	TestEqual(TEXT("and nothing was taken (all or nothing)"), Far.CountOf(V1Stud) + Personal.CountOf(V1Stud), 17);
	TestFalse(TEXT("a cost with one missing item: refused"), Pool.Consume({ { V1Stud, 1 }, { V1Plank, 1 } }));
	TestEqual(TEXT("nothing taken of the item that was there"), Far.CountOf(V1Stud), 7);
	TestTrue(TEXT("delivery: Zenny first"), Pool.Deliver(Content, { { V1Plank, 5 } }));
	TestEqual(TEXT("into Zenny's inventory"), Personal.CountOf(V1Plank), 5);
	TestFalse(TEXT("more than all of them hold: refused"), Pool.Deliver(Content, { { V1Scrap, 100000 } }));
	TestEqual(TEXT("and nothing delivered"), Near.CountOf(V1Scrap) + Far.CountOf(V1Scrap) + Personal.CountOf(V1Scrap), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Claims, "Gridlands.Core.BuildingV1.ClaimsAreDerivedAndRenewalNeverTouchesPlayerConstruction", V1Flags)
bool FGLV1Claims::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = V1Content();
	FGLPlacedPiece Core{ 7, TEXT("buildpiece.modern.base_core"), FVector(1000, 1000, 0), 0, TEXT("cell.test"), EGLPieceOrigin::Player };
	FGLPlacedPiece Shed{ 8, V1Floor, FVector(1200, 1000, 0), 0, TEXT("cell.test"), EGLPieceOrigin::Player };
	const TArray<FGLClaim> Claims = GLClaimRules::ClaimsFrom(Content, { Core, Shed }, 3200.0);
	TestEqual(TEXT("one base core: one claim"), Claims.Num(), 1);
	TestTrue(TEXT("its area holds the shed"), Claims[0].Contains(FVector2D(1200, 1000)));
	TestFalse(TEXT("not far away"), Claims[0].Contains(FVector2D(10000, 1000)));
	FGLPlacedPiece Authored = Core;
	Authored.Origin = EGLPieceOrigin::Authored;
	TestEqual(TEXT("an authored core makes no claim (claims are player construction)"), GLClaimRules::ClaimsFrom(Content, { Authored }, 3200.0).Num(), 0);
	TestFalse(TEXT("renewal never touches player construction, anywhere"), GLClaimRules::MayRenew(EGLPieceOrigin::Player, FVector2D(90000, 90000), Claims));
	TestFalse(TEXT("nor authored content inside a claim"), GLClaimRules::MayRenew(EGLPieceOrigin::Authored, FVector2D(1500, 1000), Claims));
	TestTrue(TEXT("authored content outside every claim is renewable"), GLClaimRules::MayRenew(EGLPieceOrigin::Authored, FVector2D(90000, 90000), Claims));
	FGLPlacedPiece Neighbour = Core;
	Neighbour.Id = 9;
	Neighbour.Location = FVector(4000, 1000, 0);
	TestTrue(TEXT("two cores 30 m apart overlap at 32 m (P11 refuses that placement)"),
		GLClaimRules::ClaimsFrom(Content, { Core, Neighbour }, 3200.0)[0].Overlaps(GLClaimRules::ClaimsFrom(Content, { Core, Neighbour }, 3200.0)[1]));
	FGLClaim Estate = Claims[0];
	Estate.Areas.Add({ FVector2D(7000, 1000), 3200.0, 99 });
	TestTrue(TEXT("a claim is a set of areas (room for expansion, estates): it holds both"), Estate.Contains(FVector2D(1000, 1000)) && Estate.Contains(FVector2D(9000, 1000)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Plan, "Gridlands.Core.BuildingV1.APlanRebuiltAt45DegreesIsTheSameBuilding", V1Flags)
bool FGLV1Plan::RunTest(const FString& Parameters)
{
	// P12 compatibility: a PLAN is ordinary pieces relative to an anchor. Rebuilt at 45 degrees elsewhere, it must give the
	// same relative geometry, the same socket relationships and the same support: never a second building representation.
	const FGLContentRegistry& Content = V1Content();
	double Gap = 0.0;
	TArray<FGLPlacedPiece> Octagon = V1Polygon(TEXT("buildpiece.modern.angle_post_l45"), 8, 0, FVector(0, 0, 0), Gap);
	Octagon.Add({ 100, V1Floor, FVector(-400, -400, 0) });
	Octagon.Add({ 101, V1StudWall, FVector(-400, -500, 30) });
	const FGLPlan Plan = GLPlanRules::Capture(Octagon, FVector(0, 0, 0), 0);
	const TArray<FGLPlacedPiece> Rebuilt = GLPlanRules::Instantiate(Plan, FVector(50000, -20000, 0), 18, 1, EGLPieceOrigin::Player);
	TestEqual(TEXT("every piece"), Rebuilt.Num(), Octagon.Num());
	for (int32 A = 0; A < Octagon.Num(); ++A)
	{
		TestEqual(TEXT("the same piece"), Rebuilt[A].Def, Octagon[A].Def);
		TestEqual(TEXT("turned by exactly 45 degrees"), Rebuilt[A].YawStep, GLStructureRules::NormalizeYawStep(Octagon[A].YawStep + 18));
		for (int32 B = A + 1; B < Octagon.Num(); ++B)
		{
			TestTrue(TEXT("the same relative geometry"), FMath::IsNearlyEqual(FVector::Dist(Rebuilt[A].Location, Rebuilt[B].Location), FVector::Dist(Octagon[A].Location, Octagon[B].Location), 0.01));
		}
	}
	const TMap<int32, double> Original = GLStructureRules::ComputeSupport(Content, Octagon, V1Flat);
	const TMap<int32, double> Again = GLStructureRules::ComputeSupport(Content, Rebuilt, V1Flat);
	for (int32 I = 0; I < Octagon.Num(); ++I)
	{
		TestTrue(FString::Printf(TEXT("piece %d: the same support (%.4f / %.4f)"), I, Original.FindRef(Octagon[I].Id), Again.FindRef(Rebuilt[I].Id)),
			FMath::IsNearlyEqual(Original.FindRef(Octagon[I].Id), Again.FindRef(Rebuilt[I].Id), 1e-6));
		TArray<FGLPlacedPiece> WithoutOriginal = Octagon, WithoutRebuilt = Rebuilt;
		WithoutOriginal.RemoveAt(I);
		WithoutRebuilt.RemoveAt(I);
		TestEqual(TEXT("the same socket relationships (removing it drops the same pieces)"),
			GLStructureRules::CollapsesAfterRemoving(Content, Octagon, Octagon[I].Id, V1Flat).Num(), GLStructureRules::CollapsesAfterRemoving(Content, Rebuilt, Rebuilt[I].Id, V1Flat).Num());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1CollapseAtAngle, "Gridlands.Core.BuildingV1.ACollapseAtAngleUsesThePiecesOwnAxes", V1Flags)
bool FGLV1CollapseAtAngle::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = V1Content();
	const FGLCollapseTuningDef Tuning = Content.Find<FGLTuningDef>(TEXT("tuning.world.physical"))->Collapse;
	const FGLPlacedPiece Roof{ 1, TEXT("buildpiece.modern.porch_roof"), FVector(0, 0, 280), 18 };
	const FGLCollapsePlan Plan = GLCollapseRules::Plan(Content, { { Roof, EGLCollapseMotion::Drop } }, {}, V1Flat, FVector(0, -1000, 0), Tuning);
	TestEqual(TEXT("one outcome"), Plan.Outcomes.Num(), 1);
	const FGLImpactVolume& Impact = Plan.Outcomes[0].Impact;
	TestTrue(TEXT("the drop's impact volume is on the piece's 45 degree axes"), Impact.Axis[0].Equals(FVector(FVector2D(0.7071068, 0.7071068), 0.0), 1e-5));
	const FVector Corner(GLStructureRules::RotateXY(FVector2D(95, 95), 18), 50.0);
	TestTrue(TEXT("a point under its rotated corner is hit"), Impact.Touches(Corner, 1.0));
	TestFalse(TEXT("a point under its enclosing box's corner is not"), Impact.Touches(FVector(130, 130, 50), 1.0));
	TestTrue(TEXT("it rests at the same yaw"), Plan.Outcomes[0].Rest.Rotator().Yaw == 45.0 || FMath::IsNearlyEqual(Plan.Outcomes[0].Rest.Rotator().Yaw, 45.0, 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1SaveMigration, "Gridlands.Core.BuildingV1.Version2SavesMigrateToVersion3Deterministically", V1Flags)
bool FGLV1SaveMigration::RunTest(const FString& Parameters)
{
	// A representative v2 file: v0 player pieces (a quarter-turned timber wall, a floor, a roman wall), inventory, a terrain
	// edit, authored debris, a collapse in flight and a creature: only the pieces may change, and deterministically.
	const FString V2 = TEXT(R"({"schemaVersion":2,"cell":"cell.home.origin","inventory":[{"id":"item.material.timber_plank","count":250}],
		"nextPieceId":4,"cells":[{"cell":"cell.home.origin",
		"buildPieces":[{"id":1,"def":"buildpiece.modern.timber_foundation","location":{"x":100,"y":200,"z":0},"yawQuarter":0},
		               {"id":2,"def":"buildpiece.modern.timber_wall","location":{"x":200,"y":200,"z":30},"yawQuarter":1},
		               {"id":3,"def":"buildpiece.roman.masonry_wall","location":{"x":0,"y":200,"z":30},"yawQuarter":3}],
		"terrainIndices":[42],"terrainDeltaCm":[-50],
		"structureParts":[{"placement":"placement.origin.structure_carport","part":"deck_west","state":2,"location":{"x":1,"y":2,"z":3},"rotation":{"pitch":0,"yaw":90,"roll":0}}],
		"collapses":[{"placement":"placement.origin.structure_carport","part":"deck_east","elapsedSeconds":0.25,"impactSeconds":1.0}],
		"creatures":[{"placement":"placement.lots.spawn_gremlin","state":2,"health":40}]}]})");
	FGLWorldSave Save;
	FString Problem;
	TestTrue(TEXT("a v2 save loads"), GLSaveCodec::FromJson(V2, Save, Problem, &V1Content()));
	TestEqual(TEXT("as the current version"), Save.SchemaVersion, 3);
	const FGLSavedCell* Cell = Save.FindCell(TEXT("cell.home.origin"));
	TestTrue(TEXT("its cell record"), Cell != nullptr);
	if (!Cell)
	{
		return false;
	}
	TestEqual(TEXT("every piece kept"), Cell->BuildPieces.Num(), 3);
	TestEqual(TEXT("yawQuarter 1 -> yawStep 36"), Cell->BuildPieces[1].YawStep, 36);
	TestEqual(TEXT("yawQuarter 3 -> yawStep 108"), Cell->BuildPieces[2].YawStep, 108);
	TestEqual(TEXT("every v2 piece was player-built"), Cell->BuildPieces[0].Origin, static_cast<uint8>(EGLPieceOrigin::Player));
	TestEqual(TEXT("the v0 timber wall becomes FRAME + its old look as a finish"), Cell->BuildPieces[1].Layers, TArray<FName>{ TEXT("finish.modern.timber_board_wall") });
	TestEqual(TEXT("a floor is complete as built"), Cell->BuildPieces[0].Layers.Num(), 0);
	TestEqual(TEXT("a roman wall too"), Cell->BuildPieces[2].Layers.Num(), 0);
	TestEqual(TEXT("inventory untouched"), Save.Inventory.Num() == 1 ? Save.Inventory[0].Count : -1, 250);
	TestEqual(TEXT("terrain edits untouched"), Cell->TerrainDeltaCm, TArray<int32>{ -50 });
	TestEqual(TEXT("authored debris untouched"), Cell->StructureParts.Num(), 1);
	TestEqual(TEXT("the collapse in flight untouched"), Cell->Collapses.Num() == 1 ? Cell->Collapses[0].ElapsedSeconds : -1.0, 0.25);
	TestEqual(TEXT("encounter state untouched"), Cell->Creatures.Num() == 1 ? Cell->Creatures[0].Health : -1.0, 40.0);
	// Re-save: canonical v3, and loading it again is a fixed point.
	const FString V3 = GLSaveCodec::ToJson(Save);
	TestTrue(TEXT("re-saved as version 3"), V3.Contains(TEXT("\"schemaVersion\": 3")) || V3.Contains(TEXT("\"schemaVersion\":3")));
	TestFalse(TEXT("no yawQuarter left"), V3.Contains(TEXT("yawQuarter")));
	FGLWorldSave Again;
	TestTrue(TEXT("the v3 file loads"), GLSaveCodec::FromJson(V3, Again, Problem, &V1Content()));
	TestEqual(TEXT("and re-saves identically (deterministic)"), GLSaveCodec::ToJson(Again), V3);
	FGLWorldSave Twice;
	GLSaveCodec::FromJson(V2, Twice, Problem, &V1Content());
	TestEqual(TEXT("migrating the same v2 file twice gives the same save"), GLSaveCodec::ToJson(Twice), V3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
