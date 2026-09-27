#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Terrain/GLHeightfield.h"
#include "GLTerrainSubsystem.generated.h"

class AGLCellNavBounds;
class AGLTerrainChunk;

/**
 * A cell's runtime ground (ADR-0022): the pure heightfield, the chunk actors that render and
 * collide it, and the cell's navigation bounds. Every edit rebuilds only the touched chunks and
 * tells navigation, so paths follow the new ground. Saves hold the sparse delta only.
 */
struct FGLPendingGround;
struct FGLMeshJob;

/** How a cell's ground is laid out (from its data, or explicit for tools). */
struct FGLGroundParams
{
	FVector2D Origin = FVector2D::ZeroVector;
	int32 ChunksX = 1;
	int32 ChunksY = 1;
	int32 ChunkVerts = 65;
	double SpacingCm = 100.0;
	double BaseHeightCm = 0.0;
	double MaxDigCm = 400.0;
	double MaxRaiseCm = 400.0;
	int32 ReliefSeed = 0;
	double ReliefAmplitudeCm = 0.0;
	double ReliefEdgeBlendVerts = 1.0;
};

/** One chunk of a ground: its actor (once built) and which heights version it shows. */
USTRUCT()
struct FGLChunkSlot
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<AGLTerrainChunk> Actor;
	FIntPoint First = FIntPoint::ZeroValue;
	/** Bumped by every edit that touches it; a mesh built from older heights is stale. */
	int32 Version = 0;
	int32 BuiltVersion = -1;
	int32 InFlightVersion = -1;
};

/** One cell's ground: its heightfield, chunk slots and navigation bounds. */
USTRUCT()
struct FGLCellGround
{
	GENERATED_BODY()

	FGLHeightfield Field;
	/** The cell this ground belongs to (chunk actors carry it while they are Live). */
	FName Cell;
	int32 VertsPerChunk = 0;
	/** Which load of the cell this is: results for an earlier one are discarded. */
	int32 Generation = 0;
	UPROPERTY() TArray<FGLChunkSlot> Slots;
	UPROPERTY() TObjectPtr<AGLCellNavBounds> NavBounds;
};

/** Streaming counters (evidence and tests). */
struct FGLTerrainStreamStats
{
	int32 ChunksApplied = 0;
	int32 StaleDropped = 0;
	int32 EmergencyChunks = 0;
	int32 EmergencyFields = 0;
	int32 ChunksReused = 0;
	/** Chunk actors created during play (not from the pool): each one's first mesh is the expensive build. */
	int32 ChunksSpawned = 0;
	/** Streamed applies onto an actor's first mesh (fresh) vs an actor that already had one (pooled or warmed). */
	int32 FirstBuildApplies = 0;
	double FirstBuildMsSum = 0.0;
	double FirstBuildMsMax = 0.0;
	int32 ReusedApplies = 0;
	double ReusedMsSum = 0.0;
	double ReusedMsMax = 0.0;
	/** Pooled entries that were not in the Pooled state when taken (never reused; a defect if ever non-zero). */
	int32 PoolRejected = 0;
	/** Cleared chunk actors destroyed because the pool was full. */
	int32 PoolOverflowDestroyed = 0;
};

/**
 * Runtime ground for every loaded Grid cell (ADR-0022, P3, P5). Per cell: the pure heightfield, the
 * chunk actors that render and collide it, and its navigation bounds. Streaming (ADR-0028): a
 * cell's field is built on a worker, its chunk meshes are built on workers nearest-first and
 * applied within a per-frame budget (collision cooked asynchronously), and the ground under
 * Zenny is guaranteed on demand. Edits bump chunk versions so stale results are dropped. An edit
 * across a cell edge changes both cells atomically, and is refused if the neighbour is not loaded.
 */
/** What the last Pump spent its time on (evidence for slow streaming frames). */
struct FGLTerrainPumpBreakdown
{
	double FinishMs = 0.0;
	double ApplyMs = 0.0;
	double WorstApplyMs = 0.0;
	double WorstMeshMs = 0.0;
	double WorstCollisionMs = 0.0;
	double WorstNavigationMs = 0.0;
	int32 Applied = 0;
	double LaunchMs = 0.0;
	double RetireMs = 0.0;
	int32 Retired = 0;
};

UCLASS()
class GRIDLANDSGAME_API UGLTerrainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UGLTerrainSubsystem();
	virtual ~UGLTerrainSubsystem() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Builds a cell's whole ground now (tools, tests, legacy maps), with its saved edits applied. */
	bool SetupCell(FName CellId, TConstArrayView<int32> DeltaIndices = {}, TConstArrayView<int32> DeltaCm = {});
	/** Streaming (P5): starts building a cell's field on a worker; chunks follow through Pump. */
	bool BeginCellGround(FName CellId, TConstArrayView<int32> DeltaIndices = {}, TConstArrayView<int32> DeltaCm = {});
	bool IsCellPending(FName CellId) const { return Pending.Contains(CellId); }
	/** The cell's field has been generated off-thread and waits for the next Pump to become ground. */
	bool IsCellFieldReady(FName CellId) const;
	/** Streaming work for one frame: finished fields, finished meshes (within BudgetSeconds), new jobs nearest Near. */
	void Pump(const FVector2D& Near, double BudgetSeconds = 0.003, int32 MaxInFlight = 8);
	/** Guarantees built ground (with collision) within RadiusCm of a point. False if no ground covers it. */
	bool EnsureReadyAt(const FVector2D& World, double RadiusCm = 400.0);
	/** Finishes every pending field and chunk now (tests, tools). */
	void FlushAll();
	bool IsCellComplete(FName CellId) const;
	const FGLTerrainStreamStats& GetStats() const { return Stats; }
	const FGLTerrainPumpBreakdown& GetLastPump() const { return LastPump; }
	int32 NumRetiring() const { return Retiring.Num(); }
	int32 NumPooled() const { return Pool.Num(); }
	/**
	 * The pool's bound: cleared chunk actors kept for reuse (beyond it, retired actors are destroyed).
	 * One cell is 16 x 16 = 256 chunks and at most two cells are live at once (the unload margin), so
	 * 512 lets a full cell come back entirely from the pool; memory stays bounded (cleared actors hold
	 * no mesh or collision).
	 */
	int32 PoolLimit = 512;
	/** Evidence and tests: every chunk actor, live or pooled, is in exactly one place. */
	bool CheckChunkIntegrity(TArray<FString>* OutProblems = nullptr) const;
	int32 NumJobs() const { return Jobs.Num(); }

	/** Removes a cell's ground (pending field, chunks, navigation bounds). */
	bool RemoveCell(FName CellId);
	/** Explicit ground (tests, tools) under the id None: ChunksX x ChunksY chunks of ChunkVerts vertices, built now. */
	void Setup(const FVector2D& Origin, int32 ChunksX, int32 ChunksY, int32 ChunkVerts, double SpacingCm, float BaseHeightCm,
		double MaxDigCm, double MaxRaiseCm, TArray<float>* AuthoredBase = nullptr);

	bool HasGround() const { return Grounds.Num() > 0; }
	bool HasGroundAt(const FVector2D& World) const { return GroundAt(World) != nullptr; }
	bool HasCell(FName CellId) const { return Grounds.Contains(CellId); }
	/** The loaded ground under a point, if any (its cell id; None for explicit test ground). */
	bool GroundCellAt(const FVector2D& World, FName& OutCell) const;
	TArray<FName> GetGroundCells() const;
	double HeightAt(const FVector2D& World) const;
	/** How far the ground here has been dug or raised from its authored base (cm; 0 if untouched or unloaded). */
	double EditedAt(const FVector2D& World) const;
	/** The explicit (None) ground, else the first loaded one (tools and single-ground tests). */
	const FGLHeightfield& GetField() const;
	const FGLHeightfield* FieldOf(FName CellId) const { const FGLCellGround* G = Grounds.Find(CellId); return G ? &G->Field : nullptr; }
	/** Built chunk actors across all grounds. */
	int32 NumChunks() const;

	/** Applies an edit; rebuilds touched chunks, their collision and their navigation. */
	FGLTerrainEditResult ApplyEdit(const FGLTerrainEdit& Edit, TFunctionRef<bool(const FVector2D&)> IsProtected);
	FGLTerrainEditResult ApplyEdit(const FGLTerrainEdit& Edit) { return ApplyEdit(Edit, [](const FVector2D&) { return false; }); }

	/**
	 * A player's terraforming stroke (data: terraform.*). Needs the tool; pays the cost and
	 * receives the yields only if the ground actually changes (no free soil, no lost soil);
	 * the ground under structures is protected. Flatten levels to the ground height at Centre.
	 * Emits Event.Terrain.Edited or Event.Terrain.Refused.
	 */
	FGLTerrainEditResult Terraform(AActor* Instigator, FName TerraformId, const FVector2D& Centre);

	/** Save support per cell: sparse delta from the authored base (whole cm). */
	bool CaptureCellDelta(FName CellId, TArray<int32>& OutIndices, TArray<int32>& OutDeltaCm) const;
	/** Restores a delta onto a loaded cell's ground, rebuilding only the chunks that change. */
	bool RestoreCellDelta(FName CellId, TConstArrayView<int32> Indices, TConstArrayView<int32> DeltaCm);
	/** The explicit (None) or only ground (tools, single-ground tests). */
	void CaptureDelta(TArray<int32>& OutIndices, TArray<int32>& OutDeltaCm) const;
	bool RestoreDelta(TConstArrayView<int32> Indices, TConstArrayView<int32> DeltaCm);

	static bool GroundParamsFor(FName CellId, FGLGroundParams& Out);
	static TSharedPtr<FGLHeightfield> MakeField(const FGLGroundParams& Params, TConstArrayView<int32> DeltaIndices, TConstArrayView<int32> DeltaCm);

	/** Test control only: when false, edits do not tell navigation (to prove the tests can fail). */
	bool bNotifyNavigation = true;

private:
	void FinishGround(FName CellId, FGLHeightfield&& Field, const FGLGroundParams& Params, int32 Generation, bool bBuildNow);
	void BuildSlotNow(FGLCellGround& Ground, FGLChunkSlot& Slot, bool bAsyncCollision);
	void CancelPending(FName CellId);
	AGLTerrainChunk* AcquireChunk(const FGLCellGround& Ground, const FGLChunkSlot& Slot);
	void RetireOne();
	const FGLCellGround* GroundAt(const FVector2D& World) const;
	void Emit(const TCHAR* Tag, FName Subject, AActor* Instigator, const FString& Reason);

	UPROPERTY() TMap<FName, FGLCellGround> Grounds;
	FGLTerrainPumpBreakdown LastPump;
	TMap<FName, TSharedPtr<FGLPendingGround>> Pending;
	TArray<TSharedPtr<FGLMeshJob>> Jobs;
	UPROPERTY() TArray<TWeakObjectPtr<AGLTerrainChunk>> Retiring;
	/** Cleared chunk actors waiting to be reused (no churn, bounded memory). */
	UPROPERTY() TArray<TWeakObjectPtr<AGLTerrainChunk>> Pool;
	int32 GenerationCounter = 0;
	FGLTerrainStreamStats Stats;
};
