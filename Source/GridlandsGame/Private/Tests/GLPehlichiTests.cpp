#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Events/GLEventSubsystem.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "Interaction/GLInteractable.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Pehlichi/GLCapabilityComponent.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
#include "Pehlichi/GLRepairComponent.h"
#include "Pehlichi/GLScanComponent.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Tests/GLTestUtils.h"
#include "World/GLPlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLPehlichiTests
{
	/** The origin cell's gameplay layer, a commander with an inventory, and Pehlichi. */
	struct FPehlichiScene
	{
		GLTestUtils::FTestWorld Test{ TEXT("GLPehlichiTestWorld") };
		AActor* Zenny = nullptr;
		UGLInventoryComponent* Inventory = nullptr;
		AGLPehlichi* Pehlichi = nullptr;
		UGLGlitchSubsystem* Glitches = nullptr;
		UGLPlacementSubsystem* Placements = nullptr;
		TArray<FString> Events;

		FPehlichiScene()
		{
			UWorld* World = Test.World;
			Placements = World->GetSubsystem<UGLPlacementSubsystem>();
			Glitches = World->GetSubsystem<UGLGlitchSubsystem>();
			Placements->SpawnCell(TEXT("cell.home.origin"));
			Zenny = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Zenny);
			Zenny->SetRootComponent(Root);
			Root->RegisterComponent();
			Inventory = NewObject<UGLInventoryComponent>(Zenny);
			Inventory->RegisterComponent();
			Glitches->SetCommander(Zenny);
			Pehlichi = World->SpawnActor<AGLPehlichi>(FVector::ZeroVector, FRotator::ZeroRotator);
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& E) { Events.Add(E.Tag.ToString() + TEXT(":") + E.Subject.ToString()); }));
		}

		AGLGlitch* Glitch(const TCHAR* PlacementId) const { return Glitches->FindByPlacement(PlacementId); }
		EGLGlitchState State(const TCHAR* PlacementId) const { return Glitch(PlacementId)->GetGlitch()->GetState(); }
		void Place(AActor* Actor, const FVector& At) { Actor->SetActorLocation(At); }

		/** Runs Pehlichi's movement and repair, and the world's requirement checks, for Seconds. */
		void Run(double Seconds, double Step = 0.1)
		{
			for (double T = 0.0; T < Seconds; T += Step)
			{
				Glitches->EvaluateRequirements();
				Pehlichi->GetPositioning()->Advance(Step);
				Pehlichi->GetRepair()->Advance(Step);
			}
		}

		void Salvage(const TCHAR* PlacementId)
		{
			AGLSalvageNode* Node = Placements->FindSalvageNode(PlacementId);
			while (Node && !Node->GetSalvageable()->IsSalvaged())
			{
				Node->GetSalvageable()->Interact(Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
			}
		}
	};

	const TCHAR* PehlichiTestLamp = TEXT("placement.origin.glitch_flicker_lamp");
	const TCHAR* PehlichiTestTransformer = TEXT("placement.origin.glitch_dead_transformer");
	const TCHAR* PehlichiTestBuried = TEXT("placement.origin.glitch_buried_signal");
	const FName PehlichiTestScanCapability(TEXT("capability.pehlichi.scan"));
}

using namespace GLPehlichiTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNoPlayerRepair, "Gridlands.Game.Pehlichi.PlayerHasNoWayToRepair", GLTestUtils::Flags)
bool FGLNoPlayerRepair::RunTest(const FString& Parameters)
{
	FPehlichiScene Scene;
	AGLGlitch* Glitch = Scene.Glitch(PehlichiTestLamp);
	if (!TestNotNull(TEXT("the lamp glitch spawned from its placement"), Glitch))
	{
		return false;
	}
	TestFalse(TEXT("a glitch actor is not interactable"), Glitch->Implements<UGLInteractable>());
	for (UActorComponent* Component : Glitch->GetComponents())
	{
		TestFalse(FString::Printf(TEXT("no glitch component is interactable (%s)"), *Component->GetName()), Component->Implements<UGLInteractable>());
	}
	TestEqual(TEXT("glitches never block or catch interaction traces"), static_cast<int32>(Glitch->GetActorEnableCollision() && Glitch->GetRootComponent()->IsCollisionEnabled()), 0);
	TestEqual(TEXT("spawned Latent"), Scene.State(PehlichiTestLamp), EGLGlitchState::Latent);
	TestTrue(TEXT("Latent glitches are invisible"), !Cast<UPrimitiveComponent>(Glitch->GetRootComponent())->IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLScanRules, "Gridlands.Game.Pehlichi.ScanRevealsByRangeAndLevel", GLTestUtils::Flags)
bool FGLScanRules::RunTest(const FString& Parameters)
{
	FPehlichiScene Scene;
	const FVector LampAt = Scene.Glitch(PehlichiTestLamp)->GetActorLocation();
	Scene.Place(Scene.Pehlichi, LampAt + FVector(300, 0, 0));
	const FGLScanResult Result = Scene.Pehlichi->GetScan()->Scan();
	TestEqual(TEXT("level-1 scan reveals the nearby lamp"), Scene.State(PehlichiTestLamp), EGLGlitchState::Detected);
	TestTrue(TEXT("and reports it"), Result.NewlyRevealed == 1 && Result.Findings.Num() == 1);
	TestEqual(TEXT("the transformer is out of range"), Scene.State(PehlichiTestTransformer), EGLGlitchState::Latent);
	TestTrue(TEXT("revealed glitches become visible"), Cast<UPrimitiveComponent>(Scene.Glitch(PehlichiTestLamp)->GetRootComponent())->IsVisible());

	Scene.Place(Scene.Pehlichi, Scene.Glitch(PehlichiTestBuried)->GetActorLocation() + FVector(100, 0, 0));
	Scene.Pehlichi->GetScan()->Scan();
	TestEqual(TEXT("a level-2 glitch stays hidden from a level-1 scan, even point-blank"), Scene.State(PehlichiTestBuried), EGLGlitchState::Latent);
	TestTrue(TEXT("scans are announced"), Scene.Events.ContainsByPredicate([](const FString& E) { return E.StartsWith(TEXT("Event.Pehlichi.Scanned")); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLLoopSteps5To12, "Gridlands.Game.Pehlichi.LampLoopRevealPrepareRepairUpgrade", GLTestUtils::Flags)
bool FGLLoopSteps5To12::RunTest(const FString& Parameters)
{
	// First-playable loop steps 5-12 on the flickering lamp.
	FPehlichiScene Scene;
	const FVector LampAt = Scene.Glitch(PehlichiTestLamp)->GetActorLocation();
	Scene.Place(Scene.Zenny, LampAt + FVector(0, -400, 0));
	Scene.Place(Scene.Pehlichi, LampAt + FVector(0, -500, 0));
	UGLPehlichiCommandComponent* Commands = Scene.Pehlichi->GetCommands();

	TestEqual(TEXT("5: the glitch is not visible at first"), Scene.State(PehlichiTestLamp), EGLGlitchState::Latent);
	TestEqual(TEXT("before a scan there is nothing to repair"), Commands->Issue(TEXT("Command.Pehlichi.Repair"), Scene.Zenny), EGLCommandRejection::NothingRevealedNearby);
	TestEqual(TEXT("6: Zenny commands a scan"), Commands->Issue(TEXT("Command.Pehlichi.Scan"), Scene.Zenny), EGLCommandRejection::None);
	TestEqual(TEXT("7: Pehlichi reveals it"), Scene.State(PehlichiTestLamp), EGLGlitchState::Detected);

	Scene.Run(0.5);
	TestEqual(TEXT("the blocker is still there: requirements unmet"), Scene.State(PehlichiTestLamp), EGLGlitchState::Detected);
	TestEqual(TEXT("Pehlichi refuses and says why"), Commands->Issue(TEXT("Command.Pehlichi.Repair"), Scene.Zenny), EGLCommandRejection::RequirementsUnmet);
	TestTrue(TEXT("the refusal is an event (banter hook)"), Scene.Events.Contains(TEXT("Event.Pehlichi.CommandRejected.RequirementsUnmet:Command.Pehlichi.Repair")));

	Scene.Salvage(TEXT("placement.origin.junk_pile_01"));
	Scene.Run(0.5);
	TestEqual(TEXT("8: Zenny salvaged the blocker; the glitch is repairable"), Scene.State(PehlichiTestLamp), EGLGlitchState::Repairable);

	TestEqual(TEXT("9: Zenny commands the repair"), Commands->Issue(TEXT("Command.Pehlichi.Repair"), Scene.Zenny), EGLCommandRejection::None);
	Scene.Run(1.5);
	TestEqual(TEXT("10: Pehlichi walked over and is repairing"), Scene.State(PehlichiTestLamp), EGLGlitchState::Repairing);
	TestEqual(TEXT("a second repair command while busy is refused"), Commands->Issue(TEXT("Command.Pehlichi.Repair"), Scene.Zenny), EGLCommandRejection::Busy);
	Scene.Run(6.0);
	TestEqual(TEXT("11: repaired"), Scene.State(PehlichiTestLamp), EGLGlitchState::Repaired);
	TestTrue(TEXT("Event.Glitch.Repaired"), Scene.Events.Contains(TEXT("Event.Glitch.Repaired:glitch.home.flicker_lamp")));
	TestEqual(TEXT("12: Pehlichi's scan permanently improved to level 2"), Scene.Pehlichi->GetCapabilities()->Level(PehlichiTestScanCapability), 2);

	// The upgrade matters: the level-2 glitch is now findable.
	Scene.Place(Scene.Pehlichi, Scene.Glitch(PehlichiTestBuried)->GetActorLocation() + FVector(100, 0, 0));
	Scene.Pehlichi->GetScan()->Scan();
	TestEqual(TEXT("the upgraded scan reveals the buried signal"), Scene.State(PehlichiTestBuried), EGLGlitchState::Detected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDeliveryAndInterruption, "Gridlands.Game.Pehlichi.ItemDeliveryInterruptAndResume", GLTestUtils::Flags)
bool FGLDeliveryAndInterruption::RunTest(const FString& Parameters)
{
	FPehlichiScene Scene;
	const FVector At = Scene.Glitch(PehlichiTestTransformer)->GetActorLocation();
	Scene.Place(Scene.Zenny, At + FVector(0, -300, 0));
	Scene.Place(Scene.Pehlichi, At + FVector(0, -400, 0));
	UGLPehlichiCommandComponent* Commands = Scene.Pehlichi->GetCommands();
	Commands->Issue(TEXT("Command.Pehlichi.Scan"), Scene.Zenny);
	Scene.Run(0.5);
	TestEqual(TEXT("without a fuse it is not repairable"), Scene.State(PehlichiTestTransformer), EGLGlitchState::Detected);

	Scene.Inventory->AddItem(TEXT("item.part.fuse"), 1);
	Scene.Run(0.5);
	TestEqual(TEXT("carrying the fuse makes it repairable"), Scene.State(PehlichiTestTransformer), EGLGlitchState::Repairable);
	Commands->Issue(TEXT("Command.Pehlichi.Repair"), Scene.Zenny);
	Scene.Run(4.0); // ~1 s to walk over, then ~3 s of repair
	TestEqual(TEXT("repairing"), Scene.State(PehlichiTestTransformer), EGLGlitchState::Repairing);
	TestEqual(TEXT("Pehlichi took the fuse at repair start"), Scene.Inventory->CountOf(TEXT("item.part.fuse")), 0);

	const double Before = Scene.Glitch(PehlichiTestTransformer)->GetGlitch()->GetProgressSeconds();
	Commands->Issue(TEXT("Command.Pehlichi.Follow"), Scene.Zenny);
	TestEqual(TEXT("recalling Pehlichi interrupts the repair"), Scene.State(PehlichiTestTransformer), EGLGlitchState::Interrupted);
	Scene.Run(1.0);
	TestEqual(TEXT("the delivered fuse still counts: it does not fall back to Detected"), Scene.State(PehlichiTestTransformer), EGLGlitchState::Interrupted);
	TestEqual(TEXT("KeepProgress keeps progress"), Scene.Glitch(PehlichiTestTransformer)->GetGlitch()->GetProgressSeconds(), Before);

	TestEqual(TEXT("resume"), Commands->Issue(TEXT("Command.Pehlichi.Repair"), Scene.Zenny), EGLCommandRejection::None);
	Scene.Run(10.0);
	TestEqual(TEXT("completes after resuming"), Scene.State(PehlichiTestTransformer), EGLGlitchState::Repaired);
	TestTrue(TEXT("its knowledge reward was granted"), Scene.Test.World->GetSubsystem<UGLKnowledgeSubsystem>()->Knows(TEXT("knowledge.style.modern_timber_frame")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
