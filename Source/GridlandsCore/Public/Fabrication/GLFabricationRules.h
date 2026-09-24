#pragma once

#include "CoreMinimal.h"

class FGLContentRegistry;
class FGLInventory;
class FGLKnowledge;
struct FGLRecipeDef;

/** Why a recipe cannot be made right now. */
enum class EGLCraftBlock : uint8
{
	None,
	UnknownRecipe,
	NotKnown,        // unlockedBy knowledge missing
	NeedsStation,    // recipe needs a station that is not in reach
	MissingInputs,
	NoRoomForOutput,
};

struct GRIDLANDSCORE_API FGLCraftCheck
{
	EGLCraftBlock Block = EGLCraftBlock::None;
	/** For NotKnown: the missing knowledge; for MissingInputs: the short items. */
	TArray<FName> Missing;

	bool CanCraft() const { return Block == EGLCraftBlock::None; }
};

/** Fabrication rules: pure, transactional. */
namespace GLFabricationRules
{
	/** StationsInReach: Station.* tags the crafter can use right now (empty for hand crafting). */
	GRIDLANDSCORE_API FGLCraftCheck Check(const FGLContentRegistry& Content, FName RecipeId, const FGLInventory& Inventory,
		const FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach);

	/** Crafts once if Check passes: consumes inputs and adds the output together, or changes nothing. */
	GRIDLANDSCORE_API FGLCraftCheck Craft(const FGLContentRegistry& Content, FName RecipeId, FGLInventory& Inventory,
		const FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach);
}
