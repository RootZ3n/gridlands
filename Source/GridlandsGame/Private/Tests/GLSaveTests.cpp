#include "Content/GLContent.h"
#include "Dialogue/GLDialogueDirector.h"
#include "Events/GLEventSubsystem.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "HAL/FileManager.h"
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
#include "Save/GLSaveSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLPlacementSubsystem.h"
#include "World/GLStabilitySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLSaveTests
{
	/** A playable origin: placements, a commander with an inventory, Pehlichi. */
	struct FSaveScene
	{
		GLTestUtils::FTestWorld Test;
		AActor* Zenny = nullptr;
		UGLInventoryComponent* Inventory = nullptr;
		AGLPehlichi* Pehlichi = nullptr;
		UGLGlitchSubsystem* Glitches = nullptr;

		explicit FSaveScene(const TCHAR* Name) : Test(Name)
		{
			UWorld* World = Test.World;
			World->GetSubsystem<UGLPlacementSubsystem>()->SpawnCell(TEXT("cell.home.origin"));
			Glitches = World->GetSubsystem<UGLGlitchSubsystem>();
			Zenny = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Zenny);
			Zenny->SetRootComponent(Root);
			Root->RegisterComponent();
			Inventory = NewObject<UGLInventoryComponent>(Zenny);
			Inventory->RegisterComponent();
			Glitches->SetCommander(Zenny);
			Pehlichi = World->SpawnActor<AGLPehlichi>(FVector::ZeroVector, FRotator::ZeroRotator);
			World->GetSubsystem<UGLDialogueDirector>()->bShowOnScreen = false;
		}

		UGLSaveSubsystem* Saves() const { return Test.World->GetSubsystem<UGLSaveSubsystem>(); }
		AGLGlitch* Glitch(const TCHAR* Id) const { return Glitches->FindByPlacement(Id); }

		void Run(double Seconds)
		{
			for (double T = 0.0; T < Seconds; T += 0.1)
			{
				Glitches->EvaluateRequirements();
				Pehlichi->GetPositioning()->Advance(0.1f);
				Pehlichi->GetRepair()->Advance(0.1f);
			}
		}

		void RepairLamp()
		{
			const FVector At = Glitch(TEXT("placement.origin.glitch_flicker_lamp"))->GetActorLocation();
			Zenny->SetActorLocation(At + FVector(0, -300, 0));
			Pehlichi->SetActorLocation(At + FVector(0, -200, 0));
			Pehlichi->GetScan()->Scan();
			AGLSalvageNode* Blocker = Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindSalvageNode(TEXT("placement.origin.junk_pile_01"));
			while (!Blocker->GetSalvageable()->IsSalvaged())
			{
				Blocker->GetSalvageable()->Interact(Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
			}
			Run(0.3);
			Pehlichi->GetCommands()->Issue(TEXT("Command.Pehlichi.Repair"), Zenny);
			Run(8.0);
		}
	};

	const FString TestSlot = TEXT("automation-test");
}

using namespace GLSaveTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLLoopSteps13To15, "Gridlands.Game.Save.WorldStaysChangedAcrossReload", GLTestUtils::Flags)
bool FGLLoopSteps13To15::RunTest(const FString& Parameters)
{
	// First-playable loop steps 13-15: save, reload into a fresh world, the world is still changed.
	const TCHAR* Lamp = TEXT("placement.origin.glitch_flicker_lamp");
	double InterferenceBefore = 0.0;
	{
		FSaveScene Played(TEXT("GLSavePlayed"));
		Played.RepairLamp();
		TestEqual(TEXT("played: lamp repaired"), Played.Glitch(Lamp)->GetGlitch()->GetState(), EGLGlitchState::Repaired);
		Played.Inventory->AddItem(TEXT("item.tool.pry_bar"), 1);
		InterferenceBefore = Played.Test.World->GetSubsystem<UGLStabilitySubsystem>()->InterferenceAt(Played.Glitch(Lamp)->GetActorLocation());
		TestTrue(TEXT("13: saved"), Played.Saves()->SaveToSlot(TestSlot));
	}
	FSaveScene Reloaded(TEXT("GLSaveReloaded"));
	TestEqual(TEXT("a fresh world starts with the lamp latent"), Reloaded.Glitch(Lamp)->GetGlitch()->GetState(), EGLGlitchState::Latent);
	TArray<FString> Problems;
	TestTrue(TEXT("14: loaded"), Reloaded.Saves()->LoadFromSlot(TestSlot, &Problems));
	TestEqual(TEXT("no load problems"), Problems.Num(), 0);
	TestEqual(TEXT("15: the lamp is still repaired"), Reloaded.Glitch(Lamp)->GetGlitch()->GetState(), EGLGlitchState::Repaired);
	TestEqual(TEXT("15: Pehlichi's scan upgrade persisted"), Reloaded.Pehlichi->GetCapabilities()->Level(TEXT("capability.pehlichi.scan")), 2);
	TestTrue(TEXT("the blocker stays salvaged"), Reloaded.Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindSalvageNode(TEXT("placement.origin.junk_pile_01"))->GetSalvageable()->IsSalvaged());
	TestEqual(TEXT("inventory persisted"), Reloaded.Inventory->CountOf(TEXT("item.tool.pry_bar")), 1);
	TestTrue(TEXT("scrap from the salvage persisted"), Reloaded.Inventory->CountOf(TEXT("item.material.scrap_metal")) == 4);
	TestTrue(TEXT("dialogue history persisted (the opening will not replay)"),
		Reloaded.Test.World->GetSubsystem<UGLDialogueDirector>()->GetState().EventCounts.FindRef(TEXT("Event.Glitch.Repaired")) == 1);
	const double InterferenceAfter = Reloaded.Test.World->GetSubsystem<UGLStabilitySubsystem>()->InterferenceAt(Reloaded.Glitch(Lamp)->GetActorLocation());
	TestTrue(FString::Printf(TEXT("derived stability is recomputed identically from facts (%.3f vs %.3f)"), InterferenceBefore, InterferenceAfter), FMath::IsNearlyEqual(InterferenceBefore, InterferenceAfter, 1e-9));
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(TestSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSaveMidRepair, "Gridlands.Game.Save.MidRepairLoadsAsInterrupted", GLTestUtils::Flags)
bool FGLSaveMidRepair::RunTest(const FString& Parameters)
{
	const TCHAR* Transformer = TEXT("placement.origin.glitch_dead_transformer");
	double Progress = 0.0;
	{
		FSaveScene Played(TEXT("GLSaveMidPlayed"));
		const FVector At = Played.Glitch(Transformer)->GetActorLocation();
		Played.Zenny->SetActorLocation(At + FVector(0, -300, 0));
		Played.Pehlichi->SetActorLocation(At + FVector(0, -150, 0));
		Played.Pehlichi->GetScan()->Scan();
		Played.Inventory->AddItem(TEXT("item.part.fuse"), 1);
		Played.Run(0.3);
		Played.Pehlichi->GetCommands()->Issue(TEXT("Command.Pehlichi.Repair"), Played.Zenny);
		Played.Run(3.0);
		TestEqual(TEXT("mid-repair"), Played.Glitch(Transformer)->GetGlitch()->GetState(), EGLGlitchState::Repairing);
		Progress = Played.Glitch(Transformer)->GetGlitch()->GetProgressSeconds();
		Played.Saves()->SaveToSlot(TestSlot);
	}
	FSaveScene Reloaded(TEXT("GLSaveMidReloaded"));
	Reloaded.Saves()->LoadFromSlot(TestSlot);
	UGLGlitchComponent* Glitch = Reloaded.Glitch(Transformer)->GetGlitch();
	TestEqual(TEXT("a repair nobody is doing loads as Interrupted"), Glitch->GetState(), EGLGlitchState::Interrupted);
	TestEqual(TEXT("progress kept"), Glitch->GetProgressSeconds(), Progress);
	TestTrue(TEXT("the fuse stays delivered (not demanded twice)"), Glitch->AreItemsDelivered());
	TestEqual(TEXT("and the fuse is not back in the inventory"), Reloaded.Inventory->CountOf(TEXT("item.part.fuse")), 0);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(TestSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSaveStale, "Gridlands.Game.Save.StaleIdsAreReportedNotFatal", GLTestUtils::Flags)
bool FGLSaveStale::RunTest(const FString& Parameters)
{
	FGLWorldSave Save;
	Save.Glitches.Add({ TEXT("placement.origin.glitch_removed_in_a_patch"), EGLGlitchState::Repaired, 0.0, false });
	Save.Glitches.Add({ TEXT("placement.origin.glitch_flicker_lamp"), EGLGlitchState::Repaired, 5.0, false });
	Save.Inventory.Add({ TEXT("item.material.removed_item"), 3 });
	Save.Inventory.Add({ TEXT("item.material.copper_wire"), 7 });
	FSaveScene Scene(TEXT("GLSaveStale"));
	TArray<FString> Problems;
	AddExpectedMessagePlain(TEXT("Load: saved glitch placement"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("Load: saved item"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Scene.Saves()->Apply(Save, &Problems);
	TestEqual(TEXT("both stale ids reported"), Problems.Num(), 2);
	TestEqual(TEXT("everything else still applied (glitch)"), Scene.Glitch(TEXT("placement.origin.glitch_flicker_lamp"))->GetGlitch()->GetState(), EGLGlitchState::Repaired);
	TestEqual(TEXT("everything else still applied (inventory)"), Scene.Inventory->CountOf(TEXT("item.material.copper_wire")), 7);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
