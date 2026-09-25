#include "Content/GLContent.h"
#include "Dialogue/GLDialogueDirector.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "HAL/FileManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Pehlichi/GLCapabilityComponent.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
#include "Pehlichi/GLRepairComponent.h"
#include "Pehlichi/GLScanComponent.h"
#include "Puzzle/GLPuzzleSite.h"
#include "Puzzle/GLPuzzleSubsystem.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLPlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLPuzzleTests
{
	const FName Riddle(TEXT("puzzle.home.map_riddle"));
	const TCHAR* Cartographer = TEXT("placement.origin.glitch_cartographer");

	/** The origin with Zenny, Pehlichi, and a record of every puzzle event. */
	struct FPuzzleScene
	{
		GLTestUtils::FTestWorld Test;
		AActor* Zenny = nullptr;
		UGLInventoryComponent* Inventory = nullptr;
		AGLPehlichi* Pehlichi = nullptr;
		UGLGlitchSubsystem* Glitches = nullptr;
		UGLPuzzleSubsystem* Puzzles = nullptr;
		TArray<FName> Events;

		explicit FPuzzleScene(const TCHAR* Name) : Test(Name)
		{
			UWorld* World = Test.World;
			World->GetSubsystem<UGLPlacementSubsystem>()->SpawnCell(TEXT("cell.home.origin"));
			Glitches = World->GetSubsystem<UGLGlitchSubsystem>();
			Puzzles = World->GetSubsystem<UGLPuzzleSubsystem>();
			Zenny = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Zenny);
			Zenny->SetRootComponent(Root);
			Root->RegisterComponent();
			Inventory = NewObject<UGLInventoryComponent>(Zenny);
			Inventory->RegisterComponent();
			Glitches->SetCommander(Zenny);
			Pehlichi = World->SpawnActor<AGLPehlichi>(FVector::ZeroVector, FRotator::ZeroRotator);
			World->GetSubsystem<UGLDialogueDirector>()->bShowOnScreen = false;
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event.Puzzle")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event) { Events.Add(Event.Tag.GetTagName()); }));
		}

		AGLGlitch* Glitch() const { return Glitches->FindByPlacement(Cartographer); }
		AGLPuzzleSite* Site() const
		{
			for (TActorIterator<AGLPuzzleSite> It(Test.World); It; ++It)
			{
				return *It;
			}
			return nullptr;
		}
		bool CanPresent() const
		{
			TArray<FGLInteractionOption> Options;
			Site()->GetInteractionOptions(Zenny, Options);
			return Options.Num() == 1 && Options[0].bEnabled;
		}
		void ScanCartographer()
		{
			const FVector At = Glitch()->GetActorLocation();
			Zenny->SetActorLocation(At + FVector(0, -300, 0));
			Pehlichi->SetActorLocation(At + FVector(0, -200, 0));
			Pehlichi->GetScan()->Scan();
		}
		void OpenGlovebox()
		{
			AGLSalvageNode* Glovebox = Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindSalvageNode(TEXT("placement.origin.glovebox_01"));
			while (!Glovebox->GetSalvageable()->IsSalvaged())
			{
				Glovebox->GetSalvageable()->Interact(Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
			}
		}
		void Run(double Seconds)
		{
			for (double T = 0.0; T < Seconds; T += 0.1)
			{
				Glitches->EvaluateRequirements();
				Pehlichi->GetPositioning()->Advance(0.1f);
				Pehlichi->GetRepair()->Advance(0.1f);
			}
		}
	};

	const FString PuzzleSlot = TEXT("automation-test-puzzle");
}

using namespace GLPuzzleTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPuzzlePresentProof, "Gridlands.Game.Puzzle.AnsweredByPresentingNotBySpeaking", GLTestUtils::Flags)
bool FGLPuzzlePresentProof::RunTest(const FString& Parameters)
{
	// ADR-0023 end to end: NICE poses, Pehlichi hints (capped), talk solves nothing, placing the map does.
	FPuzzleScene Scene(TEXT("GLPuzzlePresent"));
	TestNotNull(TEXT("the map stand was placed from data"), Scene.Site());
	TestFalse(TEXT("nothing is posed before Pehlichi reveals the glitch"), Scene.Puzzles->IsPosed(Riddle));

	Scene.ScanCartographer();
	TestEqual(TEXT("scan reveals the glitch"), Scene.Glitch()->GetGlitch()->GetState(), EGLGlitchState::Detected);
	TestTrue(TEXT("revealing it poses NICE's riddle"), Scene.Events.Contains(FName(TEXT("Event.Puzzle.Posed"))));
	Scene.Run(0.5);
	TestEqual(TEXT("an unsolved riddle blocks repair"), Scene.Glitch()->GetGlitch()->GetState(), EGLGlitchState::Detected);

	// Hints escalate and stop at Pehlichi's analysis (L1 -> insight 2).
	TestEqual(TEXT("hint 1"), Scene.Puzzles->RequestHint(Scene.Pehlichi), 1);
	TestEqual(TEXT("hint 2"), Scene.Puzzles->RequestHint(Scene.Pehlichi), 2);
	TestEqual(TEXT("analysis L1 cannot give hint 3"), Scene.Puzzles->RequestHint(Scene.Pehlichi), 0);
	TestTrue(TEXT("and says so"), Scene.Events.Contains(FName(TEXT("Event.Puzzle.Hint.Exhausted"))));
	TestFalse(TEXT("all that talk solved nothing"), Scene.Puzzles->IsSolved(Riddle));

	TestFalse(TEXT("the stand refuses empty hands"), Scene.CanPresent());
	TestFalse(TEXT("and interacting anyway does nothing"), Scene.Site()->Interact(Scene.Zenny, GLTestUtils::Tag(TEXT("Interact.Present"))));
	Scene.Inventory->AddItem(TEXT("item.material.scrap_metal"), 3);
	TestFalse(TEXT("the wrong item does not fit"), Scene.CanPresent());

	Scene.OpenGlovebox();
	TestEqual(TEXT("the glovebox held the map"), Scene.Inventory->CountOf(TEXT("item.misc.paper_map")), 1);
	TestTrue(TEXT("now the stand accepts it"), Scene.CanPresent());
	TestTrue(TEXT("placing the map"), Scene.Site()->Interact(Scene.Zenny, GLTestUtils::Tag(TEXT("Interact.Present"))));
	TestTrue(TEXT("solves the riddle"), Scene.Puzzles->IsSolved(Riddle));
	TestEqual(TEXT("the map is consumed"), Scene.Inventory->CountOf(TEXT("item.misc.paper_map")), 0);
	TestFalse(TEXT("solved once only"), Scene.Puzzles->Solve(Riddle));

	// Solving is the requirement; repair is still Pehlichi's alone.
	Scene.Run(0.3);
	TestEqual(TEXT("requirements met, waiting for Pehlichi"), Scene.Glitch()->GetGlitch()->GetState(), EGLGlitchState::Repairable);
	Scene.Pehlichi->GetCommands()->Issue(TEXT("Command.Pehlichi.Repair"), Scene.Zenny);
	Scene.Run(8.0);
	TestEqual(TEXT("Pehlichi repairs the cartographer's error"), Scene.Glitch()->GetGlitch()->GetState(), EGLGlitchState::Repaired);
	TestEqual(TEXT("and his analysis grows"), Scene.Pehlichi->GetCapabilities()->Level(TEXT("capability.pehlichi.analysis")), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPuzzleHintCapGrows, "Gridlands.Game.Puzzle.HintCapFollowsAnalysis", GLTestUtils::Flags)
bool FGLPuzzleHintCapGrows::RunTest(const FString& Parameters)
{
	FPuzzleScene Scene(TEXT("GLPuzzleHintCap"));
	TestEqual(TEXT("no puzzle posed: nothing to hint"), Scene.Puzzles->RequestHint(Scene.Pehlichi), 0);
	Scene.ScanCartographer();
	Scene.Pehlichi->GetCapabilities()->Grant(TEXT("capability.pehlichi.analysis"), 2);
	TestEqual(TEXT("hint 1"), Scene.Puzzles->RequestHint(Scene.Pehlichi), 1);
	TestEqual(TEXT("hint 2"), Scene.Puzzles->RequestHint(Scene.Pehlichi), 2);
	TestEqual(TEXT("analysis L2 reaches hint 3"), Scene.Puzzles->RequestHint(Scene.Pehlichi), 3);
	TestEqual(TEXT("there is no hint 4"), Scene.Puzzles->RequestHint(Scene.Pehlichi), 0);
	TestFalse(TEXT("even the last hint solves nothing"), Scene.Puzzles->IsSolved(Riddle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPuzzlePersists, "Gridlands.Game.Puzzle.SolvedStateSurvivesReload", GLTestUtils::Flags)
bool FGLPuzzlePersists::RunTest(const FString& Parameters)
{
	{
		FPuzzleScene Played(TEXT("GLPuzzlePlayed"));
		Played.ScanCartographer();
		Played.Puzzles->RequestHint(Played.Pehlichi);
		Played.OpenGlovebox();
		Played.Site()->Interact(Played.Zenny, GLTestUtils::Tag(TEXT("Interact.Present")));
		TestTrue(TEXT("saved"), Played.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(PuzzleSlot));
	}
	FPuzzleScene Reloaded(TEXT("GLPuzzleReloaded"));
	TArray<FString> Problems;
	TestTrue(TEXT("loaded"), Reloaded.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(PuzzleSlot, &Problems));
	TestEqual(TEXT("no problems"), Problems.Num(), 0);
	TestTrue(TEXT("the riddle stays solved"), Reloaded.Puzzles->IsSolved(Riddle));
	TestTrue(TEXT("and posed"), Reloaded.Puzzles->IsPosed(Riddle));
	TestEqual(TEXT("hint level kept"), Reloaded.Puzzles->HintLevel(Riddle), 1);
	TestFalse(TEXT("the glovebox stays empty"), Reloaded.CanPresent());
	Reloaded.Run(0.3);
	TestEqual(TEXT("the glitch is repairable after reload"), Reloaded.Glitch()->GetGlitch()->GetState(), EGLGlitchState::Repairable);
	TestFalse(TEXT("reload did not re-pose the riddle"), Reloaded.Events.Contains(FName(TEXT("Event.Puzzle.Posed"))));
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(PuzzleSlot));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
