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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLInventoryRules, "Gridlands.Core.Inventory.StacksWeightAndCapacity", M4CoreFlags)
bool FGLInventoryRules::RunTest(const FString& Parameters)
{
	const FName Wire(TEXT("item.material.copper_wire")); // stack 50, weight 0.1
	const FName Stone(TEXT("item.material.cut_stone"));  // stack 30, weight 3.0
	FGLInventory Inventory(3, 100.0);
	TestEqual(TEXT("120 wire fits in 3 slots of 50"), Inventory.Add(Content(), Wire, 120), 120);
	TestEqual(TEXT("as three stacks"), Inventory.GetStacks().Num(), 3);
	TestEqual(TEXT("space left for wire"), Inventory.SpaceFor(Content(), Wire), 30);
	TestEqual(TEXT("no slot for a new item"), Inventory.Add(Content(), Stone, 1), 0);
	TestEqual(TEXT("overflow is refused, not lost silently"), Inventory.Add(Content(), Wire, 40), 30);
	TestEqual(TEXT("count"), Inventory.CountOf(Wire), 150);
	TestEqual(TEXT("unknown items are refused"), Inventory.Add(Content(), TEXT("item.material.unobtainium"), 1), 0);

	TestFalse(TEXT("removing more than carried fails"), Inventory.Remove(Wire, 151));
	TestEqual(TEXT("and changes nothing"), Inventory.CountOf(Wire), 150);
	TestTrue(TEXT("remove across stacks"), Inventory.Remove(Wire, 120));
	TestEqual(TEXT("empty stacks are dropped"), Inventory.GetStacks().Num(), 1);

	FGLInventory Heavy(10, 100.0);
	Heavy.Add(Content(), Stone, 30); // 90 kg
	TestFalse(TEXT("90 of 100 is not overencumbered"), Heavy.IsOverencumbered(Content()));
	Heavy.Add(Content(), Stone, 4); // 102 kg
	TestTrue(TEXT("102 of 100 is overencumbered (allowed, not refused)"), Heavy.IsOverencumbered(Content()));
	TestEqual(TEXT("weight is exact"), Heavy.TotalWeight(Content()), 102.0);
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
