#include "Content/GLContentDefinitions.h"
#include "Dialogue/GLDialogueRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// ADR-0015 invariants D-2 (story-critical ignores the setting), D-3 (silence gap, repetition
// limits) and D-5 (deterministic under a seed), on synthetic exchanges so each rule is isolated.

namespace GLDialogueRulesTests
{
	constexpr EAutomationTestFlags DialogueFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	FGLExchangeDef MakeExchange(const TCHAR* Id, const TCHAR* Trigger, EGLExchangeCategory Category, int32 Priority = 10, int32 MaxUses = 0, double Cooldown = 0.0)
	{
		FGLExchangeDef E;
		E.Id = Id;
		E.Trigger.Add(Trigger);
		E.Category = Category;
		E.Priority = Priority;
		E.MaxUses = MaxUses;
		E.CooldownSeconds = Cooldown;
		E.Weight = 1.0;
		E.Lines.Add({ TEXT("NICE"), TEXT("A line.") });
		return E;
	}

	const FGLExchangeDef* Pick(const TArray<const FGLExchangeDef*>& All, const TCHAR* Event, double Now, EGLCommentaryFrequency Frequency,
		FGLDialogueState& State, FRandomStream& Random, FName Subject = NAME_None)
	{
		GLDialogueRules::RecordEvent(Event, State);
		const FGLDialogueChoice Choice = GLDialogueRules::Choose(All, Event, Subject, Now, Frequency, State, Random);
		if (Choice.Exchange)
		{
			GLDialogueRules::RecordPlayed(*Choice.Exchange, Now, State);
		}
		return Choice.Exchange;
	}
}

using namespace GLDialogueRulesTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDialogueMatching, "Gridlands.Core.Dialogue.TriggersSubjectsAndHistory", DialogueFlags)
bool FGLDialogueMatching::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("parent trigger matches child event"), GLDialogueRules::TagMatches(TEXT("Event.Salvage.WireStripped"), TEXT("Event.Salvage")));
	TestTrue(TEXT("exact match"), GLDialogueRules::TagMatches(TEXT("Event.Salvage"), TEXT("Event.Salvage")));
	TestFalse(TEXT("no partial-segment match"), GLDialogueRules::TagMatches(TEXT("Event.SalvageYard"), TEXT("Event.Salvage")));
	TestFalse(TEXT("child trigger does not match parent event"), GLDialogueRules::TagMatches(TEXT("Event.Salvage"), TEXT("Event.Salvage.WireStripped")));

	FGLExchangeDef Fuse = MakeExchange(TEXT("exchange.t.fuse"), TEXT("Event.Item.Acquired"), EGLExchangeCategory::Contextual);
	Fuse.Subject = TEXT("item.part.fuse");
	FGLExchangeDef Third = MakeExchange(TEXT("exchange.t.third_death"), TEXT("Event.Player.Died"), EGLExchangeCategory::Contextual);
	Third.Requires.Add({ TEXT("Event.Player.Died"), 3, 0 });
	const TArray<const FGLExchangeDef*> All = { &Fuse, &Third };
	FGLDialogueState State;
	FRandomStream Random(1);
	TestNull(TEXT("subject filter rejects other items"), Pick(All, TEXT("Event.Item.Acquired"), 0, EGLCommentaryFrequency::Unhinged, State, Random, TEXT("item.material.scrap_metal")));
	TestTrue(TEXT("subject filter accepts its item"), Pick(All, TEXT("Event.Item.Acquired"), 100, EGLCommentaryFrequency::Unhinged, State, Random, TEXT("item.part.fuse")) == &Fuse);
	TestNull(TEXT("first death: requirement not met"), Pick(All, TEXT("Event.Player.Died"), 200, EGLCommentaryFrequency::Unhinged, State, Random));
	TestNull(TEXT("second death: not yet"), Pick(All, TEXT("Event.Player.Died"), 300, EGLCommentaryFrequency::Unhinged, State, Random));
	TestTrue(TEXT("third death: history makes it eligible"), Pick(All, TEXT("Event.Player.Died"), 400, EGLCommentaryFrequency::Unhinged, State, Random) == &Third);
	TestEqual(TEXT("history counts events"), GLDialogueRules::CountEvents(State, TEXT("Event.Player")), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDialogueStoryCritical, "Gridlands.Core.Dialogue.StoryCriticalIgnoresSetting", DialogueFlags)
bool FGLDialogueStoryCritical::RunTest(const FString& Parameters)
{
	// D-2: under Quiet, inside the silence gap, while an optional exchange plays: story still gets through.
	FGLExchangeDef Chatter = MakeExchange(TEXT("exchange.t.chatter"), TEXT("Event.Test.Chatter"), EGLExchangeCategory::Contextual);
	FGLExchangeDef Story = MakeExchange(TEXT("exchange.t.story"), TEXT("Event.Test.Story"), EGLExchangeCategory::StoryCritical, 100, 1);
	FGLExchangeDef Story2 = MakeExchange(TEXT("exchange.t.story2"), TEXT("Event.Test.Story2"), EGLExchangeCategory::StoryCritical, 100, 1);
	const TArray<const FGLExchangeDef*> All = { &Chatter, &Story, &Story2 };
	for (int32 Seed = 0; Seed < 50; ++Seed)
	{
		FGLDialogueState State;
		FRandomStream Random(Seed);
		State.LastOptionalTime = 0.0;          // silence gap active
		State.PlayingUntil = 5.0;              // an optional exchange is playing
		State.bPlayingStoryCritical = false;
		TestTrue(FString::Printf(TEXT("seed %d: story pre-empts optional chatter under Quiet"), Seed),
			Pick(All, TEXT("Event.Test.Story"), 1.0, EGLCommentaryFrequency::Quiet, State, Random) == &Story);
		TestNull(TEXT("but story never interrupts story"), Pick(All, TEXT("Event.Test.Story2"), 1.5, EGLCommentaryFrequency::Quiet, State, Random));
		TestNull(TEXT("and chatter cannot interrupt anything"), Pick(All, TEXT("Event.Test.Chatter"), 1.6, EGLCommentaryFrequency::Unhinged, State, Random));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDialogueLimits, "Gridlands.Core.Dialogue.SilenceGapAndRepetitionLimits", DialogueFlags)
bool FGLDialogueLimits::RunTest(const FString& Parameters)
{
	// D-3.
	FGLExchangeDef Once = MakeExchange(TEXT("exchange.t.once"), TEXT("Event.Test.A"), EGLExchangeCategory::Contextual, 20, 1);
	FGLExchangeDef Cooled = MakeExchange(TEXT("exchange.t.cooled"), TEXT("Event.Test.B"), EGLExchangeCategory::Contextual, 20, 0, 1000.0);
	const TArray<const FGLExchangeDef*> All = { &Once, &Cooled };
	const double Gap = GLDialogueRules::TuningFor(EGLCommentaryFrequency::Unhinged).SilenceGapSeconds;
	FGLDialogueState State;
	FRandomStream Random(7);
	TestTrue(TEXT("first A plays"), Pick(All, TEXT("Event.Test.A"), 0, EGLCommentaryFrequency::Unhinged, State, Random) == &Once);
	TestNull(TEXT("B inside the silence gap is suppressed"), Pick(All, TEXT("Event.Test.B"), Gap * 0.5, EGLCommentaryFrequency::Unhinged, State, Random));
	TestTrue(TEXT("B after the gap plays"), Pick(All, TEXT("Event.Test.B"), Gap + 10, EGLCommentaryFrequency::Unhinged, State, Random) == &Cooled);
	TestNull(TEXT("A never repeats past maxUses"), Pick(All, TEXT("Event.Test.A"), 5000, EGLCommentaryFrequency::Unhinged, State, Random));
	TestNull(TEXT("B respects its own cooldown"), Pick(All, TEXT("Event.Test.B"), Gap + 500, EGLCommentaryFrequency::Unhinged, State, Random));
	TestTrue(TEXT("B returns after its cooldown"), Pick(All, TEXT("Event.Test.B"), Gap + 1100, EGLCommentaryFrequency::Unhinged, State, Random) == &Cooled);

	// Repetition protection among equals: a used exchange is drawn less often.
	FGLExchangeDef X = MakeExchange(TEXT("exchange.t.x"), TEXT("Event.Test.C"), EGLExchangeCategory::Contextual);
	FGLExchangeDef Y = MakeExchange(TEXT("exchange.t.y"), TEXT("Event.Test.C"), EGLExchangeCategory::Contextual);
	const TArray<const FGLExchangeDef*> Pair = { &X, &Y };
	FGLDialogueState Worn;
	Worn.Exchanges.Add(X.Id, { 9, -1.0e12 });
	int32 PickedY = 0;
	FRandomStream Draw(11);
	for (int32 I = 0; I < 1000; ++I)
	{
		const FGLDialogueChoice Choice = GLDialogueRules::Choose(Pair, TEXT("Event.Test.C"), NAME_None, 0.0, EGLCommentaryFrequency::Unhinged, Worn, Draw);
		PickedY += Choice.Exchange == &Y ? 1 : 0;
	}
	TestTrue(FString::Printf(TEXT("the unused exchange wins most draws (%d/1000, expected ~909)"), PickedY), PickedY > 850);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDialogueDeterminism, "Gridlands.Core.Dialogue.DeterministicUnderSeed", DialogueFlags)
bool FGLDialogueDeterminism::RunTest(const FString& Parameters)
{
	// D-5: same seed and events -> same choices.
	TArray<FGLExchangeDef> Pool;
	const TCHAR* Events[] = { TEXT("Event.Test.A"), TEXT("Event.Test.B"), TEXT("Event.Test.C") };
	for (int32 I = 0; I < 12; ++I)
	{
		Pool.Add(MakeExchange(*FString::Printf(TEXT("exchange.t.e%d"), I), Events[I % 3], I % 4 == 0 ? EGLExchangeCategory::Ambient : EGLExchangeCategory::Contextual, 10 + I % 2));
	}
	TArray<const FGLExchangeDef*> All;
	for (const FGLExchangeDef& E : Pool)
	{
		All.Add(&E);
	}
	auto Run = [&](int32 Seed)
	{
		FGLDialogueState State;
		FRandomStream Random(Seed);
		TArray<FName> Picks;
		for (int32 Step = 0; Step < 300; ++Step)
		{
			const FGLExchangeDef* E = Pick(All, Events[Step % 3], Step * 45.0, EGLCommentaryFrequency::Normal, State, Random);
			Picks.Add(E ? E->Id : NAME_None);
		}
		return Picks;
	};
	TestTrue(TEXT("identical seeds give identical choices"), Run(42) == Run(42));
	TestTrue(TEXT("different seeds can differ (the draw is real)"), Run(42) != Run(43));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDialogueFrequency, "Gridlands.Core.Dialogue.FrequencyScalesOptionalOnly", DialogueFlags)
bool FGLDialogueFrequency::RunTest(const FString& Parameters)
{
	FGLExchangeDef Ambient = MakeExchange(TEXT("exchange.t.ambient"), TEXT("Event.Test.A"), EGLExchangeCategory::Ambient);
	const TArray<const FGLExchangeDef*> All = { &Ambient };
	auto Rate = [&](EGLCommentaryFrequency Frequency)
	{
		FRandomStream Random(99);
		int32 Played = 0;
		for (int32 I = 0; I < 4000; ++I)
		{
			FGLDialogueState Fresh; // no gap, no history: only the frequency roll decides
			Played += GLDialogueRules::Choose(All, TEXT("Event.Test.A"), NAME_None, 0.0, Frequency, Fresh, Random).Exchange ? 1 : 0;
		}
		return Played / 4000.0;
	};
	const double Quiet = Rate(EGLCommentaryFrequency::Quiet), Normal = Rate(EGLCommentaryFrequency::Normal);
	const double Chatty = Rate(EGLCommentaryFrequency::Chatty), Unhinged = Rate(EGLCommentaryFrequency::Unhinged);
	TestTrue(FString::Printf(TEXT("ambient rate rises with the setting (%.2f < %.2f < %.2f < %.2f)"), Quiet, Normal, Chatty, Unhinged),
		Quiet < Normal && Normal < Chatty && Chatty < Unhinged);
	TestTrue(TEXT("Quiet ambient near 10%"), FMath::Abs(Quiet - 0.10) < 0.03);
	TestEqual(TEXT("Unhinged ambient always plays"), Unhinged, 1.0);
	TestTrue(TEXT("silence gaps shrink as the setting rises"),
		GLDialogueRules::TuningFor(EGLCommentaryFrequency::Quiet).SilenceGapSeconds > GLDialogueRules::TuningFor(EGLCommentaryFrequency::Normal).SilenceGapSeconds
		&& GLDialogueRules::TuningFor(EGLCommentaryFrequency::Normal).SilenceGapSeconds > GLDialogueRules::TuningFor(EGLCommentaryFrequency::Unhinged).SilenceGapSeconds);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
