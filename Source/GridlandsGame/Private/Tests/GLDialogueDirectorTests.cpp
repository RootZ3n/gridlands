#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Dialogue/GLDialogueDirector.h"
#include "Events/GLEventSubsystem.h"
#include "Inventory/GLInventoryComponent.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Tests/GLTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLDialogueDirectorTests
{
	void Emit(UWorld* World, const TCHAR* Tag, FName Subject = NAME_None)
	{
		FGLGameplayEvent Event;
		Event.Tag = GLTestUtils::Tag(Tag);
		Event.Subject = Subject;
		UGLEventSubsystem::Emit(World, Event);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDirectorReacts, "Gridlands.Game.Dialogue.DirectorReactsToRealGameplay", GLTestUtils::Flags)
bool FGLDirectorReacts::RunTest(const FString& Parameters)
{
	GLTestUtils::FTestWorld Test(TEXT("GLDialogueTestWorld"));
	UGLDialogueDirector* Director = Test.World->GetSubsystem<UGLDialogueDirector>();
	if (!TestNotNull(TEXT("the director exists in a game world"), Director))
	{
		return false;
	}
	Director->bShowOnScreen = false;
	Director->SetSeed(5);
	Director->SetFrequency(EGLCommentaryFrequency::Quiet);
	TArray<FGLDialogueLine> Lines;
	Director->OnLine.AddLambda([&](const FGLDialogueLine& Line) { Lines.Add(Line); });

	// Story-critical opening plays even on Quiet.
	Director->SetTimeOverride(0.0);
	GLDialogueDirectorTests::Emit(Test.World, TEXT("Event.Game.Started"));
	TestEqual(TEXT("opening has three lines"), Lines.Num(), 3);
	TestTrue(TEXT("opening is exchange.story.opening"), Lines.Num() > 0 && Lines[0].ExchangeId == FName(TEXT("exchange.story.opening")));
	TestTrue(TEXT("lines are staggered"), Lines.Num() == 3 && Lines[1].Delay > 0.0 && Lines[2].Delay > Lines[1].Delay);
	GLDialogueDirectorTests::Emit(Test.World, TEXT("Event.Game.Started"));
	TestEqual(TEXT("the opening never repeats"), Lines.Num(), 3);

	// Real gameplay: salvage the wiring run (Item.Acquired x2, Salvage.Completed, Salvage.WireStripped).
	Director->SetFrequency(EGLCommentaryFrequency::Unhinged);
	Director->SetTimeOverride(1000.0);
	Lines.Reset();
	AActor* Player = Test.World->SpawnActor<AActor>();
	UGLInventoryComponent* Inventory = NewObject<UGLInventoryComponent>(Player);
	Inventory->RegisterComponent();
	AGLSalvageNode* Node = Test.World->SpawnActor<AGLSalvageNode>(FVector(300, 0, 0), FRotator::ZeroRotator);
	Node->GetSalvageable()->Setup(TEXT("salvage.house.wiring_run"));
	while (!Node->GetSalvageable()->IsSalvaged())
	{
		Node->GetSalvageable()->Interact(Player, GLTestUtils::Tag(TEXT("Interact.Salvage")));
	}
	TSet<FName> Exchanges;
	for (const FGLDialogueLine& Line : Lines)
	{
		Exchanges.Add(Line.ExchangeId);
		TestTrue(TEXT("only NICE and Pehlichi speak (Zenny is silent)"), Line.Speaker == TEXT("NICE") || Line.Speaker == TEXT("Pehlichi"));
	}
	TestEqual(TEXT("one burst of events -> one exchange, no pile-up"), Exchanges.Num(), 1);
	TestTrue(TEXT("it reacts to what actually happened (the fuse)"), Exchanges.Contains(TEXT("exchange.item.fuse")));

	// Later, wire stripping again: after the gap and the busy window, the wire joke can play.
	Director->SetTimeOverride(2000.0);
	Lines.Reset();
	GLDialogueDirectorTests::Emit(Test.World, TEXT("Event.Salvage.WireStripped"), TEXT("salvage.house.wiring_run"));
	TestTrue(TEXT("wire joke plays later"), Lines.Num() > 0 && Lines[0].ExchangeId == FName(TEXT("exchange.salvage.wire_bike")));
	TestEqual(TEXT("history counted both wire strips"), GLDialogueRules::CountEvents(Director->GetState(), TEXT("Event.Salvage.WireStripped")), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
