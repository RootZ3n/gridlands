#include "Save/GLSaveSubsystem.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Dialogue/GLDialogueDirector.h"
#include "Economy/GLWorldSettingsSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "GridlandsGame.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Pehlichi/GLCapabilityComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "World/GLPlacementSubsystem.h"

namespace
{
	FGLSavedTransform ToSaved(const AActor* Actor)
	{
		FGLSavedTransform Saved;
		if (Actor)
		{
			Saved.Location = Actor->GetActorLocation();
			Saved.Yaw = Actor->GetActorRotation().Yaw;
		}
		return Saved;
	}

	void ApplyTransform(AActor* Actor, const FGLSavedTransform& Saved)
	{
		if (Actor && !Saved.Location.IsZero())
		{
			Actor->SetActorLocationAndRotation(Saved.Location, FRotator(0.0, Saved.Yaw, 0.0));
		}
	}
}

void UGLSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGLEventSubsystem* Bus = Collection.InitializeDependency<UGLEventSubsystem>())
	{
		Bus->Subscribe(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Glitch.Repaired")),
			FGLGameplayEventDelegate::CreateUObject(this, &UGLSaveSubsystem::HandleGlitchRepaired));
	}
}

AGLPehlichi* UGLSaveSubsystem::FindPehlichi() const
{
	for (TActorIterator<AGLPehlichi> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

FGLWorldSave UGLSaveSubsystem::Capture() const
{
	FGLWorldSave Save;
	UWorld* World = GetWorld();
	const UGLGlitchSubsystem* Glitches = World->GetSubsystem<UGLGlitchSubsystem>();
	for (const TWeakObjectPtr<AGLGlitch>& Actor : Glitches->GetAll())
	{
		if (const UGLGlitchComponent* Glitch = Actor.IsValid() ? Actor->GetGlitch() : nullptr)
		{
			Save.Glitches.Add({ Glitch->GetPlacementId(), FGLGlitchLifecycle::ToPersistedState(Glitch->GetState()), Glitch->GetProgressSeconds(), Glitch->AreItemsDelivered() });
			if (Save.Cell.IsNone())
			{
				// placement.<cell short>.<name> -> the cell whose id ends in that short name
				const FString Short = Glitch->GetPlacementId().ToString().Mid(10).Left(Glitch->GetPlacementId().ToString().Mid(10).Find(TEXT(".")));
				GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
				{
					if (Entry.Kind == TEXT("cell") && Entry.Id.ToString().EndsWith(TEXT(".") + Short))
					{
						Save.Cell = Entry.Id;
					}
				});
			}
		}
	}
	for (TActorIterator<AGLSalvageNode> It(World); It; ++It)
	{
		if (It->GetSalvageable()->IsSalvaged() && !It->PlacementId.IsNone())
		{
			Save.SalvagedPlacements.Add(It->PlacementId);
		}
	}
	Save.SalvagedPlacements.Sort(FNameLexicalLess());

	const AActor* Zenny = Glitches->GetCommander();
	Save.Zenny = ToSaved(Zenny);
	if (const UGLInventoryComponent* Inventory = Zenny ? Zenny->FindComponentByClass<UGLInventoryComponent>() : nullptr)
	{
		TMap<FName, int32> Totals;
		for (const FGLInventoryStack& Stack : Inventory->GetInventory().GetStacks())
		{
			Totals.FindOrAdd(Stack.Item) += Stack.Count;
		}
		for (const TPair<FName, int32>& Total : Totals)
		{
			Save.Inventory.Add({ Total.Key, Total.Value });
		}
	}
	if (const UGLKnowledgeSubsystem* Knowledge = World->GetSubsystem<UGLKnowledgeSubsystem>())
	{
		Save.Knowledge = Knowledge->GetKnowledge().GetKnown().Array();
		Save.Knowledge.Sort(FNameLexicalLess());
	}
	if (const AGLPehlichi* Pehlichi = FindPehlichi())
	{
		Save.Pehlichi = ToSaved(Pehlichi);
		GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
		{
			if (Entry.Kind == TEXT("capability"))
			{
				Save.PehlichiCapabilities.Add({ Entry.Id, Pehlichi->GetCapabilities()->Level(Entry.Id) });
			}
		});
	}
	if (const UGLDialogueDirector* Director = World->GetSubsystem<UGLDialogueDirector>())
	{
		for (const TPair<FName, FGLExchangeHistory>& Use : Director->GetState().Exchanges)
		{
			Save.ExchangeUses.Add({ Use.Key, Use.Value.Uses });
		}
		for (const TPair<FName, int32>& Count : Director->GetState().EventCounts)
		{
			Save.EventCounts.Add({ Count.Key, Count.Value });
		}
	}
	if (const UGLWorldSettingsSubsystem* Settings = World->GetSubsystem<UGLWorldSettingsSubsystem>())
	{
		Save.SettingsPreset = Settings->GetPresetId();
	}
	auto ById = [](const FGLSavedCount& A, const FGLSavedCount& B) { return A.Id.LexicalLess(B.Id); };
	Save.Inventory.Sort(ById);
	Save.PehlichiCapabilities.Sort(ById);
	Save.ExchangeUses.Sort(ById);
	Save.EventCounts.Sort(ById);
	Save.Glitches.Sort([](const FGLSavedGlitch& A, const FGLSavedGlitch& B) { return A.Placement.LexicalLess(B.Placement); });
	return Save;
}

void UGLSaveSubsystem::Apply(const FGLWorldSave& Save, TArray<FString>* OutProblems)
{
	auto Problem = [OutProblems](const FString& Message)
	{
		UE_LOG(LogGridlands, Warning, TEXT("Load: %s"), *Message);
		if (OutProblems)
		{
			OutProblems->Add(Message);
		}
	};
	UWorld* World = GetWorld();
	UGLGlitchSubsystem* Glitches = World->GetSubsystem<UGLGlitchSubsystem>();
	UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>();

	for (const FGLSavedGlitch& Saved : Save.Glitches)
	{
		AGLGlitch* Glitch = Glitches->FindByPlacement(Saved.Placement);
		if (!Glitch)
		{
			Problem(FString::Printf(TEXT("saved glitch placement %s no longer exists"), *Saved.Placement.ToString()));
			continue;
		}
		if (!Glitch->GetGlitch()->RestoreFromSave(Saved.State, Saved.ProgressSeconds, Saved.ItemsDelivered, FGLRestoreAuthority()))
		{
			Problem(FString::Printf(TEXT("could not restore %s to %s"), *Saved.Placement.ToString(), FGLGlitchLifecycle::StateName(Saved.State)));
		}
	}
	for (const FName& Id : Save.SalvagedPlacements)
	{
		if (AGLSalvageNode* Node = Placements->FindSalvageNode(Id))
		{
			Node->GetSalvageable()->RestoreSalvaged();
		}
		else
		{
			Problem(FString::Printf(TEXT("saved salvage placement %s no longer exists"), *Id.ToString()));
		}
	}

	AActor* Zenny = Glitches->GetCommander();
	ApplyTransform(Zenny, Save.Zenny);
	if (UGLInventoryComponent* Inventory = Zenny ? Zenny->FindComponentByClass<UGLInventoryComponent>() : nullptr)
	{
		TArray<TPair<FName, int32>> Items;
		for (const FGLSavedCount& Item : Save.Inventory)
		{
			if (GLContent::Get().Find<FGLItemDef>(Item.Id))
			{
				Items.Emplace(Item.Id, Item.Count);
			}
			else
			{
				Problem(FString::Printf(TEXT("saved item %s no longer exists"), *Item.Id.ToString()));
			}
		}
		Inventory->RestoreContents(Items);
	}
	if (UGLKnowledgeSubsystem* Knowledge = World->GetSubsystem<UGLKnowledgeSubsystem>())
	{
		Knowledge->Restore(Save.Knowledge);
	}
	if (AGLPehlichi* Pehlichi = FindPehlichi())
	{
		ApplyTransform(Pehlichi, Save.Pehlichi);
		for (const FGLSavedCount& Level : Save.PehlichiCapabilities)
		{
			Pehlichi->GetCapabilities()->Grant(Level.Id, Level.Count);
		}
	}
	if (UGLDialogueDirector* Director = World->GetSubsystem<UGLDialogueDirector>())
	{
		TArray<TPair<FName, int32>> Uses, Counts;
		for (const FGLSavedCount& Use : Save.ExchangeUses)
		{
			Uses.Emplace(Use.Id, Use.Count);
		}
		for (const FGLSavedCount& Count : Save.EventCounts)
		{
			Counts.Emplace(Count.Id, Count.Count);
		}
		Director->RestoreHistory(Uses, Counts);
	}
	if (UGLWorldSettingsSubsystem* Settings = World->GetSubsystem<UGLWorldSettingsSubsystem>(); Settings && !Save.SettingsPreset.IsNone())
	{
		Settings->SetPreset(Save.SettingsPreset);
	}
	Glitches->EvaluateRequirements();
}

FString UGLSaveSubsystem::SlotPath(const FString& Slot)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("Gridlands"), Slot + TEXT(".json"));
}

bool UGLSaveSubsystem::SlotExists(const FString& Slot)
{
	return FPaths::FileExists(SlotPath(Slot));
}

bool UGLSaveSubsystem::SaveToSlot(const FString& Slot) const
{
	const bool bSaved = FFileHelper::SaveStringToFile(GLSaveCodec::ToJson(Capture()), *SlotPath(Slot), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogGridlands, Log, TEXT("Save: %s %s"), bSaved ? TEXT("wrote") : TEXT("FAILED to write"), *SlotPath(Slot));
	return bSaved;
}

bool UGLSaveSubsystem::LoadFromSlot(const FString& Slot, TArray<FString>* OutProblems)
{
	FString Text, Problem;
	FGLWorldSave Save;
	if (!FFileHelper::LoadFileToString(Text, *SlotPath(Slot)) || !GLSaveCodec::FromJson(Text, Save, Problem))
	{
		UE_LOG(LogGridlands, Warning, TEXT("Load: %s: %s"), *SlotPath(Slot), Problem.IsEmpty() ? TEXT("missing") : *Problem);
		return false;
	}
	Apply(Save, OutProblems);
	UE_LOG(LogGridlands, Log, TEXT("Load: restored %d glitches, %d salvaged placements from %s"), Save.Glitches.Num(), Save.SalvagedPlacements.Num(), *SlotPath(Slot));
	return true;
}

void UGLSaveSubsystem::HandleGlitchRepaired(const FGLGameplayEvent&)
{
	if (bAutosave)
	{
		SaveToSlot(AutosaveSlot);
	}
}
