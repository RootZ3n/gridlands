#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Fabrication/GLFabricationRules.h"
#include "GLFabricatorComponent.generated.h"

/**
 * Lets its owner fabricate recipes from its inventory, using the world's knowledge and any
 * stations in reach. Emits Event.Item.Fabricated (subject: the output item) on success.
 */
UCLASS(ClassGroup = (Gridlands), meta = (BlueprintSpawnableComponent))
class GRIDLANDSGAME_API UGLFabricatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	FGLCraftCheck Check(FName RecipeId) const;
	FGLCraftCheck Fabricate(FName RecipeId);
	/** Recipes this owner could make right now, sorted by id. */
	TArray<FName> CraftableRecipes() const;
	/** What F makes: the first craftable recipe whose output Zenny does not already carry, else the first craftable. */
	FName PreferredRecipe() const;
	/** Station.* tags within reach of the owner. */
	TArray<FName> StationsInReach() const;
};
