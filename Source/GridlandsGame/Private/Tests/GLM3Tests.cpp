#include "Character/GLCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Interaction/GLDebugInteractable.h"
#include "Interaction/GLInteractorComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/GLTestUtils.h"
#include "World/GLGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GLTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLEventBus, "Gridlands.Game.Events.SubscribersReceiveMatchingTags", Flags)
bool FGLEventBus::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	UGLEventSubsystem* Bus = Test.World->GetSubsystem<UGLEventSubsystem>();
	if (!TestNotNull(TEXT("event bus exists in a game world"), Bus))
	{
		return false;
	}
	int32 SalvageFamily = 0, PlayerFamily = 0, Exact = 0;
	const FDelegateHandle Family = Bus->Subscribe(Tag(TEXT("Event.Salvage")), FGLGameplayEventDelegate::CreateLambda([&](const FGLGameplayEvent&) { ++SalvageFamily; }));
	Bus->Subscribe(Tag(TEXT("Event.Player")), FGLGameplayEventDelegate::CreateLambda([&](const FGLGameplayEvent&) { ++PlayerFamily; }));
	Bus->Subscribe(Tag(TEXT("Event.Salvage.WireStripped")), FGLGameplayEventDelegate::CreateLambda([&](const FGLGameplayEvent& E)
	{
		++Exact;
		TestEqual(TEXT("payload arrives"), E.Numbers.FindRef(TEXT("count")), 5.0);
	}));

	FGLGameplayEvent Wire;
	Wire.Tag = Tag(TEXT("Event.Salvage.WireStripped"));
	Wire.Subject = TEXT("salvage.house.wiring_run");
	Wire.Numbers.Add(TEXT("count"), 5.0);
	UGLEventSubsystem::Emit(Test.World, Wire);
	TestEqual(TEXT("parent-tag subscriber receives the child event"), SalvageFamily, 1);
	TestEqual(TEXT("exact subscriber receives it"), Exact, 1);
	TestEqual(TEXT("unrelated subscriber does not"), PlayerFamily, 0);

	Bus->Unsubscribe(Family);
	UGLEventSubsystem::Emit(Test.World, Wire);
	TestEqual(TEXT("unsubscribed listener hears nothing more"), SalvageFamily, 1);
	TestEqual(TEXT("others still do"), Exact, 2);

	for (int32 I = 0; I < UGLEventSubsystem::MaxRecentEvents + 10; ++I)
	{
		UGLEventSubsystem::Emit(Test.World, Wire);
	}
	TestEqual(TEXT("history is bounded"), Bus->GetRecentEvents().Num(), UGLEventSubsystem::MaxRecentEvents);
	UGLEventSubsystem::Emit(nullptr, Wire); // must not crash without a world
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLInteraction, "Gridlands.Game.Interaction.FocusAndInteract", Flags)
bool FGLInteraction::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	UWorld* World = Test.World;
	AGLDebugInteractable* Target = World->SpawnActor<AGLDebugInteractable>(FVector(250, 0, 50), FRotator::ZeroRotator);
	AActor* Player = World->SpawnActor<AActor>();
	UGLInteractorComponent* Interactor = NewObject<UGLInteractorComponent>(Player);
	Interactor->RegisterComponent();

	FString PerformedVerb;
	World->GetSubsystem<UGLEventSubsystem>()->Subscribe(Tag(TEXT("Event.Interaction.Performed")),
		FGLGameplayEventDelegate::CreateLambda([&](const FGLGameplayEvent& E) { PerformedVerb = E.Subject.ToString(); }));

	TestTrue(TEXT("focus finds the interactable in reach"), Interactor->UpdateFocus(FVector(0, 0, 50), FVector::ForwardVector) == Target);
	TArray<FGLInteractionOption> Options;
	Interactor->GetFocusOptions(Options);
	TestEqual(TEXT("one option"), Options.Num(), 1);
	TestTrue(TEXT("interact succeeds"), Interactor->TryInteract());
	TestEqual(TEXT("the target was used"), Target->TimesUsed, 1);
	TestEqual(TEXT("an interaction event was emitted"), PerformedVerb, FString(TEXT("Interact.Use")));

	Target->bEnabled = false;
	Interactor->GetFocusOptions(Options);
	TestTrue(TEXT("disabled option explains why"), Options.Num() == 1 && !Options[0].bEnabled && !Options[0].DisabledReason.IsEmpty());
	TestFalse(TEXT("a disabled option cannot be performed"), Interactor->TryInteract());
	TestEqual(TEXT("still used once"), Target->TimesUsed, 1);

	TestNull(TEXT("nothing in focus when looking away"), Interactor->UpdateFocus(FVector(0, 0, 50), -FVector::ForwardVector));
	Target->SetActorLocation(FVector(Interactor->Reach + 200.0, 0, 50));
	TestNull(TEXT("nothing in focus beyond reach"), Interactor->UpdateFocus(FVector(0, 0, 50), FVector::ForwardVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCharacterInput, "Gridlands.Game.Character.InputIsDefinedInCode", Flags)
bool FGLCharacterInput::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	AGLCharacter* Zenny = Test.World->SpawnActor<AGLCharacter>(FVector(0, 0, 200), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("character spawns"), Zenny))
	{
		return false;
	}
	TestNotNull(TEXT("character has an interactor"), Zenny->GetInteractor());
	Zenny->BuildInput();
	const TPair<const TCHAR*, EInputActionValueType> Expected[] = {
		{ TEXT("Move"), EInputActionValueType::Axis2D }, { TEXT("Look"), EInputActionValueType::Axis2D },
		{ TEXT("Jump"), EInputActionValueType::Boolean }, { TEXT("Interact"), EInputActionValueType::Boolean },
	};
	for (const auto& [Name, Type] : Expected)
	{
		const UInputAction* Action = Zenny->FindInputAction(Name);
		TestTrue(FString::Printf(TEXT("action %s exists with the right value type"), Name), Action && Action->ValueType == Type);
	}
	TSet<FKey> Keys;
	for (const FEnhancedActionKeyMapping& Mapping : Zenny->GetMappingContext()->GetMappings())
	{
		Keys.Add(Mapping.Key);
	}
	for (const FKey& Key : { EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::Mouse2D, EKeys::SpaceBar, EKeys::E, EKeys::Gamepad_Left2D })
	{
		TestTrue(FString::Printf(TEXT("%s is mapped"), *Key.ToString()), Keys.Contains(Key));
	}
	TestTrue(TEXT("the game mode spawns Zenny"), GetDefault<AGLGameMode>()->DefaultPawnClass == AGLCharacter::StaticClass());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
