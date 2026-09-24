#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Economy/GLWorldSettingsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/GLTestUtils.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "World/GLPlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLM4Tests
{
	/** A bare actor with an inventory: the salvaging "player". */
	UGLInventoryComponent* SpawnSalvager(UWorld* World)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UGLInventoryComponent* Inventory = NewObject<UGLInventoryComponent>(Actor);
		Inventory->RegisterComponent();
		return Inventory;
	}

	AGLSalvageNode* SpawnNode(UWorld* World, const TCHAR* SalvageId)
	{
		AGLSalvageNode* Node = World->SpawnActor<AGLSalvageNode>(FVector(500, 0, 0), FRotator::ZeroRotator);
		Node->GetSalvageable()->Setup(SalvageId);
		return Node;
	}

	int32 HitUntilSalvaged(UGLSalvageableComponent* Salvageable, AActor* Player)
	{
		int32 Hits = 0;
		while (!Salvageable->IsSalvaged() && Hits < 100)
		{
			Salvageable->Interact(Player, Tag(TEXT("Interact.Salvage")));
			++Hits;
		}
		return Hits;
	}
}

using namespace GLM4Tests;
using namespace GLTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSalvageCompletes, "Gridlands.Game.Salvage.CompletesGrantsYieldsAndEvents", Flags)
bool FGLSalvageCompletes::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	UGLInventoryComponent* Inventory = SpawnSalvager(Test.World);
	AGLSalvageNode* Node = SpawnNode(Test.World, TEXT("salvage.house.wiring_run"));
	TArray<FString> Events;
	Test.World->GetSubsystem<UGLEventSubsystem>()->Subscribe(Tag(TEXT("Event")), FGLGameplayEventDelegate::CreateLambda([&](const FGLGameplayEvent& E)
	{
		Events.Add(E.Tag.ToString() + TEXT(":") + E.Subject.ToString());
	}));

	TArray<FGLInteractionOption> Options;
	Node->GetSalvageable()->GetInteractionOptions(Inventory->GetOwner(), Options);
	TestTrue(TEXT("offers Interact.Salvage"), Options.Num() == 1 && Options[0].bEnabled && Options[0].Verb == Tag(TEXT("Interact.Salvage")));
	TestEqual(TEXT("bare hands: three hits"), HitUntilSalvaged(Node->GetSalvageable(), Inventory->GetOwner()), 3);

	TestEqual(TEXT("5 copper wire at default settings"), Inventory->CountOf(TEXT("item.material.copper_wire")), 5);
	TestEqual(TEXT("1 fuse"), Inventory->CountOf(TEXT("item.part.fuse")), 1);
	TestTrue(TEXT("Event.Salvage.Completed"), Events.Contains(TEXT("Event.Salvage.Completed:salvage.house.wiring_run")));
	TestTrue(TEXT("data-driven Event.Salvage.WireStripped (dialogue hook)"), Events.Contains(TEXT("Event.Salvage.WireStripped:salvage.house.wiring_run")));
	TestTrue(TEXT("Event.Item.Acquired for the wire"), Events.Contains(TEXT("Event.Item.Acquired:item.material.copper_wire")));
	TestTrue(TEXT("the node is gone"), Node->IsHidden() && !Node->GetActorEnableCollision());
	TestFalse(TEXT("a salvaged node cannot be salvaged again"), Node->GetSalvageable()->Interact(Inventory->GetOwner(), Tag(TEXT("Interact.Salvage"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSalvageSettings, "Gridlands.Game.Salvage.WorldSettingsScaleRepeatableYields", Flags)
bool FGLSalvageSettings::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	TestTrue(TEXT("switch to the relaxed preset"), Test.World->GetSubsystem<UGLWorldSettingsSubsystem>()->SetPreset(TEXT("settings.preset.relaxed")));
	TestFalse(TEXT("unknown presets are refused"), Test.World->GetSubsystem<UGLWorldSettingsSubsystem>()->SetPreset(TEXT("settings.preset.cheat")));
	UGLInventoryComponent* Inventory = SpawnSalvager(Test.World);
	AGLSalvageNode* Node = SpawnNode(Test.World, TEXT("salvage.yard.junk_pile"));
	HitUntilSalvaged(Node->GetSalvageable(), Inventory->GetOwner());
	TestEqual(TEXT("relaxed (x2) doubles repeatable common salvage: 4 -> 8"), Inventory->CountOf(TEXT("item.material.scrap_metal")), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSalvagePryBar, "Gridlands.Game.Salvage.PryBarSalvagesFaster", Flags)
bool FGLSalvagePryBar::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	UGLInventoryComponent* Bare = SpawnSalvager(Test.World);
	UGLInventoryComponent* Equipped = SpawnSalvager(Test.World);
	Equipped->AddItem(TEXT("item.tool.pry_bar"), 1);
	const int32 BareHits = HitUntilSalvaged(SpawnNode(Test.World, TEXT("salvage.yard.junk_pile"))->GetSalvageable(), Bare->GetOwner());
	const int32 PryHits = HitUntilSalvaged(SpawnNode(Test.World, TEXT("salvage.yard.junk_pile"))->GetSalvageable(), Equipped->GetOwner());
	TestEqual(TEXT("bare hands: 6 hits"), BareHits, 6);
	TestEqual(TEXT("with the pry bar: 3 hits"), PryHits, 3);
	TestEqual(TEXT("both get the same yield"), Bare->CountOf(TEXT("item.material.scrap_metal")), Equipped->CountOf(TEXT("item.material.scrap_metal")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLOverencumbered, "Gridlands.Game.Inventory.OverencumbranceIsAnnouncedOnce", Flags)
bool FGLOverencumbered::RunTest(const FString& Parameters)
{
	FTestWorld Test;
	UGLInventoryComponent* Inventory = SpawnSalvager(Test.World);
	int32 Announced = 0;
	Test.World->GetSubsystem<UGLEventSubsystem>()->Subscribe(Tag(TEXT("Event.Player.Overencumbered")), FGLGameplayEventDelegate::CreateLambda([&](const FGLGameplayEvent&) { ++Announced; }));
	Inventory->AddItem(TEXT("item.material.cut_stone"), 30); // 90
	TestEqual(TEXT("not yet"), Announced, 0);
	Inventory->AddItem(TEXT("item.material.cut_stone"), 30); // 180 > 150
	TestTrue(TEXT("now overencumbered"), Inventory->IsOverencumbered());
	TestEqual(TEXT("announced once"), Announced, 1);
	Inventory->AddItem(TEXT("item.material.cut_stone"), 5);
	TestEqual(TEXT("not re-announced while still overencumbered"), Announced, 1);
	Inventory->RemoveItem(TEXT("item.material.cut_stone"), 40);
	TestFalse(TEXT("relieved"), Inventory->IsOverencumbered());
	Inventory->AddItem(TEXT("item.material.cut_stone"), 40);
	TestEqual(TEXT("announced again after relief"), Announced, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPlacementsSpawn, "Gridlands.Game.Placement.SpawnsSalvageNodesFromData", Flags)
bool FGLPlacementsSpawn::RunTest(const FString& Parameters)
{
	FTestWorld Test; // no map: anchored placements resolve through the exported anchor records
	UGLPlacementSubsystem* Placements = Test.World->GetSubsystem<UGLPlacementSubsystem>();
	// Every supported placement of the cell spawns (salvage nodes since M4, glitches since M7).
	int32 Supported = 0, Salvage = 0;
	GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLPlacementDef* P = Entry.Definition.GetPtr<FGLPlacementDef>();
		if (P && Entry.Id.ToString().StartsWith(TEXT("placement.origin.")))
		{
			Supported += (P->Kind == TEXT("salvage_node") || P->Kind == TEXT("glitch")) ? 1 : 0;
			Salvage += P->Kind == TEXT("salvage_node") ? 1 : 0;
		}
	});
	TestEqual(TEXT("every supported origin placement spawns"), Placements->SpawnCell(TEXT("cell.home.origin")), Supported);
	TestEqual(TEXT("including the three salvage nodes"), Salvage, 3);

	const AGLSalvageNode* Junk = Placements->FindSalvageNode(TEXT("placement.origin.junk_pile_01"));
	TestTrue(TEXT("transform placement at its authored location"), Junk && Junk->GetActorLocation().Equals(FVector(1200, 300, 0)));
	const AGLSalvageNode* Fence = Placements->FindSalvageNode(TEXT("placement.origin.fence_panel_01"));
	const FGLAnchorRecord* FenceAnchor = GLContent::Get().FindAnchor(TEXT("anchor.origin.fence_01"));
	TestTrue(TEXT("anchored placement at its anchor"), Fence && FenceAnchor && Fence->GetActorLocation().Equals(FenceAnchor->Location));
	const AGLSalvageNode* Wiring = Placements->FindSalvageNode(TEXT("placement.origin.wiring_run_house_01"));
	const FGLAnchorRecord* House = GLContent::Get().FindAnchor(TEXT("anchor.origin.house_01"));
	TestTrue(TEXT("anchored placement applies its offset"), Wiring && House && Wiring->GetActorLocation().Equals(House->Location + FVector(0, -520, -150)));
	TestEqual(TEXT("nodes carry their placement id"), Junk ? Junk->PlacementId : NAME_None, FName(TEXT("placement.origin.junk_pile_01")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
