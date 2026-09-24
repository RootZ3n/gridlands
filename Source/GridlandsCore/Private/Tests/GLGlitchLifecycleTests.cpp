#include "Glitch/GLGlitchLifecycle.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// These tests pin ADR-0005 (Pehlichi is the sole repair authority) to the code.
// They enumerate the whole state x state x authority space instead of sampling it.

namespace GLGlitchLifecycleTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	TArray<EGLGlitchState> AllStates()
	{
		TArray<EGLGlitchState> States;
		for (int32 Index = 0; Index < FGLGlitchLifecycle::NumStates; ++Index)
		{
			States.Add(static_cast<EGLGlitchState>(Index));
		}
		return States;
	}

	TArray<EGLGlitchAuthority> AllAuthorities()
	{
		TArray<EGLGlitchAuthority> Authorities;
		for (int32 Index = 0; Index < FGLGlitchLifecycle::NumAuthorities; ++Index)
		{
			Authorities.Add(static_cast<EGLGlitchAuthority>(Index));
		}
		return Authorities;
	}

	/** States reachable from Start using only the given authorities. */
	TSet<EGLGlitchState> Reachable(EGLGlitchState Start, const TArray<EGLGlitchAuthority>& Using)
	{
		TSet<EGLGlitchState> Seen = { Start };
		TArray<EGLGlitchState> Frontier = { Start };
		while (Frontier.Num() > 0)
		{
			const EGLGlitchState From = Frontier.Pop();
			for (const EGLGlitchState To : AllStates())
			{
				for (const EGLGlitchAuthority By : Using)
				{
					if (!Seen.Contains(To) && FGLGlitchLifecycle::IsTransitionAllowed(From, To, By))
					{
						Seen.Add(To);
						Frontier.Add(To);
					}
				}
			}
		}
		return Seen;
	}

	TArray<EGLGlitchAuthority> AllExcept(EGLGlitchAuthority Excluded)
	{
		TArray<EGLGlitchAuthority> Authorities = AllAuthorities();
		Authorities.Remove(Excluded);
		return Authorities;
	}
}

using namespace GLGlitchLifecycleTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGlitchLifecyclePlayerHasNoAuthority,
	"Gridlands.Core.Glitch.Lifecycle.PlayerHasNoAuthority", TestFlags)
bool FGLGlitchLifecyclePlayerHasNoAuthority::RunTest(const FString& Parameters)
{
	int32 Checked = 0;
	for (const EGLGlitchState From : AllStates())
	{
		for (const EGLGlitchState To : AllStates())
		{
			++Checked;
			TestFalse(FString::Printf(TEXT("Player must not move %s -> %s"),
				FGLGlitchLifecycle::StateName(From), FGLGlitchLifecycle::StateName(To)),
				FGLGlitchLifecycle::IsTransitionAllowed(From, To, EGLGlitchAuthority::Player));
		}
	}
	TestEqual(TEXT("every state pair was checked"), Checked, FGLGlitchLifecycle::NumStates * FGLGlitchLifecycle::NumStates);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGlitchLifecycleOnlyPehlichiRepairs,
	"Gridlands.Core.Glitch.Lifecycle.OnlyPehlichiRepairCompletesRepair", TestFlags)
bool FGLGlitchLifecycleOnlyPehlichiRepairs::RunTest(const FString& Parameters)
{
	for (const EGLGlitchState From : AllStates())
	{
		for (const EGLGlitchAuthority By : AllAuthorities())
		{
			const bool bExpected = From == EGLGlitchState::Repairing && By == EGLGlitchAuthority::PehlichiRepair;
			TestEqual(FString::Printf(TEXT("%s -> Repaired by %s"),
				FGLGlitchLifecycle::StateName(From), FGLGlitchLifecycle::AuthorityName(By)),
				FGLGlitchLifecycle::IsTransitionAllowed(From, EGLGlitchState::Repaired, By), bExpected);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGlitchLifecycleRepairNeedsScanAndRepair,
	"Gridlands.Core.Glitch.Lifecycle.RepairRequiresPehlichiScanAndRepair", TestFlags)
bool FGLGlitchLifecycleRepairNeedsScanAndRepair::RunTest(const FString& Parameters)
{
	// With every authority, a latent glitch can eventually be repaired...
	TestTrue(TEXT("Latent can reach Repaired with all authorities"),
		Reachable(EGLGlitchState::Latent, AllAuthorities()).Contains(EGLGlitchState::Repaired));

	// ...but not without Pehlichi's scan: nothing else reveals a latent glitch.
	TestFalse(TEXT("Latent cannot leave Latent without PehlichiScan"),
		Reachable(EGLGlitchState::Latent, AllExcept(EGLGlitchAuthority::PehlichiScan)).Num() > 1);

	// ...and from no state can anything but Pehlichi's repair reach Repaired.
	for (const EGLGlitchState From : AllStates())
	{
		if (From == EGLGlitchState::Repaired)
		{
			continue;
		}
		TestFalse(FString::Printf(TEXT("%s cannot reach Repaired without PehlichiRepair"), FGLGlitchLifecycle::StateName(From)),
			Reachable(From, AllExcept(EGLGlitchAuthority::PehlichiRepair)).Contains(EGLGlitchState::Repaired));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGlitchLifecycleNoDeadStates,
	"Gridlands.Core.Glitch.Lifecycle.EveryUnrepairedStateCanStillBeRepaired", TestFlags)
bool FGLGlitchLifecycleNoDeadStates::RunTest(const FString& Parameters)
{
	for (const EGLGlitchState From : AllStates())
	{
		TestTrue(FString::Printf(TEXT("%s can reach Repaired"), FGLGlitchLifecycle::StateName(From)),
			Reachable(From, AllAuthorities()).Contains(EGLGlitchState::Repaired));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGlitchLifecycleRepairedIsTerminal,
	"Gridlands.Core.Glitch.Lifecycle.RepairedIsTerminal", TestFlags)
bool FGLGlitchLifecycleRepairedIsTerminal::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Repaired is terminal"), FGLGlitchLifecycle::IsTerminal(EGLGlitchState::Repaired));
	for (const EGLGlitchState To : AllStates())
	{
		TestFalse(FString::Printf(TEXT("Repaired -> %s"), FGLGlitchLifecycle::StateName(To)),
			FGLGlitchLifecycle::IsTransitionAllowedByAnyone(EGLGlitchState::Repaired, To));
	}
	for (const EGLGlitchState State : AllStates())
	{
		TestEqual(FString::Printf(TEXT("only Repaired is terminal (%s)"), FGLGlitchLifecycle::StateName(State)),
			FGLGlitchLifecycle::IsTerminal(State), State == EGLGlitchState::Repaired);
		TestFalse(FString::Printf(TEXT("no self transition on %s"), FGLGlitchLifecycle::StateName(State)),
			FGLGlitchLifecycle::IsTransitionAllowedByAnyone(State, State));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGlitchLifecycleTableMatchesDocs,
	"Gridlands.Core.Glitch.Lifecycle.TableMatchesDocumentedTransitions", TestFlags)
bool FGLGlitchLifecycleTableMatchesDocs::RunTest(const FString& Parameters)
{
	// Docs/GLITCH-AND-PEHLICHI.md section 2 lists 10 edges carrying 12 (edge, authority) grants.
	// A change to the table must change this test and that document together.
	int32 Edges = 0;
	int32 Grants = 0;
	for (const EGLGlitchState From : AllStates())
	{
		for (const EGLGlitchState To : AllStates())
		{
			Edges += FGLGlitchLifecycle::IsTransitionAllowedByAnyone(From, To) ? 1 : 0;
			for (const EGLGlitchAuthority By : AllAuthorities())
			{
				Grants += FGLGlitchLifecycle::IsTransitionAllowed(From, To, By) ? 1 : 0;
			}
		}
	}
	TestEqual(TEXT("documented edge count"), Edges, 10);
	TestEqual(TEXT("documented grant count"), Grants, 12);

	TestTrue(TEXT("Latent -> Detected by PehlichiScan"),
		FGLGlitchLifecycle::IsTransitionAllowed(EGLGlitchState::Latent, EGLGlitchState::Detected, EGLGlitchAuthority::PehlichiScan));
	TestTrue(TEXT("Detected -> Repairable by World"),
		FGLGlitchLifecycle::IsTransitionAllowed(EGLGlitchState::Detected, EGLGlitchState::Repairable, EGLGlitchAuthority::World));
	TestTrue(TEXT("Repairable -> Repairing by PehlichiRepair"),
		FGLGlitchLifecycle::IsTransitionAllowed(EGLGlitchState::Repairable, EGLGlitchState::Repairing, EGLGlitchAuthority::PehlichiRepair));
	TestTrue(TEXT("Repairing -> Interrupted by Hostile"),
		FGLGlitchLifecycle::IsTransitionAllowed(EGLGlitchState::Repairing, EGLGlitchState::Interrupted, EGLGlitchAuthority::Hostile));
	TestFalse(TEXT("Detected -> Repairing is not a shortcut"),
		FGLGlitchLifecycle::IsTransitionAllowedByAnyone(EGLGlitchState::Detected, EGLGlitchState::Repairing));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGlitchLifecycleVisibilityAndPersistence,
	"Gridlands.Core.Glitch.Lifecycle.VisibilityAndPersistedState", TestFlags)
bool FGLGlitchLifecycleVisibilityAndPersistence::RunTest(const FString& Parameters)
{
	for (const EGLGlitchState State : AllStates())
	{
		TestEqual(FString::Printf(TEXT("visibility of %s"), FGLGlitchLifecycle::StateName(State)),
			FGLGlitchLifecycle::IsVisibleToPlayer(State), State != EGLGlitchState::Latent);

		const EGLGlitchState Persisted = FGLGlitchLifecycle::ToPersistedState(State);
		TestNotEqual(FString::Printf(TEXT("%s never persists as Repairing"), FGLGlitchLifecycle::StateName(State)),
			Persisted, EGLGlitchState::Repairing);
		if (State != EGLGlitchState::Repairing)
		{
			TestEqual(FString::Printf(TEXT("%s persists unchanged"), FGLGlitchLifecycle::StateName(State)), Persisted, State);
		}
	}
	TestEqual(TEXT("Repairing persists as Interrupted"),
		FGLGlitchLifecycle::ToPersistedState(EGLGlitchState::Repairing), EGLGlitchState::Interrupted);
	TestFalse(TEXT("out-of-range authority is refused"),
		FGLGlitchLifecycle::IsTransitionAllowed(EGLGlitchState::Latent, EGLGlitchState::Detected, EGLGlitchAuthority::Count));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
