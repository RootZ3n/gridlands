// Deterministic structural collapse (P6, ADR-0030): who falls, how, where it rests, what it hits.

#include "Building/GLCollapseRules.h"
#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLCollapseTests
{
	constexpr EAutomationTestFlags CollapseFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	const FGLContentRegistry& CollapseContent()
	{
		static FGLContentRegistry Registry;
		static bool bLoaded = Registry.LoadRepository(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		return Registry;
	}

	const FName CPost(TEXT("buildpiece.modern.timber_post"));
	const FName CDeck(TEXT("buildpiece.modern.timber_deck"));
	const FName CStump(TEXT("buildpiece.nature.pine_stump"));
	const FName CTrunk(TEXT("buildpiece.nature.pine_trunk"));

	double CFlat(const FVector2D&) { return 0.0; }

	/** The P6 carport: posts 1 (south) and 2 (north) hold deck 3; deck 4 hangs off deck 3 sideways. */
	TArray<FGLPlacedPiece> Carport()
	{
		return {
			{ 1, CPost, FVector(-100, -100, 0) },
			{ 2, CPost, FVector(-100, 100, 0) },
			{ 3, CDeck, FVector(0, 0, 250) },
			{ 4, CDeck, FVector(200, 0, 250) },
		};
	}

	FGLCollapseTuningDef Tuning()
	{
		FGLCollapseTuningDef T;
		T.Gravity = 9.81;
		T.StartDelaySeconds = 0.3;
		T.ImpactMarginMetres = 0.3;
		T.ImpactHeightMetres = 2.0;
		T.DamageBase = 10.0;
		T.DamagePerMetreFallen = 20.0;
		T.DamageMax = 150.0;
		T.ToppleStartDegrees = 5.0;
		return T;
	}

	TArray<FGLPlacedPiece> Without(TArray<FGLPlacedPiece> Pieces, TArray<int32> Ids)
	{
		Pieces.RemoveAll([&Ids](const FGLPlacedPiece& P) { return Ids.Contains(P.Id); });
		return Pieces;
	}

	TArray<FGLCollapseRequest> Requests(const TArray<FGLPlacedPiece>& Pieces, const TArray<int32>& Ids, EGLCollapseMotion Motion = EGLCollapseMotion::Drop)
	{
		TArray<FGLCollapseRequest> Out;
		for (const FGLPlacedPiece& P : Pieces)
		{
			if (Ids.Contains(P.Id))
			{
				Out.Add({ P, Motion });
			}
		}
		return Out;
	}
}

using GLCollapseTests::CollapseContent;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCollapseWhoFalls, "Gridlands.Core.Structure.OnlyWhatLosesSupportFallsAndItCascades", GLCollapseTests::CollapseFlags)
bool FGLCollapseWhoFalls::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = CollapseContent();
	const TArray<FGLPlacedPiece> All = GLCollapseTests::Carport();
	TestEqual(TEXT("the whole carport stands"), GLCollapseRules::Unsupported(Content, All, GLCollapseTests::CFlat).Num(), 0);
	TestEqual(TEXT("either post alone holds it (redundancy)"), GLCollapseRules::Unsupported(Content, GLCollapseTests::Without(All, { 1 }), GLCollapseTests::CFlat).Num(), 0);
	const TArray<int32> Both = GLCollapseRules::Unsupported(Content, GLCollapseTests::Without(All, { 1, 2 }), GLCollapseTests::CFlat);
	TestEqual(TEXT("the last post gone: both decks fall (the second only hung off the first)"), Both, TArray<int32>({ 3, 4 }));
	// Ground that drops away under a post takes its support with it (support is ADR-0024's, derived).
	const TArray<int32> Undermined = GLCollapseRules::Unsupported(Content, GLCollapseTests::Without(All, { 1 }),
		[](const FVector2D& At) { return At.Y > 0.0 ? -200.0 : 0.0; });
	TestTrue(TEXT("a post off the ground holds nothing"), Undermined.Contains(2) && Undermined.Contains(3) && Undermined.Contains(4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCollapseDrop, "Gridlands.Core.Structure.DropIsDeterministicAndItsImpactIsTruthful", GLCollapseTests::CollapseFlags)
bool FGLCollapseDrop::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = CollapseContent();
	const TArray<FGLPlacedPiece> All = GLCollapseTests::Carport();
	const TArray<FGLCollapseRequest> Falling = GLCollapseTests::Requests(All, { 3, 4 });
	const FGLCollapsePlan Plan = GLCollapseRules::Plan(Content, Falling, {}, GLCollapseTests::CFlat, FVector(-100, 100, 0), GLCollapseTests::Tuning());
	if (!TestEqual(TEXT("both decks planned"), Plan.Outcomes.Num(), 2))
	{
		return false;
	}
	const FGLCollapseOutcome& West = Plan.Outcomes[0];
	TestEqual(TEXT("it rests on the ground"), West.Rest.GetLocation().Z, 0.0, 0.01);
	TestEqual(TEXT("fell 2.5 m"), West.FallMetres, 2.5, 1e-6);
	TestEqual(TEXT("hits after the start delay plus a free fall: 0.3 + sqrt(2 x 2.5 / 9.81)"), West.ImpactSeconds, 0.3 + FMath::Sqrt(2.0 * 2.5 / 9.81), 1e-6);
	TestEqual(TEXT("damage = base + per metre x fall (provisional data)"), West.Damage, 10.0 + 20.0 * 2.5, 1e-6);
	// The impact is where it lands: under the deck is hit, beside it (past the margin) is not.
	TestTrue(TEXT("someone standing under it is hit"), West.Impact.Touches(FVector(-100, 100, 90), 40.0));
	TestFalse(TEXT("someone 1.5 m beside it is not"), West.Impact.Touches(FVector(-100, 100 + 100 + 30 + 150 + 40, 90), 40.0));
	// Presentation follows: the pose starts where it was, ends at rest, and is between on the way.
	TestTrue(TEXT("before it starts it has not moved"), GLCollapseRules::Motion(West, 0.1).GetLocation().Equals(West.Start.GetLocation()));
	TestTrue(TEXT("after impact it is at rest"), GLCollapseRules::Motion(West, 5.0).GetLocation().Equals(West.Rest.GetLocation()));
	const double Mid = GLCollapseRules::Motion(West, (West.StartSeconds + West.ImpactSeconds) * 0.5).GetLocation().Z;
	TestTrue(TEXT("on the way down it is between"), Mid < 250.0 && Mid > 0.0);
	// Deterministic: the same inputs give the same plan.
	const FGLCollapsePlan Again = GLCollapseRules::Plan(Content, Falling, {}, GLCollapseTests::CFlat, FVector(-100, 100, 0), GLCollapseTests::Tuning());
	TestTrue(TEXT("identical plan"), Again.Outcomes[1].Rest.Equals(Plan.Outcomes[1].Rest) && Again.Outcomes[1].ImpactSeconds == Plan.Outcomes[1].ImpactSeconds);
	// Stacking: a deck above another lands on the lower one's rest, not through it.
	const TArray<FGLPlacedPiece> Stack = { { 5, GLCollapseTests::CDeck, FVector(0, 0, 250) }, { 6, GLCollapseTests::CDeck, FVector(0, 0, 500) } };
	const FGLCollapsePlan Stacked = GLCollapseRules::Plan(Content, GLCollapseTests::Requests(Stack, { 5, 6 }), {}, GLCollapseTests::CFlat, FVector::ZeroVector, GLCollapseTests::Tuning());
	TestEqual(TEXT("the upper deck lands on the lower (20 cm up)"), Stacked.Outcomes[1].Rest.GetLocation().Z, 20.0, 0.01);
	// Something that stays standing catches what falls onto it.
	const TArray<FGLPlacedPiece> Low = { { 7, GLCollapseTests::CPost, FVector(200, 0, 0) } };
	const FGLCollapsePlan Caught = GLCollapseRules::Plan(Content, GLCollapseTests::Requests(All, { 4 }), Low, GLCollapseTests::CFlat, FVector::ZeroVector, GLCollapseTests::Tuning());
	TestEqual(TEXT("it lands on the standing post (2.5 m): no fall, no damage"), Caught.Outcomes[0].Damage, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCollapseTopple, "Gridlands.Core.Structure.ToppleFollowsItsDirectionPolicy", GLCollapseTests::CollapseFlags)
bool FGLCollapseTopple::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = CollapseContent();
	const FGLPlacedPiece Trunk{ 1, GLCollapseTests::CTrunk, FVector(0, 0, 40) };
	FGLCollapseRequest Request{ Trunk, EGLCollapseMotion::Topple, EGLToppleDirection::AwayFromInstigator };
	// Zenny chops from the west (-X): the trunk falls east (+X).
	const FGLCollapsePlan Plan = GLCollapseRules::Plan(Content, { Request }, {}, GLCollapseTests::CFlat, FVector(-300, 0, 0), GLCollapseTests::Tuning());
	const FGLCollapseOutcome& Fell = Plan.Outcomes[0];
	const FVector Up = Fell.Rest.GetRotation().GetUpVector();
	TestTrue(TEXT("it lies along the fall direction (its up now points east)"), Up.Equals(FVector(1, 0, 0), 0.01));
	TestTrue(TEXT("lying on the ground"), GLCollapseRules::Motion(Fell, 60.0).GetLocation().Z > -1.0);
	TestTrue(TEXT("its impact covers where the top lands (7 m east)"), Fell.Impact.Touches(FVector(700, 0, 90), 40.0));
	TestFalse(TEXT("and not the chopper's side"), Fell.Impact.Touches(FVector(-300, 0, 90), 40.0));
	TestEqual(TEXT("damage from half its height (4 m)"), Fell.Damage, 10.0 + 20.0 * 4.0, 1e-6);
	TestTrue(TEXT("a topple takes real time (a rod pivoting from 5 degrees)"), Fell.ImpactSeconds > 1.0 && Fell.ImpactSeconds < 5.0);
	// The tilt only grows.
	double Last = -1.0;
	bool bMonotonic = true;
	for (double T = Fell.StartSeconds; T < Fell.ImpactSeconds; T += 0.05)
	{
		const double Tilt = FMath::Acos(FMath::Clamp(GLCollapseRules::Motion(Fell, T).GetRotation().GetUpVector().Z, -1.0, 1.0));
		bMonotonic &= Tilt >= Last - 1e-6;
		Last = Tilt;
	}
	TestTrue(TEXT("the tilt only grows until impact"), bMonotonic);
	// The policy is data: pieceForward ignores who chopped it.
	Request.Direction = EGLToppleDirection::PieceBackward;
	const FGLCollapsePlan Back = GLCollapseRules::Plan(Content, { Request }, {}, GLCollapseTests::CFlat, FVector(-300, 0, 0), GLCollapseTests::Tuning());
	TestTrue(TEXT("pieceBackward falls west whoever chops"), Back.Outcomes[0].Rest.GetRotation().GetUpVector().Equals(FVector(-1, 0, 0), 0.01));
	return true;
}

#endif
