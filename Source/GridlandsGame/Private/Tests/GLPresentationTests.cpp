#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "Dialogue/GLDialogueDirector.h"
#include "Presentation/GLDerez.h"
#include "Events/GLEventSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "UI/GLSubtitles.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLPresentationTests
{
	FGLDialogueLine SubLine(const TCHAR* Exchange, int32 Index, const TCHAR* Speaker, const TCHAR* Text, double Delay)
	{
		FGLDialogueLine L;
		L.ExchangeId = Exchange;
		L.LineIndex = Index;
		L.Speaker = Speaker;
		L.Text = Text;
		L.Delay = Delay;
		return L;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSubtitleTimeline, "Gridlands.Game.Presentation.SubtitleTimelineFollowsTheDirector", GLTestUtils::Flags)
bool FGLSubtitleTimeline::RunTest(const FString& Parameters)
{
	using namespace GLPresentationTests;
	const FGLSpeakerStyle Nice = FGLSpeakerStyle::For(TEXT("NICE")), Peh = FGLSpeakerStyle::For(TEXT("Pehlichi"));
	TestTrue(TEXT("NICE and Pehlichi are named (not colour alone)"), Nice.Name == TEXT("NICE") && Peh.Name == TEXT("PEHLICHI"));
	TestFalse(TEXT("and coloured differently"), Nice.Colour.Equals(Peh.Colour));

	FGLSubtitleQueue Q;
	Q.Push(SubLine(TEXT("exchange.a"), 0, TEXT("NICE"), TEXT("One two three four five six."), 0.0), 10.0);
	Q.Push(SubLine(TEXT("exchange.a"), 1, TEXT("Pehlichi"), TEXT("Short."), 3.0), 10.0);
	TestEqual(TEXT("at the start only the first line"), Q.Visible(10.1).Num(), 1);
	TestEqual(TEXT("the second appears on the director's schedule"), Q.Visible(13.1).Last().Speaker, FName(TEXT("Pehlichi")));
	TestTrue(TEXT("reading time is bounded (2..9 s)"), FGLSubtitleQueue::ReadingSeconds(TEXT("Hi")) == 2.0 && FGLSubtitleQueue::ReadingSeconds(FString::ChrN(400, 'a').Replace(TEXT("a"), TEXT("a "))) == 9.0);
	TestTrue(TEXT("accessibility: a slower reading scale keeps lines longer"), FGLSubtitleQueue::ReadingSeconds(TEXT("one two three four five six seven"), 1.5) > FGLSubtitleQueue::ReadingSeconds(TEXT("one two three four five six seven")));

	// A voice sets the duration (future voice assets attach to the same line data).
	FGLSubtitleQueue V;
	V.Push(SubLine(TEXT("exchange.v"), 0, TEXT("NICE"), TEXT("Hi."), 0.0), 0.0, 6.0);
	TestTrue(TEXT("voiced line stays up for its voice"), V.Visible(6.1).Num() == 1 && V.Visible(6.4).Num() == 0);

	// Interruption (the director only starts an exchange mid-another when it interrupts it).
	FGLSubtitleQueue I;
	I.Push(SubLine(TEXT("exchange.chatter"), 0, TEXT("NICE"), TEXT("Blah blah blah blah."), 0.0), 0.0);
	I.Push(SubLine(TEXT("exchange.chatter"), 1, TEXT("Pehlichi"), TEXT("Pending line."), 4.0), 0.0);
	I.Push(SubLine(TEXT("exchange.story"), 0, TEXT("NICE"), TEXT("Story critical."), 0.0), 1.0);
	const TArray<FGLSubtitle> Now = I.Visible(1.1);
	TestTrue(TEXT("the interrupting line shows alone"), Now.Num() == 1 && Now[0].Exchange == FName(TEXT("exchange.story")));
	TestFalse(TEXT("the interrupted exchange's pending line never appears"), I.Visible(4.5).ContainsByPredicate([](const FGLSubtitle& S) { return S.Text == TEXT("Pending line."); }));

	// Never more than two on screen.
	FGLSubtitleQueue M;
	for (int32 K = 0; K < 4; ++K)
	{
		M.Push(SubLine(TEXT("exchange.m"), K, TEXT("NICE"), TEXT("a b c d e f g h i j k l m n o p"), 0.1 * K), 0.0);
	}
	TestEqual(TEXT("at most two lines at once"), M.Visible(0.5).Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSubtitleDirector, "Gridlands.Game.Presentation.DirectorLinesBecomeSubtitles", GLTestUtils::Flags)
bool FGLSubtitleDirector::RunTest(const FString& Parameters)
{
	GLTestUtils::FTestWorld Test(TEXT("GLSubtitleWorld"));
	UGLDialogueDirector* Director = Test.World->GetSubsystem<UGLDialogueDirector>();
	UGLSubtitleSubsystem* Subtitles = Test.World->GetSubsystem<UGLSubtitleSubsystem>();
	TestFalse(TEXT("debug text is off by default: the subtitle layer presents dialogue"), Director->bShowOnScreen);
	Director->SetTimeOverride(0.0);
	Subtitles->TimeOverride = 0.0;
	FGLGameplayEvent Started;
	Started.Tag = GLTestUtils::Tag(TEXT("Event.Game.Started"));
	UGLEventSubsystem::Emit(Test.World, Started);
	TestEqual(TEXT("the opening's three lines are on the timeline"), Subtitles->GetQueue().Num(), 3);
	TestEqual(TEXT("NICE speaks first"), Subtitles->Visible().Last().Speaker, FName(TEXT("NICE")));
	Subtitles->TimeOverride = 7.0;
	TestTrue(TEXT("and Pehlichi answers on schedule"), Subtitles->Visible().ContainsByPredicate([](const FGLSubtitle& S) { return S.Speaker == FName(TEXT("Pehlichi")); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDerezCreature, "Gridlands.Game.Presentation.DefeatedCreatureDerezzesThenIsGone", GLTestUtils::Flags)
bool FGLDerezCreature::RunTest(const FString& Parameters)
{
	GLTestUtils::FTestWorld Test(TEXT("GLDerezWorld"));
	AGLCreature* Gremlin = Test.World->SpawnActor<AGLCreature>(FVector(0, 0, 100), FRotator::ZeroRotator);
	Gremlin->Setup(TEXT("creature.drain.static_gremlin"), TEXT("placement.test.derez"));
	Gremlin->GetHealth()->ApplyDamage(1000.0, nullptr);
	TestTrue(TEXT("gameplay first: defeated at once"), Gremlin->IsDefeated());
	TestFalse(TEXT("and no longer blocks anything"), Gremlin->GetActorEnableCollision());
	TestTrue(TEXT("presentation: it is de-rezzing, still visible"), Gremlin->GetDerez()->IsRunning() && !Gremlin->IsHidden());
	int32 Steps = 0;
	while (Gremlin->GetDerez()->Advance(0.1f) && Steps < 100)
	{
		++Steps;
	}
	TestTrue(FString::Printf(TEXT("the dissolve takes its time (%d steps)"), Steps), Steps >= 10 && Steps <= 13);
	TestTrue(TEXT("then it is gone"), Gremlin->IsHidden());
	// Restored from a save it is already gone: no de-rez replay.
	AGLCreature* Loaded = Test.World->SpawnActor<AGLCreature>(FVector(500, 0, 100), FRotator::ZeroRotator);
	Loaded->Setup(TEXT("creature.drain.static_gremlin"), TEXT("placement.test.loaded"));
	Loaded->RestoreDefeated();
	TestTrue(TEXT("a restored defeat is hidden immediately, with no dissolve"), Loaded->IsHidden() && !Loaded->GetDerez()->IsRunning());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
