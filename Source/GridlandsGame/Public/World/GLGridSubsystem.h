#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLGridSubsystem.generated.h"

class AGLGridBoundary;
class ULevelStreamingDynamic;

/** A Grid cell that is currently in the world. */
USTRUCT()
struct FGLLoadedCell
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<ULevelStreamingDynamic> Level;
	UPROPERTY() TObjectPtr<AGLGridBoundary> Boundary;
};

/**
 * Grid-cell streaming (P3, ADR-0026). Cells near Zenny are loaded: the cell's authored level as a
 * level instance at its world offset, then its runtime layer (ground with saved edits, placements,
 * kept state, pieces). Cells he leaves are stowed (their state captured) and removed. Hysteresis:
 * load within LoadMarginM of a cell, unload beyond UnloadMarginM.
 *
 * World Partition streams actors placed in the editor; a Grid cell's ground, placements and pieces
 * are spawned at runtime, which World Partition never unloads. So this subsystem owns the Grid
 * layer, and each cell's authored geometry uses ordinary level streaming.
 */
UCLASS()
class GRIDLANDSGAME_API UGLGridSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UGLGridSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bEnabled; }

	/** Starts streaming around the player (the game mode calls this). */
	void Enable(bool bInLoadLevels = true) { bEnabled = true; bLoadLevels = bInLoadLevels; }
	/** Loads and unloads cells for a position (Tick uses the player; tests pass positions). */
	void Update(const FVector& Where);

	bool LoadCell(FName Cell);
	bool UnloadCell(FName Cell);
	bool IsLoaded(FName Cell) const { return Loaded.Contains(Cell); }
	TArray<FName> GetLoadedCells() const { TArray<FName> Out; Loaded.GetKeys(Out); Out.Sort(FNameLexicalLess()); return Out; }
	FName GetCurrentCell() const { return Current; }
	int32 GetLoadCount() const { return Loads; }
	int32 GetUnloadCount() const { return Unloads; }

	UPROPERTY(EditAnywhere, Category = "Grid") float LoadMarginM = 48.f;
	UPROPERTY(EditAnywhere, Category = "Grid") float UnloadMarginM = 96.f;
	/** Spawn the temporary boundary markers (development). */
	bool bShowBoundaries = true;

private:
	bool bEnabled = false;
	bool bLoadLevels = true;
	FName Current;
	int32 Loads = 0;
	int32 Unloads = 0;
	double SinceUpdate = 0.0;
	UPROPERTY() TMap<FName, FGLLoadedCell> Loaded;
	UPROPERTY() TMap<FName, TObjectPtr<ULevelStreamingDynamic>> CellLevels;
};
