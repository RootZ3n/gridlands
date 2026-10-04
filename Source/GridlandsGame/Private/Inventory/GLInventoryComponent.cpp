#include "Inventory/GLInventoryComponent.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Knowledge/GLKnowledge.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Salvage/GLSalvageRules.h"

UGLInventoryComponent::UGLInventoryComponent()
	: Inventory(32) // P11 provisional; BeginPlay applies the tuned count (tuning.world.physical building.personalSlots)
{
}

void UGLInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	const int32 Slots = GLContent::Tuning().Building.PersonalSlots;
	if (Slots > 0 && Slots != Inventory.GetMaxSlots())
	{
		const FGLInventory Before = Inventory;
		Inventory = FGLInventory(Slots);
		for (const FGLInventoryStack& Stack : Before.GetStacks())
		{
			Inventory.ForceAdd(GLContent::Get(), Stack.Item, Stack.Count);
		}
	}
}

void UGLInventoryComponent::AnnounceFull(FName Subject)
{
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Player.InventoryFull"));
	Event.Subject = Subject;
	Event.Instigator = GetOwner();
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
}

int32 UGLInventoryComponent::AddItem(FName Item, int32 Count)
{
	const int32 Added = Inventory.Add(GLContent::Get(), Item, Count);
	if (Added > 0)
	{
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Item.Acquired"));
		Event.Subject = Item;
		Event.Instigator = GetOwner();
		Event.Numbers.Add(TEXT("count"), Added);
		UGLEventSubsystem::Emit(this, MoveTemp(Event));
	}
	if (Added < Count)
	{
		AnnounceFull(Item);
	}
	return Added;
}

FGLCraftCheck UGLInventoryComponent::Craft(FName RecipeId, const FGLKnowledge& Knowledge, const TArray<FName>& StationsInReach)
{
	const FGLCraftCheck Result = GLFabricationRules::Craft(GLContent::Get(), RecipeId, Inventory, Knowledge, StationsInReach);
	if (Result.CanCraft())
	{
		const FGLRecipeDef* Recipe = GLContent::Get().Find<FGLRecipeDef>(RecipeId);
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Item.Acquired"));
		Event.Subject = Recipe->Output.Item;
		Event.Instigator = GetOwner();
		Event.Numbers.Add(TEXT("count"), Recipe->Output.Count);
		UGLEventSubsystem::Emit(this, MoveTemp(Event));
	}
	else if (Result.Block == EGLCraftBlock::NoRoomForOutput)
	{
		AnnounceFull(RecipeId);
	}
	return Result;
}

void UGLInventoryComponent::RestoreContents(const TArray<TPair<FName, int32>>& Items)
{
	Inventory = FGLInventory(Inventory.GetMaxSlots());
	for (const TPair<FName, int32>& Item : Items)
	{
		Inventory.ForceAdd(GLContent::Get(), Item.Key, Item.Value); // a save is never truncated (P11)
	}
}

bool UGLInventoryComponent::RemoveItem(FName Item, int32 Count)
{
	return Inventory.Remove(Item, Count);
}

const FGLItemDef* UGLInventoryComponent::BestToolFor(const FGLSalvageDef& Salvage) const
{
	const FGLItemDef* Best = nullptr;
	double BestMultiplier = 0.0;
	for (const FGLInventoryStack& Stack : Inventory.GetStacks())
	{
		const FGLItemDef* Item = GLContent::Get().Find<FGLItemDef>(Stack.Item);
		if (Item && Item->IsTool() && GLSalvageRules::CanSalvage(Salvage, Item))
		{
			const double Multiplier = GLSalvageRules::ToolMultiplier(Salvage, Item);
			if (!Best || Multiplier > BestMultiplier)
			{
				Best = Item;
				BestMultiplier = Multiplier;
			}
		}
	}
	return Best;
}
