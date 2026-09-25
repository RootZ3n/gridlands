#pragma once

#include "CoreMinimal.h"
#include "Save/GLWorldSave.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLSaveSubsystem.generated.h"

/**
 * The world save (ADR-0019). Captures facts from the live world and applies them after the cell's
 * placements have spawned. Files: Saved/SaveGames/Gridlands/<slot>.json. Derived values are never
 * saved; they are recomputed from the restored facts (S-1).
 */
UCLASS()
class GRIDLANDSGAME_API UGLSaveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	FGLWorldSave Capture() const;
	/** Applies Save to this world. Facts about ids the current content no longer has are reported, not fatal. */
	void Apply(const FGLWorldSave& Save, TArray<FString>* OutProblems = nullptr);

	/** Grid streaming (P3): the cells whose state is live (placements, ground or pieces present). */
	TSet<FName> LoadedCells() const;
	/** One cell's live state. */
	FGLSavedCell CaptureCell(FName Cell) const;
	/** Applies a cell record to that cell's live actors (silently: no events). */
	void ApplyCell(const FGLSavedCell& Record, TArray<FString>* OutProblems = nullptr);
	/** Keeps a cell's state before it streams out. */
	void StowCell(FName Cell);
	/** Hands back (and forgets) a streamed-out cell's kept state. False if there is none. */
	bool TakeDormant(FName Cell, FGLSavedCell& Out);
	const FGLSavedCell* PeekDormant(FName Cell) const { return Dormant.Find(Cell); }
	const TMap<FName, FGLSavedCell>& GetDormant() const { return Dormant; }

	bool SaveToSlot(const FString& Slot) const;
	bool LoadFromSlot(const FString& Slot, TArray<FString>* OutProblems = nullptr);
	static FString SlotPath(const FString& Slot);
	static bool SlotExists(const FString& Slot);

	/** When on, every repair autosaves to AutosaveSlot. The game mode enables it; tests leave it off. */
	bool bAutosave = false;
	/** Seconds after a build or terrain edit before the autosave (edits in between share it). */
	float AutosaveDelaySeconds = 5.f;
	FString AutosaveSlot = TEXT("world");

private:
	void HandleGlitchRepaired(const struct FGLGameplayEvent& Event);
	void HandleWorldEdited(const struct FGLGameplayEvent& Event);
	FTimerHandle DebouncedSave;
	/** State of cells that are not loaded (from the save file, or captured when they streamed out). */
	TMap<FName, FGLSavedCell> Dormant;
	class AGLPehlichi* FindPehlichi() const;
};
