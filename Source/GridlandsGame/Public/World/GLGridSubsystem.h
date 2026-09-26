#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLGridSubsystem.generated.h"

class AGLGridBoundary;
class ULevelStreamingDynamic;

/** A Grid cell in the world (loading or ready). */
USTRUCT()
struct FGLLoadedCell
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<ULevelStreamingDynamic> Level;
	UPROPERTY() TObjectPtr<AGLGridBoundary> Boundary;
	/** Which load of this cell (a reload after an unload is a new epoch). */
	int32 Epoch = 0;
	/** Its authoritative runtime layer is in: placements made and its kept state applied (P7: before any presentation). */
	bool bRuntime = false;
	double StartedAt = 0.0;
	double GroundAt = -1.0;
	double RuntimeAt = -1.0;
	/** Every deferred presentation unit (structure part actors, vegetation) has been made. */
	double PresentedAt = -1.0;
	double CompleteAt = -1.0;
};

/** Timings of one cell load (evidence). */
struct FGLCellLoadRecord
{
	FName Cell;
	int32 Epoch = 0;
	double GroundSeconds = -1.0;
	/** Authoritative gameplay state ready (saved state resolved). */
	double RuntimeSeconds = -1.0;
	/** Presentation complete (all deferred actors made). */
	double PresentedSeconds = -1.0;
	/** Ground complete and presentation complete. */
	double CompleteSeconds = -1.0;
	bool bCancelled = false;
};

/**
 * Grid-cell streaming (ADR-0026, seamless per ADR-0028). Cells within LoadMarginM of Zenny start
 * loading: the authored level asynchronously, the ground on workers (UGLTerrainSubsystem::Pump,
 * nearest chunks first, within a per-frame budget). A cell's runtime layer (placements, kept state,
 * pieces) appears once its level is visible and its field exists. Cells beyond UnloadMarginM are
 * stowed and removed. Zenny never stands over missing ground (EnsureReadyAt). Epochs keep an old
 * load's work from touching a newer one; a half-loaded cell never overwrites its kept state.
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
	/** One frame of streaming for a position (Tick uses the player; tests call it with their own). */
	void Advance(const FVector& Where, double BudgetSeconds = 0.003);
	/** Starts and stops cell loads for a position (no building work). */
	void Update(const FVector& Where);
	/** Finishes everything in flight now (tests; never used in play). */
	void FlushAll();

	bool LoadCell(FName Cell);
	bool UnloadCell(FName Cell);
	bool IsLoaded(FName Cell) const { return Loaded.Contains(Cell); }
	bool IsRuntimeReady(FName Cell) const { const FGLLoadedCell* C = Loaded.Find(Cell); return C && C->bRuntime; }
	bool IsComplete(FName Cell) const { const FGLLoadedCell* C = Loaded.Find(Cell); return C && C->CompleteAt >= 0.0; }
	int32 GetEpoch(FName Cell) const { const FGLLoadedCell* C = Loaded.Find(Cell); return C ? C->Epoch : 0; }
	TArray<FName> GetLoadedCells() const { TArray<FName> Out; Loaded.GetKeys(Out); Out.Sort(FNameLexicalLess()); return Out; }
	FName GetCurrentCell() const { return Current; }
	int32 GetLoadCount() const { return Loads; }
	int32 GetUnloadCount() const { return Unloads; }
	const TArray<FGLCellLoadRecord>& GetLoadRecords() const { return Records; }
	/** Game-thread time the last Advance took (streaming cost per frame, evidence). */
	double GetLastAdvanceSeconds() const { return LastAdvanceSeconds; }
	/** Of that, the deferred presentation work (P7), and how many units it made. */
	double GetLastPresentationSeconds() const { return LastPresentationSeconds; }
	int32 GetLastPresentationUnits() const { return LastPresentationUnits; }
	/** Of that, the authoritative runtime layers that came in (0 in most frames). */
	double GetLastAuthoritativeSeconds() const { return LastAuthoritativeSeconds; }

	/** Start loading a neighbour this far (m) from its edge; unload beyond UnloadMarginM (hysteresis). */
	UPROPERTY(EditAnywhere, Category = "Grid") float LoadMarginM = 256.f;
	UPROPERTY(EditAnywhere, Category = "Grid") float UnloadMarginM = 384.f;
	/**
	 * P7 multi-frame presentation: the per-frame budget for instantiating deferred structure parts and
	 * vegetation (ADR-0033), and the radius around Zenny made at once regardless (what can be touched).
	 */
	UPROPERTY(EditAnywhere, Category = "Grid") float PresentationBudgetMs = 1.5f; // < 0: paused (tests)
	UPROPERTY(EditAnywhere, Category = "Grid") float PresentationNearM = 20.f;
	/** Presentation only uses what is left of this much streaming time in the frame (ground work comes first). */
	UPROPERTY(EditAnywhere, Category = "Grid") float PresentationCeilingMs = 6.f;
	/** Spawn the temporary boundary markers (development). */
	bool bShowBoundaries = true;
	/** Test control: pretend authored levels have not finished loading yet. */
	bool bHoldLevels = false;

private:
	void TryFinishRuntime(FName Cell, FGLLoadedCell& Entry);
	bool LevelReady(const FGLLoadedCell& Entry) const;
	double Now() const;

	bool bEnabled = false;
	bool bLoadLevels = true;
	FName Current;
	int32 Loads = 0;
	int32 Unloads = 0;
	int32 EpochCounter = 0;
	double LastAdvanceSeconds = 0.0;
	double LastPresentationSeconds = 0.0;
	double LastAuthoritativeSeconds = 0.0;
	int32 LastPresentationUnits = 0;
	TArray<FGLCellLoadRecord> Records;
	UPROPERTY() TMap<FName, FGLLoadedCell> Loaded;
	UPROPERTY() TMap<FName, TObjectPtr<ULevelStreamingDynamic>> CellLevels;
};
