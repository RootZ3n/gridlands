#include "Fabrication/GLFabricationRules.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Inventory/GLInventory.h"
#include "Inventory/GLMaterialPool.h"
#include "Knowledge/GLKnowledge.h"

namespace GLFabricationRules
{
	FGLCraftCheck Check(const FGLContentRegistry& Content, FName RecipeId, const FGLMaterialPool& Pool,
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
			if (Pool.CountOf(Input.Item) < Input.Count)
			{
				Result.Missing.Add(Input.Item);
			}
		}
		if (Result.Missing.Num() > 0)
		{
			Result.Block = EGLCraftBlock::MissingInputs;
			return Result;
		}
		// Room for the output once the inputs are gone: proven on copies.
		if (!Pool.CanExchange(Content, Recipe->Inputs, { { Recipe->Output.Item, Recipe->Output.Count } }))
		{
			Result.Block = EGLCraftBlock::NoRoomForOutput;
		}
		return Result;
	}

	FGLCraftCheck Craft(const FGLContentRegistry& Content, FName RecipeId, FGLMaterialPool& Pool,
		const FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach)
	{
		const FGLCraftCheck Result = Check(Content, RecipeId, Pool, Knowledge, StationsInReach);
		if (Result.CanCraft())
		{
			const FGLRecipeDef* Recipe = Content.Find<FGLRecipeDef>(RecipeId);
			verify(Pool.Exchange(Content, Recipe->Inputs, { { Recipe->Output.Item, Recipe->Output.Count } })); // proven by Check
		}
		return Result;
	}

	FGLCraftCheck Check(const FGLContentRegistry& Content, FName RecipeId, const FGLInventory& Inventory,
		const FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach)
	{
		const FGLMaterialPool Pool({ const_cast<FGLInventory*>(&Inventory) }); // Check never mutates the pool
		return Check(Content, RecipeId, Pool, Knowledge, StationsInReach);
	}

	FGLCraftCheck Craft(const FGLContentRegistry& Content, FName RecipeId, FGLInventory& Inventory,
		const FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach)
	{
		FGLMaterialPool Pool({ &Inventory });
		return Craft(Content, RecipeId, Pool, Knowledge, StationsInReach);
	}
}
