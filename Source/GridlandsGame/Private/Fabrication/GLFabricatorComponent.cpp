#include "Fabrication/GLFabricatorComponent.h"

#include "Building/GLBuildingSubsystem.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "Fabrication/GLStationComponent.h"
#include "GameplayTagsManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"

namespace
{
	const FGLKnowledge& WorldKnowledge(const UActorComponent* Component)
	{
		static const FGLKnowledge Empty;
		const UGLKnowledgeSubsystem* Subsystem = Component->GetWorld() ? Component->GetWorld()->GetSubsystem<UGLKnowledgeSubsystem>() : nullptr;
		return Subsystem ? Subsystem->GetKnowledge() : Empty;
	}
}

TArray<FName> UGLFabricatorComponent::StationsInReach() const
{
	TArray<FName> Stations;
	const AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return Stations;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const UGLStationComponent* Station = It->FindComponentByClass<UGLStationComponent>();
		if (Station && FVector::Dist(It->GetActorLocation(), Owner->GetActorLocation()) <= Station->Reach)
		{
			Stations.AddUnique(Station->StationTag);
		}
	}
	// P11: stations the player built (a sawhorse) are facts of the structural model, not actors with components.
	if (const UGLBuildingSubsystem* Building = GetWorld()->GetSubsystem<UGLBuildingSubsystem>())
	{
		for (const FName& Tag : Building->StationsNear(Owner->GetActorLocation()))
		{
			Stations.AddUnique(Tag);
		}
	}
	return Stations;
}

FGLCraftCheck UGLFabricatorComponent::Check(FName RecipeId) const
{
	const UGLInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return { EGLCraftBlock::MissingInputs, {} };
	}
	// P11: inside a base, shared storage supplies the inputs (storage first) and takes the output if Zenny is full.
	const UGLBuildingSubsystem* Building = GetWorld() ? GetWorld()->GetSubsystem<UGLBuildingSubsystem>() : nullptr;
	if (Building)
	{
		const FGLMaterialSources Sources = Building->SourcesFor(GetOwner(), GetOwner()->GetActorLocation());
		return GLFabricationRules::Check(GLContent::Get(), RecipeId, Sources.Pool(), WorldKnowledge(this), StationsInReach());
	}
	return GLFabricationRules::Check(GLContent::Get(), RecipeId, Inventory->GetInventory(), WorldKnowledge(this), StationsInReach());
}

FGLCraftCheck UGLFabricatorComponent::Fabricate(FName RecipeId)
{
	UGLInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return { EGLCraftBlock::MissingInputs, {} };
	}
	const UGLBuildingSubsystem* Building = GetWorld() ? GetWorld()->GetSubsystem<UGLBuildingSubsystem>() : nullptr;
	FGLCraftCheck Result;
	if (Building)
	{
		FGLMaterialSources Sources = Building->SourcesFor(GetOwner(), GetOwner()->GetActorLocation());
		FGLMaterialPool Pool = Sources.Pool();
		Result = GLFabricationRules::Craft(GLContent::Get(), RecipeId, Pool, WorldKnowledge(this), StationsInReach());
		if (Result.CanCraft())
		{
			const FGLRecipeDef* Recipe = GLContent::Get().Find<FGLRecipeDef>(RecipeId);
			Sources.AnnounceAcquired(GetOwner(), { { Recipe->Output.Item, Recipe->Output.Count } });
		}
		else if (Result.Block == EGLCraftBlock::NoRoomForOutput)
		{
			Inventory->AnnounceFull(RecipeId);
		}
	}
	else
	{
		Result = Inventory->Craft(RecipeId, WorldKnowledge(this), StationsInReach());
	}
	if (Result.CanCraft())
	{
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Item.Fabricated"));
		Event.Subject = GLContent::Get().Find<FGLRecipeDef>(RecipeId)->Output.Item;
		Event.Instigator = GetOwner();
		UGLEventSubsystem::Emit(this, MoveTemp(Event));
	}
	return Result;
}

FName UGLFabricatorComponent::PreferredRecipe() const
{
	const TArray<FName> Recipes = CraftableRecipes();
	const UGLInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	for (const FName& Id : Recipes)
	{
		const FGLRecipeDef* Recipe = GLContent::Get().Find<FGLRecipeDef>(Id);
		if (Recipe && Inventory && Inventory->CountOf(Recipe->Output.Item) == 0)
		{
			return Id;
		}
	}
	return Recipes.Num() > 0 ? Recipes[0] : NAME_None;
}

TArray<FName> UGLFabricatorComponent::CraftableRecipes() const
{
	TArray<FName> Recipes;
	GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
	{
		if (Entry.Kind == TEXT("recipe") && Check(Entry.Id).CanCraft())
		{
			Recipes.Add(Entry.Id);
		}
	});
	Recipes.Sort(FNameLexicalLess());
	return Recipes;
}
