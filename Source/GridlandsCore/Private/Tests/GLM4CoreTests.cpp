#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Economy/GLYield.h"
#include "Inventory/GLInventory.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Salvage/GLSalvageRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLM4CoreTests
{
	constexpr EAutomationTestFlags M4CoreFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	const FGLContentRegistry& Content()
	{
		static FGLContentRegistry Registry;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			Registry.LoadRepository(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
			bLoaded = true;
		}
		return Registry;
	}
}

using namespace GLM4CoreTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLInventoryRules, "Gridlands.Core.Inventory.StacksAndCapacity", M4CoreFlags)
bool FGLInventoryRules::RunTest(const FString& Parameters)
{
	// P11: slots and per-item stack limits are the inventory limit; there is no carried weight (operator, 2026-10-03).
	const FName Wire(TEXT("item.material.copper_wire"));
	const FName Stone(TEXT("item.material.cut_stone"));
	const int32 WireStack = Content().Find<FGLItemDef>(Wire)->StackSize;
	FGLInventory Inventory(3);
	TestEqual(TEXT("2.4 stacks of wire fit in 3 slots"), Inventory.Add(Content(), Wire, WireStack * 2 + WireStack / 2), WireStack * 2 + WireStack / 2);
	TestEqual(TEXT("as three stacks"), Inventory.GetStacks().Num(), 3);
	TestEqual(TEXT("space left for wire"), Inventory.SpaceFor(Content(), Wire), WireStack - WireStack / 2);
	TestEqual(TEXT("no slot for a new item"), Inventory.Add(Content(), Stone, 1), 0);
	TestEqual(TEXT("overflow is refused, not lost silently"), Inventory.Add(Content(), Wire, WireStack), WireStack - WireStack / 2);
	TestEqual(TEXT("count"), Inventory.CountOf(Wire), WireStack * 3);
	for (const FGLInventoryStack& Stack : Inventory.GetStacks())
	{
		TestTrue(TEXT("no stack exceeds the stack limit"), Stack.Count <= WireStack);
	}
	TestEqual(TEXT("unknown items are refused"), Inventory.Add(Content(), TEXT("item.material.unobtainium"), 1), 0);

	TestFalse(TEXT("removing more than carried fails"), Inventory.Remove(Wire, WireStack * 3 + 1));
	TestEqual(TEXT("and changes nothing"), Inventory.CountOf(Wire), WireStack * 3);
	TestTrue(TEXT("remove across stacks"), Inventory.Remove(Wire, WireStack * 2));
	TestEqual(TEXT("empty stacks are dropped"), Inventory.GetStacks().Num(), 1);

	// A save is never truncated: restoring more than fits keeps everything (adds are refused until space is freed).
	FGLInventory Restored(2);
	TestEqual(TEXT("force-add restores all of it"), Restored.ForceAdd(Content(), Wire, WireStack * 3 + 7), WireStack * 3 + 7);
	TestEqual(TEXT("nothing is discarded"), Restored.CountOf(Wire), WireStack * 3 + 7);
	TestTrue(TEXT("it is over capacity"), Restored.IsOverCapacity());
	TestEqual(TEXT("over capacity, a new item is refused"), Restored.Add(Content(), Stone, 1), 0);
	TestEqual(TEXT("and there is no space for more wire"), Restored.SpaceFor(Content(), Wire), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLYieldRules, "Gridlands.Core.Economy.YieldScalingFollowsCategory", M4CoreFlags)
bool FGLYieldRules::RunTest(const FString& Parameters)
{
	// E-1/E-2 over every real category and preset, plus extreme synthetic multipliers.
	TArray<FGLSettingsPresetDef> Presets;
	TArray<const FGLYieldCategoryDef*> Categories;
	Content().ForEachEntry([&](const FGLContentEntry& Entry)
	{
		if (const FGLSettingsPresetDef* Preset = Entry.Definition.GetPtr<FGLSettingsPresetDef>())
		{
			Presets.Add(*Preset);
		}
		if (const FGLYieldCategoryDef* Category = Entry.Definition.GetPtr<FGLYieldCategoryDef>())
		{
			Categories.Add(Category);
		}
	});
	for (const double Extreme : { 0.001, 0.5, 3.0, 50.0 })
	{
		FGLSettingsPresetDef Synthetic;
		Synthetic.Id = *FString::Printf(TEXT("settings.test.x%g"), Extreme);
		Synthetic.YieldMultipliers.ResourceYield = Extreme;
		Synthetic.YieldMultipliers.CreatureDrops = Extreme;
		Presets.Add(Synthetic);
	}
	TestTrue(TEXT("all ten yield categories present"), Categories.Num() == 10);
	int32 Checked = 0;
	for (const FGLYieldCategoryDef* Category : Categories)
	{
		for (const FGLSettingsPresetDef& Preset : Presets)
		{
			for (const int32 Authored : { 1, 3, 5, 40 })
			{
				const int32 Result = GLYield::Apply(Authored, *Category, Preset);
				if (!Category->Scalable)
				{
					TestEqual(FString::Printf(TEXT("%s never scales (%s)"), *Category->Id.ToString(), *Preset.Id.ToString()), Result, Authored);
				}
				else
				{
					const int32 Expected = FMath::Max(1, FMath::RoundToInt(Authored * GLYield::MultiplierFor(*Category, Preset)));
					TestEqual(FString::Printf(TEXT("%s scales (%s)"), *Category->Id.ToString(), *Preset.Id.ToString()), Result, Expected);
					TestTrue(TEXT("a reached repeatable source never yields nothing"), Result >= 1);
				}
				++Checked;
			}
		}
	}
	const FGLYieldCategoryDef* Rare = Content().Find<FGLYieldCategoryDef>(TEXT("yield.rare.material"));
	TestTrue(TEXT("E1: repeatable rare materials scale"), Rare && Rare->Scalable);
	const FGLYieldCategoryDef* Glitch = Content().Find<FGLYieldCategoryDef>(TEXT("yield.reward.glitch"));
	TestTrue(TEXT("E1: glitch rewards never scale"), Glitch && !Glitch->Scalable);
	TestTrue(TEXT("combinations checked"), Checked >= 200);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSalvageRulesTest, "Gridlands.Core.Salvage.ToolEfficiencyAndGating", M4CoreFlags)
bool FGLSalvageRulesTest::RunTest(const FString& Parameters)
{
	const FGLItemDef* PryBarDef = Content().Find<FGLItemDef>(TEXT("item.tool.pry_bar"));
	const FGLItemDef* Wire = Content().Find<FGLItemDef>(TEXT("item.material.copper_wire"));
	struct FCase { const TCHAR* Salvage; int32 BareHits; int32 PryHits; };
	for (const FCase& Case : { FCase{ TEXT("salvage.yard.junk_pile"), 6, 3 }, FCase{ TEXT("salvage.yard.fence_panel"), 4, 2 }, FCase{ TEXT("salvage.house.wiring_run"), 3, 2 } })
	{
		const FGLSalvageDef* Salvage = Content().Find<FGLSalvageDef>(Case.Salvage);
		if (!TestNotNull(Case.Salvage, Salvage))
		{
			continue;
		}
		const FGLMaterialDef* Material = Content().Find<FGLMaterialDef>(Salvage->Material);
		TestEqual(FString::Printf(TEXT("%s bare-handed hits"), Case.Salvage), GLSalvageRules::HitsToSalvage(*Salvage, Material, nullptr), Case.BareHits);
		TestEqual(FString::Printf(TEXT("%s pry-bar hits"), Case.Salvage), GLSalvageRules::HitsToSalvage(*Salvage, Material, PryBarDef), Case.PryHits);
		TestTrue(FString::Printf(TEXT("%s: the pry bar is strictly better"), Case.Salvage), Case.PryHits < Case.BareHits);
		TestEqual(TEXT("a non-tool item gives no bonus"), GLSalvageRules::ToolMultiplier(*Salvage, Wire), 1.0);
	}

	FGLSalvageDef Gated;
	Gated.Integrity = 10.0;
	Gated.RequiresTool = TEXT("Tool.Pry");
	FText Reason;
	TestFalse(TEXT("gated salvage refuses bare hands"), GLSalvageRules::CanSalvage(Gated, nullptr, &Reason));
	TestFalse(TEXT("and says why"), Reason.IsEmpty());
	TestTrue(TEXT("and accepts the right tool"), GLSalvageRules::CanSalvage(Gated, PryBarDef));

	FGLItemDef LowTier = *PryBarDef;
	LowTier.Tool.Tier = 0;
	FGLSalvageDef Junk = *Content().Find<FGLSalvageDef>(TEXT("salvage.yard.junk_pile"));
	TestEqual(TEXT("a tool below minTier earns no bonus"), GLSalvageRules::ToolMultiplier(Junk, &LowTier), 1.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
