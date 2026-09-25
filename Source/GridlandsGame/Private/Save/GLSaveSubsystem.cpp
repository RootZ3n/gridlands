#include "Save/GLSaveSubsystem.h"

#include "TimerManager.h"

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
#include "Puzzle/GLPuzzleSubsystem.h"
#include "Building/GLBuildingSubsystem.h"
#include "Content/GLContentDefinitions.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "Storm/GLStormSubsystem.h"
#include "World/GLAmbientSubsystem.h"
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
		// Building and terraforming are world changes too: save them soon after, not only on quit.
		for (const TCHAR* Tag : { TEXT("Event.Building.Placed"), TEXT("Event.Building.Demolished"), TEXT("Event.Terrain.Edited") })
		{
			Bus->Subscribe(UGameplayTagsManager::Get().RequestGameplayTag(Tag),
				FGLGameplayEventDelegate::CreateUObject(this, &UGLSaveSubsystem::HandleWorldEdited));
		}
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
	if (const UGLPlacementSubsystem* Placed = World->GetSubsystem<UGLPlacementSubsystem>())
	{
		for (const TPair<FName, TWeakObjectPtr<AGLCreature>>& Entry : Placed->GetCreatures())
		{
			if (Entry.Value.IsValid() && Entry.Value->IsDefeated())
			{
				Save.DefeatedCreatures.Add(Entry.Key);
			}
		}
		Save.DefeatedCreatures.Sort(FNameLexicalLess());
	}
	if (const UGLAmbientSubsystem* Ambient = World->GetSubsystem<UGLAmbientSubsystem>())
	{
		Save.Discoveries = Ambient->GetDiscovered();
		Save.Discoveries.Sort(FNameLexicalLess());
	}
	if (const UGLStormSubsystem* Storms = World->GetSubsystem<UGLStormSubsystem>())
	{
		Save.StormsOccurred = Storms->GetOccurred();
	}
	if (const UGLHealthComponent* Life = Zenny ? Zenny->FindComponentByClass<UGLHealthComponent>() : nullptr)
	{
		Save.ZennyHealth = Life->IsDead() ? Life->GetMax() : Life->GetCurrent(); // dying then saving wakes you whole
	}
	if (const UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>())
	{
		for (const FGLPlacedPiece& Piece : Building->GetPieces())
		{
			Save.BuildPieces.Add({ Piece.Id, Piece.Def, Piece.Location, Piece.YawQuarter });
		}
		Save.NextPieceId = Building->GetNextId();
	}
	if (const UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>(); Terrain && Terrain->HasGround())
	{
		Terrain->CaptureDelta(Save.TerrainIndices, Save.TerrainDeltaCm);
	}
	if (const UGLPuzzleSubsystem* Puzzles = World->GetSubsystem<UGLPuzzleSubsystem>())
	{
		Save.SolvedPuzzles = Puzzles->GetSolved().Array();
		Save.SolvedPuzzles.Sort(FNameLexicalLess());
		Save.PosedPuzzles = Puzzles->GetPosed();
		for (const TPair<FName, int32>& Hint : Puzzles->GetHintLevels())
		{
			Save.PuzzleHints.Add({ Hint.Key, Hint.Value });
		}
		Save.PuzzleHints.Sort([](const FGLSavedCount& A, const FGLSavedCount& B) { return A.Id.LexicalLess(B.Id); });
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
	if (UGLHealthComponent* Life = Zenny ? Zenny->FindComponentByClass<UGLHealthComponent>() : nullptr; Life && Save.ZennyHealth >= 0.0)
	{
		Life->Restore(Save.ZennyHealth);
	}
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
	for (const FName& Id : Save.DefeatedCreatures)
	{
		if (AGLCreature* Creature = Placements->FindCreature(Id))
		{
			Creature->RestoreDefeated();
		}
		else
		{
			Problem(FString::Printf(TEXT("saved defeated creature %s no longer exists"), *Id.ToString()));
		}
	}
	if (UGLAmbientSubsystem* Ambient = World->GetSubsystem<UGLAmbientSubsystem>())
	{
		Ambient->Restore(Save.Discoveries);
	}
	if (UGLStormSubsystem* Storms = World->GetSubsystem<UGLStormSubsystem>())
	{
		TMap<FName, int32> Counts;
		for (const FGLSavedCount& Count : Save.EventCounts)
		{
			Counts.Add(Count.Id, Count.Count);
		}
		Storms->Restore(Save.StormsOccurred, Counts);
	}
	// Ground first, then the pieces that stand on it (support is derived from both).
	if (UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>(); Terrain && Terrain->HasGround())
	{
		if (!Terrain->RestoreDelta(Save.TerrainIndices, Save.TerrainDeltaCm))
		{
			Problem(TEXT("saved terrain does not fit this cell's ground; ground left as authored"));
		}
	}
	if (UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>())
	{
		TArray<FGLPlacedPiece> Pieces;
		for (const FGLSavedPiece& Saved : Save.BuildPieces)
		{
			if (!GLContent::Get().Find<FGLBuildPieceDef>(Saved.Def))
			{
				Problem(FString::Printf(TEXT("saved build piece %s no longer exists"), *Saved.Def.ToString()));
				continue;
			}
			Pieces.Add({ Saved.Id, Saved.Def, Saved.Location, Saved.YawQuarter });
		}
		Building->Restore(Pieces, Save.NextPieceId);
	}
	if (UGLPuzzleSubsystem* Puzzles = World->GetSubsystem<UGLPuzzleSubsystem>())
	{
		TArray<TPair<FName, int32>> Hints;
		for (const FGLSavedCount& Hint : Save.PuzzleHints)
		{
			Hints.Emplace(Hint.Id, Hint.Count);
		}
		Puzzles->Restore(Save.SolvedPuzzles, Save.PosedPuzzles, Hints);
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
	UE_LOG(LogGridlands, Log, TEXT("Load: restored %d glitches, %d salvaged placements, %d build pieces, %d edited ground vertices from %s"),
		Save.Glitches.Num(), Save.SalvagedPlacements.Num(), Save.BuildPieces.Num(), Save.TerrainIndices.Num(), *SlotPath(Slot));
	return true;
}

void UGLSaveSubsystem::HandleWorldEdited(const FGLGameplayEvent&)
{
	UWorld* World = GetWorld();
	if (!bAutosave || !World || World->GetTimerManager().IsTimerActive(DebouncedSave))
	{
		return;
	}
	// Debounced: a burst of edits (a whole wall, ten shovel strokes) becomes one write.
	World->GetTimerManager().SetTimer(DebouncedSave, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (bAutosave)
		{
			SaveToSlot(AutosaveSlot);
		}
	}), AutosaveDelaySeconds, false);
}

void UGLSaveSubsystem::HandleGlitchRepaired(const FGLGameplayEvent&)
{
	if (bAutosave)
	{
		SaveToSlot(AutosaveSlot);
	}
}
