#include "Misc/AutomationTest.h"
#include "World/GLStabilityModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLStabilityModelTests
{
	constexpr EAutomationTestFlags StabilityFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	TArray<FGLStabilitySample> Town()
	{
		return {
			{ FVector2D(0, 0), 1.0, 4000.0, false },
			{ FVector2D(3000, 1000), 1.5, 5000.0, false },
			{ FVector2D(-2500, 2500), 1.0, 3000.0, false },
		};
	}
}

using namespace GLStabilityModelTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStabilityDerived, "Gridlands.Core.Stability.DerivedDeterministicAndMonotone", StabilityFlags)
bool FGLStabilityDerived::RunTest(const FString& Parameters)
{
	// S-1 (ADR-0013): the numbers are a pure function of glitch states; repairing never makes anything worse.
	const TArray<FGLStabilitySample> Broken = Town();
	TArray<FVector2D> Points;
	for (int32 X = -6000; X <= 6000; X += 500)
	{
		for (int32 Y = -6000; Y <= 6000; Y += 500)
		{
			Points.Add(FVector2D(X, Y));
		}
	}
	for (const FVector2D& P : Points)
	{
		TestEqual(TEXT("same states, same interference"), GLStabilityModel::InterferenceAt(P, 0.1, Broken), GLStabilityModel::InterferenceAt(P, 0.1, Broken));
	}
	// Repair glitches one at a time: interference at every point never rises, and near each repair it drops.
	TArray<FGLStabilitySample> States = Broken;
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		TArray<double> Before;
		for (const FVector2D& P : Points)
		{
			Before.Add(GLStabilityModel::InterferenceAt(P, 0.1, States));
		}
		States[Index].bRepaired = true;
		for (int32 I = 0; I < Points.Num(); ++I)
		{
			const double After = GLStabilityModel::InterferenceAt(Points[I], 0.1, States);
			if (After > Before[I] + 1e-12)
			{
				AddError(FString::Printf(TEXT("repair %d raised interference at %s"), Index, *Points[I].ToString()));
			}
		}
		TestTrue(FString::Printf(TEXT("repair %d clears the air around it"), Index),
			GLStabilityModel::InterferenceAt(States[Index].Location, 0.1, States) < GLStabilityModel::InterferenceAt(States[Index].Location, 0.1, Broken));
	}
	TestEqual(TEXT("fully repaired, a home band reads Clear"), GLStabilityModel::TierFor(GLStabilityModel::InterferenceAt(FVector2D(0, 0), 0.0, States)), EGLInterferenceTier::Clear);
	TestTrue(TEXT("interference stays within 0..1"), GLStabilityModel::InterferenceAt(FVector2D(0, 0), 5.0, Broken) == 1.0 && GLStabilityModel::InterferenceAt(FVector2D(0, 0), -5.0, States) == 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStabilityComposure, "Gridlands.Core.Stability.NiceComposureAndTiers", StabilityFlags)
bool FGLStabilityComposure::RunTest(const FString& Parameters)
{
	TArray<FGLStabilitySample> States = Town();
	TestEqual(TEXT("NICE is composed while nothing is repaired"), GLStabilityModel::NiceComposure(States), 1.0);
	States[1].bRepaired = true; // weight 1.5 of 3.5
	TestTrue(TEXT("composure falls by the repaired weight share"), FMath::IsNearlyEqual(GLStabilityModel::NiceComposure(States), 1.0 - 1.5 / 3.5));
	for (FGLStabilitySample& S : States)
	{
		S.bRepaired = true;
	}
	TestEqual(TEXT("everything repaired: NICE has come apart"), GLStabilityModel::NiceComposure(States), 0.0);
	TestEqual(TEXT("no glitches: composure 1"), GLStabilityModel::NiceComposure({}), 1.0);
	TestEqual(TEXT("tier Clear"), GLStabilityModel::TierFor(0.1), EGLInterferenceTier::Clear);
	TestEqual(TEXT("tier Hazy"), GLStabilityModel::TierFor(0.3), EGLInterferenceTier::Hazy);
	TestEqual(TEXT("tier Static"), GLStabilityModel::TierFor(0.5), EGLInterferenceTier::Static);
	TestEqual(TEXT("tier Blizzard"), GLStabilityModel::TierFor(0.9), EGLInterferenceTier::Blizzard);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
