// P10 (ADR-0038): a collapse in flight as a durable fact. What the decision fixed is saved and rebuilt bit for bit,
// through the real save codec, without re-planning; a topple's angles are re-integrated from their three inputs.

#include "Building/GLCollapseRules.h"
#include "Building/GLPendingCollapse.h"
#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Save/GLWorldSave.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLPendingCollapseTests
{
	constexpr EAutomationTestFlags PCFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	const FGLContentRegistry& PCContent()
	{
		static FGLContentRegistry Registry;
		static bool bLoaded = Registry.LoadRepository(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		return Registry;
	}

	FGLCollapseTuningDef PCTuning()
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
		T.PinMinSeverity = 2.0;
		return T;
	}

	/** A sloped, bumpy ground: rests and landings are not round numbers. */
	double PCGround(const FVector2D& At) { return 3.7 + 0.031 * At.X - 0.017 * At.Y + 2.0 * FMath::Sin(At.X * 0.0123); }

	/** The P6 carport with both posts gone (two drops), and a lone pine trunk toppling: every motion the plan makes. */
	TArray<FGLCollapseOutcome> PCOutcomes()
	{
		const FGLContentRegistry& Content = PCContent();
		const FName Deck(TEXT("buildpiece.modern.timber_deck"));
		const FName Trunk(TEXT("buildpiece.nature.pine_trunk"));
		TArray<FGLCollapseRequest> Decks = {
			{ { 3, Deck, FVector(13.3, -7.1, 261.7) }, EGLCollapseMotion::Drop },
			{ { 4, Deck, FVector(213.3, -7.1, 261.7) }, EGLCollapseMotion::Drop },
		};
		TArray<FGLCollapseOutcome> Out = GLCollapseRules::Plan(Content, Decks, {}, [](const FVector2D& At) { return PCGround(At); }, FVector(-400, 0, 0), PCTuning()).Outcomes;
		const TArray<FGLCollapseRequest> Topple = { { { 7, Trunk, FVector(1503.9, 377.2, 41.3), 1 }, EGLCollapseMotion::Topple } };
		Out.Append(GLCollapseRules::Plan(Content, Topple, {}, [](const FVector2D& At) { return PCGround(At); }, FVector(1100, 300, 0), PCTuning()).Outcomes);
		return Out;
	}

	bool PCSameBits(double A, double B) { return FMemory::Memcmp(&A, &B, sizeof(double)) == 0; }

	bool PCSamePose(const FTransform& A, const FTransform& B)
	{
		const FVector LA = A.GetLocation(), LB = B.GetLocation();
		const FQuat QA = A.GetRotation(), QB = B.GetRotation();
		return PCSameBits(LA.X, LB.X) && PCSameBits(LA.Y, LB.Y) && PCSameBits(LA.Z, LB.Z)
			&& PCSameBits(QA.X, QB.X) && PCSameBits(QA.Y, QB.Y) && PCSameBits(QA.Z, QB.Z) && PCSameBits(QA.W, QB.W);
	}

	/** Through the real file format: a world save holding the records, written as JSON text and read back. */
	TArray<FGLSavedCollapse> PCThroughCodec(const TArray<FGLSavedCollapse>& Records, FAutomationTestBase& Test)
	{
		FGLWorldSave Save;
		FGLSavedCell& Cell = Save.Cells.AddDefaulted_GetRef();
		Cell.Cell = TEXT("cell.outer.diner_lots");
		Cell.Collapses = Records;
		FGLWorldSave Back;
		FString Problem;
		Test.TestTrue(TEXT("the save codec reads it back"), GLSaveCodec::FromJson(GLSaveCodec::ToJson(Save), Back, Problem));
		return Back.Cells.Num() == 1 ? Back.Cells[0].Collapses : TArray<FGLSavedCollapse>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPendingCollapseBits, "Gridlands.Core.Structure.APendingCollapseRebuildsBitIdenticallyThroughTheSaveFormat", GLPendingCollapseTests::PCFlags)
bool FGLPendingCollapseBits::RunTest(const FString& Parameters)
{
	const TArray<FGLCollapseOutcome> Plans = GLPendingCollapseTests::PCOutcomes();
	TestEqual(TEXT("two drops and a topple"), Plans.Num(), 3);
	TestTrue(TEXT("the topple has its angle samples"), Plans.Num() == 3 && Plans[2].Motion == EGLCollapseMotion::Topple && Plans[2].ToppleAngles.Num() > 50);
	TArray<FGLSavedCollapse> Records;
	const double Elapsed = 1.0 / 3.0 + 1e-13; // an awkward double: every bit must survive the text format
	for (const FGLCollapseOutcome& Plan : Plans)
	{
		Records.Add(GLPendingCollapse::Capture(TEXT("placement.x"), *FString::Printf(TEXT("part_%d"), Plan.PieceId), Plan, Elapsed, TEXT("material.timber.pine"), TEXT("Zenny"), TEXT("Zenny")));
	}
	const TArray<FGLSavedCollapse> Read = GLPendingCollapseTests::PCThroughCodec(Records, *this);
	if (!TestEqual(TEXT("every record comes back"), Read.Num(), Plans.Num()))
	{
		return false;
	}
	for (int32 I = 0; I < Plans.Num(); ++I)
	{
		FGLCollapseOutcome Rebuilt;
		FString Problem, Difference;
		TestTrue(*FString::Printf(TEXT("record %d reconstructs (%s)"), I, *Problem), GLPendingCollapse::Reconstruct(Read[I], Rebuilt, &Problem));
		TestTrue(*FString::Printf(TEXT("record %d is bit-identical to the plan (first difference: %s)"), I, *Difference), GLPendingCollapse::Identical(Plans[I], Rebuilt, &Difference));
		TestTrue(TEXT("  its elapsed time, bit for bit"), GLPendingCollapseTests::PCSameBits(Read[I].ElapsedSeconds, Elapsed));
		TestTrue(TEXT("  cause and credit kept apart and kept"), Read[I].Cause == FName(TEXT("Zenny")) && Read[I].Credit == FName(TEXT("Zenny")));
		// Presentation: every pose from the decision to past the impact, every millisecond, is the plan's pose.
		int32 Mismatches = 0;
		for (int32 Ms = 0; Ms <= FMath::CeilToInt((Plans[I].ImpactSeconds + 0.1) * 1000.0); ++Ms)
		{
			Mismatches += GLPendingCollapseTests::PCSamePose(GLCollapseRules::Motion(Plans[I], Ms / 1000.0), GLCollapseRules::Motion(Rebuilt, Ms / 1000.0)) ? 0 : 1;
		}
		TestEqual(*FString::Printf(TEXT("record %d: every millisecond's pose is bit-identical"), I), Mismatches, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPendingCollapseNoReplan, "Gridlands.Core.Structure.APendingCollapseIsNeverReplanned", GLPendingCollapseTests::PCFlags)
bool FGLPendingCollapseNoReplan::RunTest(const FString& Parameters)
{
	const TArray<FGLCollapseOutcome> Plans = GLPendingCollapseTests::PCOutcomes();
	const FGLCollapseOutcome& Topple = Plans.Last();
	const FGLSavedCollapse Record = GLPendingCollapse::Capture(TEXT("placement.x"), TEXT("trunk"), Topple, 0.5, NAME_None, TEXT("Zenny"), TEXT("Zenny"));
	// The world changes after the support failed (other tuning, other ground): a re-plan would answer differently.
	FGLCollapseTuningDef Changed = GLPendingCollapseTests::PCTuning();
	Changed.Gravity = 3.0;
	Changed.ToppleStartDegrees = 20.0;
	const FName Trunk(TEXT("buildpiece.nature.pine_trunk"));
	const TArray<FGLCollapseRequest> Same = { { { 7, Trunk, FVector(1503.9, 377.2, 41.3), 1 }, EGLCollapseMotion::Topple } };
	const FGLCollapsePlan Replanned = GLCollapseRules::Plan(GLPendingCollapseTests::PCContent(), Same, {}, [](const FVector2D&) { return 0.0; }, FVector(1100, 300, 0), Changed);
	TestFalse(TEXT("control: a re-plan in the changed world is a different collapse"), Replanned.Outcomes.Num() == 1 && GLPendingCollapse::Identical(Topple, Replanned.Outcomes[0]));
	FGLCollapseOutcome Rebuilt;
	TestTrue(TEXT("the record rebuilds"), GLPendingCollapse::Reconstruct(Record, Rebuilt));
	TestTrue(TEXT("and it is the collapse that was decided, not the re-plan"), GLPendingCollapse::Identical(Topple, Rebuilt));
	// A record that is not that plan is refused, never silently loosened.
	FGLSavedCollapse Tampered = Record;
	Tampered.ImpactSeconds += 0.25;
	FString Problem;
	TestFalse(TEXT("a topple whose impact time disagrees with its own integration is refused"), GLPendingCollapse::Reconstruct(Tampered, Rebuilt, &Problem));
	TestTrue(TEXT("  with a reason"), Problem.Contains(TEXT("re-integrates")));
	FGLSavedCollapse BadMotion = Record;
	BadMotion.Motion = 7;
	TestFalse(TEXT("an unknown motion is refused"), GLPendingCollapse::Reconstruct(BadMotion, Rebuilt));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLImpactSeverity, "Gridlands.Core.Structure.ImpactSeverityIsPhysicalAndPinningIsTuning", GLPendingCollapseTests::PCFlags)
bool FGLImpactSeverity::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = GLPendingCollapseTests::PCContent();
	const FName Deck(TEXT("buildpiece.modern.timber_deck"));
	FGLCollapseTuningDef T = GLPendingCollapseTests::PCTuning();
	auto Flat = [](const FVector2D&) { return 0.0; };
	auto DeckAt = [&](double Up, double Scale)
	{
		const TArray<FGLCollapseRequest> R = { { { 1, Deck, FVector(0, 0, Up) }, EGLCollapseMotion::Drop, EGLToppleDirection::AwayFromInstigator, Scale } };
		return GLCollapseRules::Plan(Content, R, {}, Flat, FVector::ZeroVector, T).Outcomes[0];
	};
	const FGLCollapseOutcome High = DeckAt(250.0, 1.0);
	TestEqual(TEXT("severity = metres fallen x impactScale (2.5 m, timber 1.0)"), High.Severity, 2.5);
	TestTrue(TEXT("a 2.5 m fall pins at the 2.0 threshold"), GLCollapseRules::Pins(High, T));
	TestEqual(TEXT("a heavier material hits harder"), DeckAt(250.0, 1.6).Severity, 4.0);
	const FGLCollapseOutcome Low = DeckAt(150.0, 1.0);
	TestFalse(TEXT("a 1.5 m fall does not pin"), GLCollapseRules::Pins(Low, T));
	TestEqual(TEXT("  (but it still does its damage)"), Low.Damage, 10.0 + 20.0 * 1.5);
	TestEqual(TEXT("barely moving is no impact at all"), DeckAt(3.0, 1.0).Severity, 0.0);
	T.PinMinSeverity = 0.0;
	TestFalse(TEXT("no threshold: nothing pins"), GLCollapseRules::Pins(High, T));
	// The one capsule test is the three-sphere test the impact always used.
	FGLImpactVolume V;
	V.Centre = FVector(10, -20, 120);
	V.Axis[0] = FVector(0.6, 0.8, 0.0);
	V.Axis[1] = FVector(-0.8, 0.6, 0.0);
	V.HalfExtent = FVector(130, 60, 100);
	int32 Disagree = 0, Touching = 0;
	for (double X = -300; X <= 300; X += 23)
	{
		for (double Y = -300; Y <= 300; Y += 23)
		{
			for (double Z = -100; Z <= 400; Z += 31)
			{
				const FVector C(X, Y, Z), Up(0, 0, 20);
				const bool bOld = V.Touches(C, 40) || V.Touches(C + Up, 40) || V.Touches(C - Up, 40);
				const bool bNew = V.TouchesCapsule(C, 40, 60);
				Disagree += bOld != bNew ? 1 : 0;
				Touching += bNew ? 1 : 0;
			}
		}
	}
	TestEqual(TEXT("TouchesCapsule agrees with the three-sphere test everywhere"), Disagree, 0);
	TestTrue(TEXT("  (and the sample both touches and misses)"), Touching > 100 && Touching < 10000);
	return true;
}

#endif
