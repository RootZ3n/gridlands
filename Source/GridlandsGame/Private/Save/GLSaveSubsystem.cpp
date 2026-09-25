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
#include "World/GLGridCells.h"
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
		Save.NextPieceId = Building->GetNextId();
	}
	// Per-cell state (v2): live for loaded cells, the kept record for streamed-out ones.
	const TSet<FName> Loaded = LoadedCells();
	for (const FName& Cell : Loaded)
	{
		FGLSavedCell Record = CaptureCell(Cell);
		if (!Record.IsEmpty())
		{
			Save.Cells.Add(MoveTemp(Record));
		}
	}
	for (const TPair<FName, FGLSavedCell>& Kept : Dormant)
	{
		if (!Loaded.Contains(Kept.Key) && !Kept.Value.IsEmpty())
		{
			Save.Cells.Add(Kept.Value);
		}
	}
	Save.Cells.Sort([](const FGLSavedCell& A, const FGLSavedCell& B) { return A.Cell.LexicalLess(B.Cell); });
	Save.Cell = Zenny ? GLGridCells::CellAt(FVector2D(Zenny->GetActorLocation())) : NAME_None;
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
	return Save;
}

TSet<FName> UGLSaveSubsystem::LoadedCells() const
{
	UWorld* World = GetWorld();
	TSet<FName> Cells;
	if (const UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>())
	{
		Cells.Append(Placements->GetSpawnedCells());
	}
	if (const UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>())
	{
		Cells.Append(Terrain->GetGroundCells());
	}
	if (const UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>())
	{
		Cells.Append(Building->CellsWithPieces());
	}
	return Cells;
}

FGLSavedCell UGLSaveSubsystem::CaptureCell(FName Cell) const
{
	UWorld* World = GetWorld();
	FGLSavedCell Record;
	Record.Cell = Cell;
	for (const TWeakObjectPtr<AGLGlitch>& Actor : World->GetSubsystem<UGLGlitchSubsystem>()->GetAll())
	{
		const UGLGlitchComponent* Glitch = Actor.IsValid() ? Actor->GetGlitch() : nullptr;
		if (Glitch && UGLPlacementSubsystem::IsPlacementOfCell(Glitch->GetPlacementId(), Cell))
		{
			Record.Glitches.Add({ Glitch->GetPlacementId(), FGLGlitchLifecycle::ToPersistedState(Glitch->GetState()), Glitch->GetProgressSeconds(), Glitch->AreItemsDelivered() });
		}
	}
	for (TActorIterator<AGLSalvageNode> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetSalvageable()->IsSalvaged() && UGLPlacementSubsystem::IsPlacementOfCell(It->PlacementId, Cell))
		{
			Record.SalvagedPlacements.Add(It->PlacementId);
		}
	}
	if (const UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>())
	{
		for (const TPair<FName, TWeakObjectPtr<AGLCreature>>& Entry : Placements->GetCreatures())
		{
			if (Entry.Value.IsValid() && Entry.Value->IsDefeated() && UGLPlacementSubsystem::IsPlacementOfCell(Entry.Key, Cell))
			{
				Record.DefeatedCreatures.Add(Entry.Key);
			}
		}
	}
	if (const UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>())
	{
		for (const FGLPlacedPiece& Piece : Building->PiecesOfCell(Cell))
		{
			Record.BuildPieces.Add({ Piece.Id, Piece.Def, Piece.Location, Piece.YawQuarter });
		}
		Record.BuildPieces.Sort([](const FGLSavedPiece& A, const FGLSavedPiece& B) { return A.Id < B.Id; });
	}
	if (const UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>())
	{
		Terrain->CaptureCellDelta(Cell, Record.TerrainIndices, Record.TerrainDeltaCm);
	}
	Record.Glitches.Sort([](const FGLSavedGlitch& A, const FGLSavedGlitch& B) { return A.Placement.LexicalLess(B.Placement); });
	Record.SalvagedPlacements.Sort(FNameLexicalLess());
	Record.DefeatedCreatures.Sort(FNameLexicalLess());
	return Record;
}

void UGLSaveSubsystem::ApplyCell(const FGLSavedCell& Record, TArray<FString>* OutProblems)
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
	// Everything here restores silently: no events, so no dialogue replays because a cell came back.
	for (const FGLSavedGlitch& Saved : Record.Glitches)
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
	for (const FName& Id : Record.SalvagedPlacements)
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
	for (const FName& Id : Record.DefeatedCreatures)
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
	// Ground first, then the pieces that stand on it (support is derived from both).
	if (UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>(); Terrain && Terrain->HasCell(Record.Cell))
	{
		if (!Terrain->RestoreCellDelta(Record.Cell, Record.TerrainIndices, Record.TerrainDeltaCm))
		{
			Problem(FString::Printf(TEXT("saved terrain does not fit %s's ground; ground left as authored"), *Record.Cell.ToString()));
		}
	}
	if (UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>())
	{
		TArray<FGLPlacedPiece> Pieces;
		for (const FGLSavedPiece& Saved : Record.BuildPieces)
		{
			if (!GLContent::Get().Find<FGLBuildPieceDef>(Saved.Def))
			{
				Problem(FString::Printf(TEXT("saved build piece %s no longer exists"), *Saved.Def.ToString()));
				continue;
			}
			Pieces.Add({ Saved.Id, Saved.Def, Saved.Location, Saved.YawQuarter, Record.Cell });
		}
		Building->RemoveCell(Record.Cell);
		Building->RestoreCell(Record.Cell, Pieces);
	}
}

void UGLSaveSubsystem::StowCell(FName Cell)
{
	Dormant.Add(Cell, CaptureCell(Cell));
}

bool UGLSaveSubsystem::TakeDormant(FName Cell, FGLSavedCell& Out)
{
	return Dormant.RemoveAndCopyValue(Cell, Out);
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

	// Per-cell state (v2): loaded cells now, the rest kept until their cell streams in.
	Dormant.Reset();
	const TSet<FName> Loaded = LoadedCells();
	for (const FGLSavedCell& Record : Save.Cells)
	{
		if (Loaded.Contains(Record.Cell))
		{
			ApplyCell(Record, OutProblems);
		}
		else
		{
			Dormant.Add(Record.Cell, Record);
		}
	}
	// Flat v1-style fields (built in code, e.g. by tools and tests): applied to whatever is live.
	FGLSavedCell Flat;
	Flat.Cell = Save.Cell;
	Flat.Glitches = Save.Glitches;
	Flat.SalvagedPlacements = Save.SalvagedPlacements;
	Flat.DefeatedCreatures = Save.DefeatedCreatures;
	if (!Flat.IsEmpty())
	{
		ApplyCell(Flat, OutProblems);
	}
	if (UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>())
	{
		Building->SetNextId(Save.NextPieceId);
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
