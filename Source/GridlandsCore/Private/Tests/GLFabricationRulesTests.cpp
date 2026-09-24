#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Fabrication/GLFabricationRules.h"
#include "Inventory/GLInventory.h"
#include "Knowledge/GLKnowledge.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLFabricationRulesTests
{
	constexpr EAutomationTestFlags FabricationFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	const FGLContentRegistry& FabricationContent()
	{
		static FGLContentRegistry Registry;
		static bool bLoaded = Registry.LoadRepository(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		return Registry;
	}

	const FName PryRecipe(TEXT("recipe.tool.pry_bar"));
	const FName Scrap(TEXT("item.material.scrap_metal"));
	const FName PryBar(TEXT("item.tool.pry_bar"));
}

using namespace GLFabricationRulesTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCraftTransactional, "Gridlands.Core.Fabrication.CraftIsTransactional", FabricationFlags)
bool FGLCraftTransactional::RunTest(const FString& Parameters)
{
	const FGLContentRegistry& Content = FabricationContent();
	const FGLKnowledge Knowledge;
	const TArray<FName> NoStations;

	FGLInventory Short(8);
	Short.Add(Content, Scrap, 2);
	const FGLCraftCheck Missing = GLFabricationRules::Craft(Content, PryRecipe, Short, Knowledge, NoStations);
	TestTrue(TEXT("two scrap is not enough"), Missing.Block == EGLCraftBlock::MissingInputs && Missing.Missing.Contains(Scrap));
	TestEqual(TEXT("a refused craft consumes nothing"), Short.CountOf(Scrap), 2);

	FGLInventory Enough(8);
	Enough.Add(Content, Scrap, 4);
	TestTrue(TEXT("three scrap crafts a pry bar"), GLFabricationRules::Craft(Content, PryRecipe, Enough, Knowledge, NoStations).CanCraft());
	TestEqual(TEXT("inputs consumed exactly"), Enough.CountOf(Scrap), 1);
	TestEqual(TEXT("output added"), Enough.CountOf(PryBar), 1);

	// Output needs a slot: a full inventory of other items (inputs still leave their stack) refuses.
	FGLInventory Full(1);
	Full.Add(Content, Scrap, 50); // one full slot of 50 scrap: using 3 still leaves the slot occupied
	const FGLCraftCheck NoRoom = GLFabricationRules::Craft(Content, PryRecipe, Full, Knowledge, NoStations);
	TestTrue(TEXT("no room for the output"), NoRoom.Block == EGLCraftBlock::NoRoomForOutput);
	TestEqual(TEXT("and nothing was consumed"), Full.CountOf(Scrap), 50);

	FGLInventory Exact(1);
	Exact.Add(Content, Scrap, 3); // inputs free the only slot
	TestTrue(TEXT("consuming the inputs can make the room"), GLFabricationRules::Craft(Content, PryRecipe, Exact, Knowledge, NoStations).CanCraft());
	TestTrue(TEXT("unknown recipe"), GLFabricationRules::Check(Content, TEXT("recipe.tool.nope"), Exact, Knowledge, NoStations).Block == EGLCraftBlock::UnknownRecipe);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCraftGates, "Gridlands.Core.Fabrication.KnowledgeAndStationGates", FabricationFlags)
bool FGLCraftGates::RunTest(const FString& Parameters)
{
	// Synthetic registry entries would need files; instead gate the real recipe through a copy.
	FGLRecipeDef Gated = *FabricationContent().Find<FGLRecipeDef>(PryRecipe);
	Gated.UnlockedBy = { TEXT("knowledge.style.roman_masonry"), TEXT("knowledge.material.copper_wire") };
	Gated.Station = TEXT("Station.Workbench");

	FGLKnowledge Knowledge;
	TArray<FName> Missing;
	TestFalse(TEXT("AND semantics: nothing known"), Knowledge.KnowsAll(Gated.UnlockedBy, &Missing));
	TestEqual(TEXT("both missing"), Missing.Num(), 2);
	TestTrue(TEXT("learning is idempotent"), Knowledge.Learn(TEXT("knowledge.style.roman_masonry")) && !Knowledge.Learn(TEXT("knowledge.style.roman_masonry")));
	Missing.Reset();
	TestFalse(TEXT("one of two is not enough"), Knowledge.KnowsAll(Gated.UnlockedBy, &Missing));
	TestTrue(TEXT("reports the one missing"), Missing.Num() == 1 && Missing[0] == FName(TEXT("knowledge.material.copper_wire")));
	Knowledge.Learn(TEXT("knowledge.material.copper_wire"));
	TestTrue(TEXT("all known"), Knowledge.KnowsAll(Gated.UnlockedBy));
	TestTrue(TEXT("an empty unlock list is always available"), Knowledge.KnowsAll({}));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
