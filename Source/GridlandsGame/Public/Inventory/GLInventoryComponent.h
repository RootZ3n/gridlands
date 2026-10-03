#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Fabrication/GLFabricationRules.h"
#include "Inventory/GLInventory.h"
#include "GLInventoryComponent.generated.h"

struct FGLItemDef;
struct FGLSalvageDef;

/**
 * Carries items for its owner using the pure FGLInventory rules: slots and stack limits (P11: there is no carried
 * weight; operator, 2026-10-03). Emits Event.Item.Acquired, and Event.Player.InventoryFull when something it was given
 * would not fit (a pickup, salvage, refund or craft refused for lack of room).
 */
UCLASS(ClassGroup = (Gridlands), meta = (BlueprintSpawnableComponent))
class GRIDLANDSGAME_API UGLInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLInventoryComponent();
	virtual void BeginPlay() override;

	/** Adds items; returns how many fit (announces InventoryFull when not all did). */
	int32 AddItem(FName Item, int32 Count);
	bool RemoveItem(FName Item, int32 Count);
	/** Crafts RecipeId transactionally (GLFabricationRules); announces the output like any acquired item. */
	FGLCraftCheck Craft(FName RecipeId, const class FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach);
	int32 CountOf(FName Item) const { return Inventory.CountOf(Item); }
	/** Replaces the contents from a save, without Item.Acquired events. Nothing is ever discarded (FGLInventory::ForceAdd). */
	void RestoreContents(const TArray<TPair<FName, int32>>& Items);
	const FGLInventory& GetInventory() const { return Inventory; }
	/** P11: material pools fill and drain it directly (shared base storage); they announce what they deliver. */
	FGLInventory& GetMutableInventory() { return Inventory; }
	/** Event.Player.InventoryFull: Subject is what did not fit. */
	void AnnounceFull(FName Subject);

	/** The carried tool allowed on Salvage with the highest multiplier, or null if none is carried. */
	const FGLItemDef* BestToolFor(const FGLSalvageDef& Salvage) const;

private:
	FGLInventory Inventory;
};
