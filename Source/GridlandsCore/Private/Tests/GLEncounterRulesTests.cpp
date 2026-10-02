// P9 creature rules (pure): patrol loops, the neutralized outcome, ambient masking of hearing, and the
// navigation rule (ADR-0029 as amended: navigation exists where active gameplay requires it).

#include "Combat/GLCreatureRules.h"
#include "Content/GLContentDefinitions.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLEncounterRulesTests
{
	constexpr EAutomationTestFlags ERFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	FGLCreatureDef ERGuard()
	{
		FGLCreatureDef D;
		D.Health = 60.0;
		D.WalkSpeed = 1.6;
		D.ChaseSpeed = 3.4;
		D.Perception.SightRadius = 12.0;
		D.Perception.ConeDegrees = 110.0;
		D.Perception.HearingRadius = 16.0;
		D.Attack.Damage = 12.0;
		D.Attack.Reach = 1.6;
		D.Attack.CooldownSeconds = 1.4;
		D.LeashRadius = 30.0;
		return D;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPatrolLoops, "Gridlands.Core.Combat.PatrolWalksItsLoopAndReturnsToIt", GLEncounterRulesTests::ERFlags)
bool FGLPatrolLoops::RunTest(const FString& Parameters)
{
	const FGLCreatureDef D = GLEncounterRulesTests::ERGuard();
	const TArray<FVector> Loop = { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 1000, 0) };
	FGLCreatureFacts F;
	F.Zenny = FVector(1e6, 0, 0); // far away
	F.Patrol = Loop;
	F.PatrolIndex = 1;
	FGLCreatureDecision Dec = GLCreatureRules::Decide(D, EGLCreatureState::Idle, F);
	TestEqual(TEXT("calm with a loop: it patrols"), Dec.State, EGLCreatureState::Patrol);
	TestTrue(TEXT("towards its current waypoint"), Dec.bMove && Dec.MoveTo.Equals(Loop[1]) && Dec.PatrolIndex == 1);
	F.Self = FVector(1000, 50, 0);
	Dec = GLCreatureRules::Decide(D, EGLCreatureState::Patrol, F);
	TestTrue(TEXT("arrived: on to the next waypoint"), Dec.PatrolIndex == 2 && Dec.MoveTo.Equals(Loop[2]));
	F.PatrolIndex = 2;
	F.Self = FVector(1000, 1000, 0);
	TestEqual(TEXT("the loop wraps around"), GLCreatureRules::Decide(D, EGLCreatureState::Patrol, F).PatrolIndex, 0);
	// Seeing Zenny beats patrolling; losing Zenny ends with a search, then the loop again (never "home").
	F.Self = FVector(0, 0, 0);
	F.Forward = FVector::ForwardVector;
	F.Zenny = FVector(600, 0, 0);
	F.bLineOfSight = true;
	TestEqual(TEXT("it sees Zenny: chase"), GLCreatureRules::Decide(D, EGLCreatureState::Patrol, F).State, EGLCreatureState::Chase);
	F.bLineOfSight = false;
	F.SearchSecondsLeft = 5.0;
	F.LastKnown = FVector(600, 0, 0);
	TestEqual(TEXT("sight lost: it searches"), GLCreatureRules::Decide(D, EGLCreatureState::Chase, F).State, EGLCreatureState::Search);
	F.SearchSecondsLeft = 0.0;
	TestEqual(TEXT("memory spent: back to its loop"), GLCreatureRules::Decide(D, EGLCreatureState::Search, F).State, EGLCreatureState::Patrol);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNeutralizedRules, "Gridlands.Core.Combat.NeutralizedIsInertAliveAndNeedsNoNavigation", GLEncounterRulesTests::ERFlags)
bool FGLNeutralizedRules::RunTest(const FString& Parameters)
{
	const FGLCreatureDef D = GLEncounterRulesTests::ERGuard();
	FGLCreatureFacts F;
	F.Zenny = FVector(100, 0, 0); // in reach, in sight
	F.bLineOfSight = true;
	F.bNeutralized = true;
	F.LureSecondsLeft = 5.0;
	const FGLCreatureDecision Dec = GLCreatureRules::Decide(D, EGLCreatureState::Attack, F);
	TestEqual(TEXT("neutralized: it does nothing, even with Zenny in reach and a lure"), Dec.State, EGLCreatureState::Neutralized);
	TestFalse(TEXT("  no move, no strike"), Dec.bMove || Dec.bStrike);
	TestFalse(TEXT("not an active hostile"), GLCreatureRules::IsActiveHostile(EGLCreatureState::Neutralized));
	TestFalse(TEXT("nor is a defeated one"), GLCreatureRules::IsActiveHostile(EGLCreatureState::Defeated));
	// Navigation exists where active gameplay requires it.
	TestFalse(TEXT("idle at home: no navigation of its own"), GLCreatureRules::NeedsNavigation(EGLCreatureState::Idle));
	TestFalse(TEXT("neutralized: none"), GLCreatureRules::NeedsNavigation(EGLCreatureState::Neutralized));
	TestFalse(TEXT("defeated: none"), GLCreatureRules::NeedsNavigation(EGLCreatureState::Defeated));
	for (const EGLCreatureState S : { EGLCreatureState::Patrol, EGLCreatureState::Investigate, EGLCreatureState::Chase, EGLCreatureState::Attack, EGLCreatureState::Search, EGLCreatureState::Return })
	{
		TestTrue(*FString::Printf(TEXT("%s: needs navigation"), GLCreatureRules::StateName(S)), GLCreatureRules::NeedsNavigation(S));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLMaskedHearing, "Gridlands.Core.Combat.AmbientNoiseMasksHearingAtTheListener", GLEncounterRulesTests::ERFlags)
bool FGLMaskedHearing::RunTest(const FString& Parameters)
{
	const FGLCreatureDef D = GLEncounterRulesTests::ERGuard(); // hears 16 m
	const FVector Self(0, 0, 0);
	TestTrue(TEXT("a 10 m noise 8 m away: heard in silence"), GLCreatureRules::Hears(D, Self, FVector(800, 0, 0), 1000.0));
	TestFalse(TEXT("under a 0.5 mask its reach halves to 5 m: not heard"), GLCreatureRules::Hears(D, Self, FVector(800, 0, 0), 1000.0, 0.5));
	TestTrue(TEXT("  but 4 m away still is"), GLCreatureRules::Hears(D, Self, FVector(400, 0, 0), 1000.0, 0.5));
	TestFalse(TEXT("a full mask hides everything"), GLCreatureRules::Hears(D, Self, FVector(10, 0, 0), 1000.0, 1.0));
	TestEqual(TEXT("the rule: min(noise, hearing) x (1 - M)"), GLCreatureRules::AudibleRangeCm(1600.0, 4000.0, 0.8), 320.0, 1e-6);
	TestEqual(TEXT("the mask is clamped to 0..1"), GLCreatureRules::AudibleRangeCm(1600.0, 4000.0, 1.7), 0.0, 1e-6);
	return true;
}

#endif
