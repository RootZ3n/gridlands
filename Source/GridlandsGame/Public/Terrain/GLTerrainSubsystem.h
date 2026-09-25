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
/** One cell's ground: its heightfield, chunk actors and navigation bounds. */
USTRUCT()
struct FGLCellGround
{
	GENERATED_BODY()

	FGLHeightfield Field;
	int32 VertsPerChunk = 0;
	UPROPERTY() TArray<TObjectPtr<AGLTerrainChunk>> Chunks;
	UPROPERTY() TObjectPtr<AGLCellNavBounds> NavBounds;
};

/**
 * Runtime ground for every loaded Grid cell (ADR-0022, P3): per cell, the pure heightfield, the
 * chunk actors that render and collide it, and its navigation bounds. Every edit rebuilds only
 * the touched chunks and tells navigation. An edit across a cell edge changes both cells
 * atomically, and is refused if the neighbour is not loaded. Saves hold sparse deltas per cell.
 */
UCLASS()
class GRIDLANDSGAME_API UGLTerrainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Builds a cell's ground from data (pitch, terrain, relief), with its saved edits already applied. */
	bool SetupCell(FName CellId, TConstArrayView<int32> DeltaIndices = {}, TConstArrayView<int32> DeltaCm = {});
	/** Removes a cell's ground (chunks and navigation bounds). */
	bool RemoveCell(FName CellId);
	/** Explicit ground (tests, tools) under the id None: ChunksX x ChunksY chunks of ChunkVerts vertices. */
	void Setup(const FVector2D& Origin, int32 ChunksX, int32 ChunksY, int32 ChunkVerts, double SpacingCm, float BaseHeightCm,
		double MaxDigCm, double MaxRaiseCm, TArray<float>* AuthoredBase = nullptr);

	bool HasGround() const { return Grounds.Num() > 0; }
	bool HasGroundAt(const FVector2D& World) const { return GroundAt(World) != nullptr; }
	bool HasCell(FName CellId) const { return Grounds.Contains(CellId); }
	/** The loaded ground under a point, if any (its cell id; None for explicit test ground). */
	bool GroundCellAt(const FVector2D& World, FName& OutCell) const;
	TArray<FName> GetGroundCells() const;
	double HeightAt(const FVector2D& World) const;
	/** The explicit (None) ground, else the first loaded one (tools and single-ground tests). */
	const FGLHeightfield& GetField() const;
	const FGLHeightfield* FieldOf(FName CellId) const { const FGLCellGround* G = Grounds.Find(CellId); return G ? &G->Field : nullptr; }
	int32 NumChunks() const { int32 N = 0; for (const TPair<FName, FGLCellGround>& G : Grounds) { N += G.Value.Chunks.Num(); } return N; }

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

	/** Test control only: when false, edits do not tell navigation (to prove the tests can fail). */
	bool bNotifyNavigation = true;

private:
	void Build(FName CellId, const FVector2D& Origin, int32 ChunksX, int32 ChunksY, int32 ChunkVerts, double SpacingCm, float BaseHeightCm,
		double MaxDigCm, double MaxRaiseCm, TArray<float>* AuthoredBase, TConstArrayView<int32> DeltaIndices, TConstArrayView<int32> DeltaCm);
	const FGLCellGround* GroundAt(const FVector2D& World) const;
	void Emit(const TCHAR* Tag, FName Subject, AActor* Instigator, const FString& Reason);

	UPROPERTY() TMap<FName, FGLCellGround> Grounds;
};
