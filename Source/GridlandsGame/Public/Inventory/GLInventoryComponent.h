#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Fabrication/GLFabricationRules.h"
#include "Inventory/GLInventory.h"
#include "GLInventoryComponent.generated.h"

struct FGLItemDef;
struct FGLSalvageDef;

/**
 * Carries items for its owner using the pure FGLInventory rules. Emits Event.Item.Acquired, and
 * Event.Player.Overencumbered when carried weight first crosses the limit; while overencumbered,
 * a character owner walks at OverencumberedSpeedFactor of its normal speed.
 */
UCLASS(ClassGroup = (Gridlands), meta = (BlueprintSpawnableComponent))
class GRIDLANDSGAME_API UGLInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Adds items; returns how many fit. */
	int32 AddItem(FName Item, int32 Count);
	bool RemoveItem(FName Item, int32 Count);
	/** Crafts RecipeId transactionally (GLFabricationRules); announces the output like any acquired item. */
	FGLCraftCheck Craft(FName RecipeId, const class FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach);
	int32 CountOf(FName Item) const { return Inventory.CountOf(Item); }
	bool IsOverencumbered() const { return bOverencumbered; }
	/** Replaces the contents from a save, without Item.Acquired or Overencumbered events. */
	void RestoreContents(const TArray<TPair<FName, int32>>& Items);
	const FGLInventory& GetInventory() const { return Inventory; }

	/** The carried tool allowed on Salvage with the highest multiplier, or null if none is carried. */
	const FGLItemDef* BestToolFor(const FGLSalvageDef& Salvage) const;

	UPROPERTY(EditAnywhere, Category = "Inventory") float OverencumberedSpeedFactor = 0.5f;

private:
	void UpdateEncumbrance(bool bAnnounce = true);

	FGLInventory Inventory;
	bool bOverencumbered = false;
	float NormalWalkSpeed = -1.f;
};
