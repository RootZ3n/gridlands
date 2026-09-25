#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Events/GLEventSubsystem.h"
#include "Fabrication/GLFabricatorComponent.h"
#include "Fabrication/GLStationComponent.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Tests/GLTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLFabricationTests
{
	struct FCrafter
	{
		AActor* Actor;
		UGLInventoryComponent* Inventory;
		UGLFabricatorComponent* Fabricator;
	};

	FCrafter SpawnCrafter(UWorld* World)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UGLInventoryComponent* Inventory = NewObject<UGLInventoryComponent>(Actor);
		UGLFabricatorComponent* Fabricator = NewObject<UGLFabricatorComponent>(Actor);
		Inventory->RegisterComponent();
		Fabricator->RegisterComponent();
		return { Actor, Inventory, Fabricator };
	}

	int32 Salvage(UWorld* World, const TCHAR* SalvageId, AActor* Player)
	{
		AGLSalvageNode* Node = World->SpawnActor<AGLSalvageNode>(FVector(400, 0, 0), FRotator::ZeroRotator);
		Node->GetSalvageable()->Setup(SalvageId);
		int32 Hits = 0;
		while (!Node->GetSalvageable()->IsSalvaged() && Hits < 100)
		{
			Node->GetSalvageable()->Interact(Player, GLTestUtils::Tag(TEXT("Interact.Salvage")));
			++Hits;
		}
		return Hits;
	}
}

using namespace GLFabricationTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLLoopSteps2To4, "Gridlands.Game.Fabrication.PryBarFromSalvagedScrap", GLTestUtils::Flags)
bool FGLLoopSteps2To4::RunTest(const FString& Parameters)
{
	// First-playable loop steps 2-4: salvage by hand, fabricate a primitive tool, salvage faster with it.
	GLTestUtils::FTestWorld Test(TEXT("GLFabricationTestWorld"));
	FCrafter Zenny = SpawnCrafter(Test.World);
	TArray<FName> Fabricated;
	Test.World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event.Item.Fabricated")),
		FGLGameplayEventDelegate::CreateLambda([&](const FGLGameplayEvent& E) { Fabricated.Add(E.Subject); }));

	TestTrue(TEXT("nothing to make at the start"), Zenny.Fabricator->CraftableRecipes().Num() == 0);
	const int32 BareHits = Salvage(Test.World, TEXT("salvage.yard.fence_panel"), Zenny.Actor);
	Salvage(Test.World, TEXT("salvage.yard.junk_pile"), Zenny.Actor); // 4 scrap
	TestTrue(TEXT("scrap makes the pry bar available"), Zenny.Fabricator->CraftableRecipes().Contains(TEXT("recipe.tool.pry_bar")));
	TestTrue(TEXT("fabricate the pry bar"), Zenny.Fabricator->Fabricate(TEXT("recipe.tool.pry_bar")).CanCraft());
	TestEqual(TEXT("holding the pry bar"), Zenny.Inventory->CountOf(TEXT("item.tool.pry_bar")), 1);
	TestEqual(TEXT("one scrap left"), Zenny.Inventory->CountOf(TEXT("item.material.scrap_metal")), 1);
	TestTrue(TEXT("Event.Item.Fabricated names the tool"), Fabricated.Contains(TEXT("item.tool.pry_bar")));

	const int32 ToolHits = Salvage(Test.World, TEXT("salvage.yard.fence_panel"), Zenny.Actor);
	TestTrue(FString::Printf(TEXT("the tool salvages the next fence faster (%d -> %d hits)"), BareHits, ToolHits), ToolHits < BareHits);
	TestEqual(TEXT("fence: 4 hits bare, 2 with the pry bar"), BareHits * 10 + ToolHits, 42);

	FCrafter Broke = SpawnCrafter(Test.World);
	const FGLCraftCheck Refused = Broke.Fabricator->Fabricate(TEXT("recipe.tool.pry_bar"));
	TestTrue(TEXT("without scrap the craft is refused with a reason"), Refused.Block == EGLCraftBlock::MissingInputs);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLKnowledgeSources, "Gridlands.Game.Knowledge.UnlocksFromDataDrivenSources", GLTestUtils::Flags)
bool FGLKnowledgeSources::RunTest(const FString& Parameters)
{
	GLTestUtils::FTestWorld Test(TEXT("GLKnowledgeTestWorld"));
	UGLKnowledgeSubsystem* Knowledge = Test.World->GetSubsystem<UGLKnowledgeSubsystem>();
	if (!TestNotNull(TEXT("knowledge subsystem"), Knowledge))
	{
		return false;
	}
	TArray<FName> Unlocked;
	Test.World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event.Knowledge.Unlocked")),
		FGLGameplayEventDelegate::CreateLambda([&](const FGLGameplayEvent& E) { Unlocked.Add(E.Subject); }));
	FCrafter Zenny = SpawnCrafter(Test.World);

	Zenny.Inventory->AddItem(TEXT("item.material.copper_wire"), 1);
	TestTrue(TEXT("first copper wire teaches its uses (onAcquireUnlocks)"), Knowledge->Knows(TEXT("knowledge.material.copper_wire")));
	Salvage(Test.World, TEXT("salvage.yard.fence_panel"), Zenny.Actor);
	TestTrue(TEXT("salvaging a fence teaches timber framing (onSalvageUnlocks)"), Knowledge->Knows(TEXT("knowledge.style.modern_timber_frame")));
	TestEqual(TEXT("each unlock announced once"), Unlocked.Num(), 2);
	Zenny.Inventory->AddItem(TEXT("item.material.copper_wire"), 1);
	TestEqual(TEXT("re-acquiring does not re-announce"), Unlocked.Num(), 2);
	TestFalse(TEXT("unknown knowledge ids are refused"), Knowledge->Learn(TEXT("knowledge.style.nope")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStationReach, "Gridlands.Game.Fabrication.StationsInReach", GLTestUtils::Flags)
bool FGLStationReach::RunTest(const FString& Parameters)
{
	GLTestUtils::FTestWorld Test(TEXT("GLStationTestWorld"));
	FCrafter Zenny = SpawnCrafter(Test.World);
	AActor* Bench = Test.World->SpawnActor<AActor>();
	UGLStationComponent* Station = NewObject<UGLStationComponent>(Bench);
	Station->StationTag = TEXT("Station.Workbench");
	Station->RegisterComponent();
	USceneComponent* Root = NewObject<USceneComponent>(Bench);
	Bench->SetRootComponent(Root);
	Root->RegisterComponent();

	Bench->SetActorLocation(FVector(200, 0, 0));
	TestTrue(TEXT("a bench within reach counts"), Zenny.Fabricator->StationsInReach().Contains(TEXT("Station.Workbench")));
	Bench->SetActorLocation(FVector(2000, 0, 0));
	TestEqual(TEXT("a bench out of reach does not"), Zenny.Fabricator->StationsInReach().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPreferredRecipe, "Gridlands.Game.Fabrication.FMakesWhatZennyLacks", GLTestUtils::Flags)
bool FGLPreferredRecipe::RunTest(const FString& Parameters)
{
	// With enough for either tool, F makes the critical-path pry bar first (lexical order), and
	// then the shovel rather than a second pry bar.
	GLTestUtils::FTestWorld Test(TEXT("GLPreferredRecipeWorld"));
	FCrafter Zenny = SpawnCrafter(Test.World);
	Zenny.Inventory->AddItem(TEXT("item.material.scrap_metal"), 5);
	Zenny.Inventory->AddItem(TEXT("item.material.timber_plank"), 1);
	TestEqual(TEXT("first: the pry bar"), Zenny.Fabricator->PreferredRecipe(), FName(TEXT("recipe.tool.pry_bar")));
	Zenny.Fabricator->Fabricate(TEXT("recipe.tool.pry_bar"));
	TestEqual(TEXT("then: the shovel, not a second pry bar"), Zenny.Fabricator->PreferredRecipe(), FName(TEXT("recipe.tool.shovel")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
