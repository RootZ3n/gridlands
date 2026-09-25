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
UCLASS()
class GRIDLANDSGAME_API UGLTerrainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Builds the ground a cell's data declares (terrain block + playable extent). False if it has none. */
	bool SetupCell(FName CellId);
	/** Explicit ground (tests, tools): ChunksX x ChunksY chunks of ChunkVerts vertices, SpacingCm apart. */
	void Setup(const FVector2D& Origin, int32 ChunksX, int32 ChunksY, int32 ChunkVerts, double SpacingCm, float BaseHeightCm,
		double MaxDigCm, double MaxRaiseCm, TArray<float>* AuthoredBase = nullptr);

	bool HasGround() const { return Chunks.Num() > 0; }
	double HeightAt(const FVector2D& World) const { return Field.HeightAt(World); }
	const FGLHeightfield& GetField() const { return Field; }
	const TArray<TObjectPtr<AGLTerrainChunk>>& GetChunks() const { return Chunks; }

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

	/** Save support: sparse delta from the base (whole cm), restored onto a freshly set-up ground. */
	void CaptureDelta(TArray<int32>& OutIndices, TArray<int32>& OutDeltaCm) const { Field.EncodeDelta(OutIndices, OutDeltaCm); }
	bool RestoreDelta(TConstArrayView<int32> Indices, TConstArrayView<int32> DeltaCm);

	/** Test control only: when false, edits do not tell navigation (to prove the tests can fail). */
	bool bNotifyNavigation = true;

private:
	void Clear();
	void Emit(const TCHAR* Tag, FName Subject, AActor* Instigator, const FString& Reason);

	FGLHeightfield Field;
	int32 VertsPerChunk = 0;
	UPROPERTY() TArray<TObjectPtr<AGLTerrainChunk>> Chunks;
	UPROPERTY() TObjectPtr<AGLCellNavBounds> NavBounds;
};
