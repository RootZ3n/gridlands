#include "Fabrication/GLFabricationRules.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Inventory/GLInventory.h"
#include "Knowledge/GLKnowledge.h"

namespace GLFabricationRules
{
	FGLCraftCheck Check(const FGLContentRegistry& Content, FName RecipeId, const FGLInventory& Inventory,
		const FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach)
	{
		FGLCraftCheck Result;
		const FGLRecipeDef* Recipe = Content.Find<FGLRecipeDef>(RecipeId);
		if (!Recipe)
		{
			Result.Block = EGLCraftBlock::UnknownRecipe;
			return Result;
		}
		if (!Knowledge.KnowsAll(Recipe->UnlockedBy, &Result.Missing))
		{
			Result.Block = EGLCraftBlock::NotKnown;
			return Result;
		}
		if (!Recipe->Station.IsNone() && !StationsInReach.Contains(Recipe->Station))
		{
			Result.Block = EGLCraftBlock::NeedsStation;
			Result.Missing.Add(Recipe->Station);
			return Result;
		}
		for (const FGLItemStackDef& Input : Recipe->Inputs)
		{
			if (Inventory.CountOf(Input.Item) < Input.Count)
			{
				Result.Missing.Add(Input.Item);
			}
		}
		if (Result.Missing.Num() > 0)
		{
			Result.Block = EGLCraftBlock::MissingInputs;
			return Result;
		}
		// Room for the output once the inputs are gone: simulate on a copy.
		FGLInventory After = Inventory;
		for (const FGLItemStackDef& Input : Recipe->Inputs)
		{
			After.Remove(Input.Item, Input.Count);
		}
		if (After.SpaceFor(Content, Recipe->Output.Item) < Recipe->Output.Count)
		{
			Result.Block = EGLCraftBlock::NoRoomForOutput;
		}
		return Result;
	}

	FGLCraftCheck Craft(const FGLContentRegistry& Content, FName RecipeId, FGLInventory& Inventory,
		const FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach)
	{
		const FGLCraftCheck Result = Check(Content, RecipeId, Inventory, Knowledge, StationsInReach);
		if (Result.CanCraft())
		{
			const FGLRecipeDef* Recipe = Content.Find<FGLRecipeDef>(RecipeId);
			for (const FGLItemStackDef& Input : Recipe->Inputs)
			{
				Inventory.Remove(Input.Item, Input.Count);
			}
			const int32 Added = Inventory.Add(Content, Recipe->Output.Item, Recipe->Output.Count);
			check(Added == Recipe->Output.Count); // guaranteed by Check
		}
		return Result;
	}
}
