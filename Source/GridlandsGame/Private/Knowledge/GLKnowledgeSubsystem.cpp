#include "Knowledge/GLKnowledgeSubsystem.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"

void UGLKnowledgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGLEventSubsystem* Bus = Collection.InitializeDependency<UGLEventSubsystem>())
	{
		UGameplayTagsManager& Tags = UGameplayTagsManager::Get();
		Bus->Subscribe(Tags.RequestGameplayTag(TEXT("Event.Item.Acquired")), FGLGameplayEventDelegate::CreateUObject(this, &UGLKnowledgeSubsystem::HandleItemAcquired));
		Bus->Subscribe(Tags.RequestGameplayTag(TEXT("Event.Salvage.Completed")), FGLGameplayEventDelegate::CreateUObject(this, &UGLKnowledgeSubsystem::HandleSalvageCompleted));
	}
}

bool UGLKnowledgeSubsystem::Learn(FName Id)
{
	if (!GLContent::Get().Find<FGLKnowledgeDef>(Id) || !Knowledge.Learn(Id))
	{
		return false;
	}
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Knowledge.Unlocked"));
	Event.Subject = Id;
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
	return true;
}

void UGLKnowledgeSubsystem::HandleItemAcquired(const FGLGameplayEvent& Event)
{
	if (const FGLItemDef* Item = GLContent::Get().Find<FGLItemDef>(Event.Subject))
	{
		for (const FName& Id : Item->OnAcquireUnlocks)
		{
			Learn(Id);
		}
	}
}

void UGLKnowledgeSubsystem::HandleSalvageCompleted(const FGLGameplayEvent& Event)
{
	if (const FGLSalvageDef* Salvage = GLContent::Get().Find<FGLSalvageDef>(Event.Subject))
	{
		for (const FName& Id : Salvage->OnSalvageUnlocks)
		{
			Learn(Id);
		}
	}
}
