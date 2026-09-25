#include "Combat/GLCreatureRules.h"
#include "Content/GLContentDefinitions.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLCreatureRulesTests
{
	constexpr EAutomationTestFlags CreatureFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	FGLCreatureDef Gremlin()
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
		D.LeashRadius = 14.0;
		return D;
	}

	/** At home at the origin, facing +X, Zenny somewhere, clear line of sight unless told otherwise. */
	FGLCreatureFacts At(FVector Zenny, bool bSight = true)
	{
		FGLCreatureFacts F;
		F.Zenny = Zenny;
		F.bLineOfSight = bSight;
		return F;
	}
}

using namespace GLCreatureRulesTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCreatureSight, "Gridlands.Core.Combat.CreatureSeesOnlyWhatItShould", CreatureFlags)
bool FGLCreatureSight::RunTest(const FString& Parameters)
{
	const FGLCreatureDef D = Gremlin();
	namespace R = GLCreatureRules;
	TestEqual(TEXT("in front, in range, in sight: chase"), R::Decide(D, EGLCreatureState::Idle, At(FVector(800, 0, 0))).State, EGLCreatureState::Chase);
	TestEqual(TEXT("behind it: safe"), R::Decide(D, EGLCreatureState::Idle, At(FVector(-800, 0, 0))).State, EGLCreatureState::Idle);
	TestEqual(TEXT("beyond sight radius: safe"), R::Decide(D, EGLCreatureState::Idle, At(FVector(1300, 0, 0))).State, EGLCreatureState::Idle);
	TestEqual(TEXT("a wall in the way: safe"), R::Decide(D, EGLCreatureState::Idle, At(FVector(800, 0, 0), false)).State, EGLCreatureState::Idle);
	TestEqual(TEXT("outside its territory: it will not start a chase"), R::Decide(D, EGLCreatureState::Idle, [] { FGLCreatureFacts F = At(FVector(800, 0, 0)); F.Home = FVector(-800, 0, 0); F.Self = FVector(-200, 0, 0); return F; }()).State, EGLCreatureState::Return);
	FGLCreatureFacts Dead = At(FVector(800, 0, 0));
	Dead.bZennyAlive = false;
	TestEqual(TEXT("a de-rezzed Zenny is not chased"), R::Decide(D, EGLCreatureState::Chase, Dead).State, EGLCreatureState::Idle);
	// Once chasing it keeps track all around and a little further.
	TestEqual(TEXT("chasing: still sees behind"), R::Decide(D, EGLCreatureState::Chase, At(FVector(-800, 0, 0))).State, EGLCreatureState::Chase);
	FGLHealth Health;
	Health.Max = Health.Current = 10.0;
	TestEqual(TEXT("damage never goes below zero"), Health.Damage(25.0), 10.0);
	TestTrue(TEXT("and kills"), Health.IsDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCreatureOptions, "Gridlands.Core.Combat.LureBeatsChaseAttackRespectsCooldown", CreatureFlags)
bool FGLCreatureOptions::RunTest(const FString& Parameters)
{
	const FGLCreatureDef D = Gremlin();
	namespace R = GLCreatureRules;
	// A lure (Pehlichi's distraction) wins even over a chase in progress.
	FGLCreatureFacts Lured = At(FVector(300, 0, 0));
	Lured.LureSecondsLeft = 5.0;
	Lured.Lure = FVector(0, 900, 0);
	const FGLCreatureDecision Distracted = R::Decide(D, EGLCreatureState::Chase, Lured);
	TestEqual(TEXT("lure overrides the chase"), Distracted.State, EGLCreatureState::Investigate);
	TestEqual(TEXT("it goes to the noise, not to Zenny"), Distracted.MoveTo, FVector(0, 900, 0));
	TestFalse(TEXT("and does not strike"), Distracted.bStrike);
	// In reach: strikes only when the cooldown allows.
	FGLCreatureFacts Close = At(FVector(120, 0, 0));
	Close.SecondsSinceAttack = 0.5;
	TestFalse(TEXT("cooling down: no strike"), R::Decide(D, EGLCreatureState::Chase, Close).bStrike);
	Close.SecondsSinceAttack = 1.5;
	const FGLCreatureDecision Strike = R::Decide(D, EGLCreatureState::Chase, Close);
	TestTrue(TEXT("ready: strike"), Strike.State == EGLCreatureState::Attack && Strike.bStrike);
	// Away from home with nothing to chase: go back.
	FGLCreatureFacts Wandered = At(FVector(5000, 5000, 0), false);
	Wandered.Self = FVector(700, 0, 0);
	TestEqual(TEXT("returns home"), R::Decide(D, EGLCreatureState::Investigate, Wandered).State, EGLCreatureState::Return);
	FGLCreatureFacts Beaten = At(FVector(100, 0, 0));
	Beaten.bDefeated = true;
	TestEqual(TEXT("defeated stays defeated"), R::Decide(D, EGLCreatureState::Attack, Beaten).State, EGLCreatureState::Defeated);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
