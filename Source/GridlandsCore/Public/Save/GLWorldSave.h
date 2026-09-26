#pragma once

#include "CoreMinimal.h"
#include "Glitch/GLGlitchLifecycle.h"
#include "GLWorldSave.generated.h"

// The world save (ADR-0019: one save is one world). Stored as human-readable, versioned JSON.
// Only FACTS are saved; derived values (stability, interference, NICE's composure) are
// recomputed on load (ADR-0013, S-1). Bump CurrentVersion only together with a migration in
// GLSaveCodec and a test.

USTRUCT()
struct GRIDLANDSCORE_API FGLSavedGlitch
{
	GENERATED_BODY()

	UPROPERTY() FName Placement;
	/** Persisted form: never Repairing (see FGLGlitchLifecycle::ToPersistedState). */
	UPROPERTY() EGLGlitchState State = EGLGlitchState::Latent;
	UPROPERTY() double ProgressSeconds = 0.0;
	UPROPERTY() bool ItemsDelivered = false;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLSavedCount
{
	GENERATED_BODY()

	UPROPERTY() FName Id;
	UPROPERTY() int32 Count = 0;
};

USTRUCT()
struct GRIDLANDSCORE_API FGLSavedTransform
{
	GENERATED_BODY()

	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() double Yaw = 0.0;
};

/** A placed build piece (ADR-0024). Support is never saved; it is derived on load. */
USTRUCT()
struct GRIDLANDSCORE_API FGLSavedPiece
{
	GENERATED_BODY()

	UPROPERTY() int32 Id = 0;
	UPROPERTY() FName Def;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() int32 YawQuarter = 0;
};

/** What happened to one part of an authored structure (P6). Intact parts are not saved (they are the authored default). */
UENUM()
enum class EGLStructurePartState : uint8
{
	Intact = 0,
	Removed = 1,        // salvaged whole
	Debris = 2,         // collapsed; lies at Location/Rotation and can be salvaged
	DebrisSalvaged = 3, // collapsed, then salvaged
};

/**
 * A structural fact (P6, ADR-0030), keyed by the structure's placement and the part's name in the
 * canonical structure data. Debris keeps its authoritative rest transform, so a returning cell never
 * replays a collapse.
 */
USTRUCT()
struct GRIDLANDSCORE_API FGLSavedStructurePart
{
	GENERATED_BODY()

	UPROPERTY() FName Placement;
	UPROPERTY() FName Part;
	UPROPERTY() EGLStructurePartState State = EGLStructurePartState::Intact;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
};

/**
 * Everything saved about one Grid cell (v2, P3): the state of what lives there. Kept while the
 * cell is streamed out, so leaving and returning never loses or replays anything.
 */
USTRUCT()
struct GRIDLANDSCORE_API FGLSavedCell
{
	GENERATED_BODY()

	UPROPERTY() FName Cell;
	UPROPERTY() TArray<FGLSavedGlitch> Glitches;
	UPROPERTY() TArray<FName> SalvagedPlacements;
	UPROPERTY() TArray<FName> DefeatedCreatures;
	UPROPERTY() TArray<FGLSavedPiece> BuildPieces;
	/** Sparse ground delta from the cell's authored base: vertex index -> whole centimetres. */
	UPROPERTY() TArray<int32> TerrainIndices;
	UPROPERTY() TArray<int32> TerrainDeltaCm;
	/** P6: every authored structure part that is no longer intact (optional in v2 files: absent means none). */
	UPROPERTY() TArray<FGLSavedStructurePart> StructureParts;

	bool IsEmpty() const { return Glitches.Num() == 0 && SalvagedPlacements.Num() == 0 && DefeatedCreatures.Num() == 0 && BuildPieces.Num() == 0 && TerrainIndices.Num() == 0 && StructureParts.Num() == 0; }
};

USTRUCT()
struct GRIDLANDSCORE_API FGLWorldSave
{
	GENERATED_BODY()

	/** v2 (P3): per-cell records in Cells; v1 files migrate (their flat cell fields become one record). */
	static constexpr int32 CurrentVersion = 2;

	UPROPERTY() int32 SchemaVersion = CurrentVersion;
	UPROPERTY() FName Cell;
	UPROPERTY() FName SettingsPreset;
	UPROPERTY() TArray<FGLSavedGlitch> Glitches;
	UPROPERTY() TArray<FName> SalvagedPlacements;
	/** Zenny's inventory, as item -> count. */
	UPROPERTY() TArray<FGLSavedCount> Inventory;
	UPROPERTY() TArray<FName> Knowledge;
	/** Pehlichi's capability levels, as capability -> level. */
	UPROPERTY() TArray<FGLSavedCount> PehlichiCapabilities;
	/** Dialogue history: exchange -> uses, and Event.* tag -> count. */
	UPROPERTY() TArray<FGLSavedCount> ExchangeUses;
	UPROPERTY() TArray<FGLSavedCount> EventCounts;
	/** Puzzles (ADR-0023): solved, posed, and hint level reached. Additive to v1: absent means none. */
	UPROPERTY() TArray<FName> SolvedPuzzles;
	UPROPERTY() TArray<FName> PosedPuzzles;
	UPROPERTY() TArray<FGLSavedCount> PuzzleHints;
	/** Building and terraforming (M10). Additive to v1: absent means none. */
	UPROPERTY() TArray<FGLSavedPiece> BuildPieces;
	UPROPERTY() int32 NextPieceId = 1;
	/** Sparse ground delta from the cell's base: vertex index -> whole centimetres (ADR-0022). */
	UPROPERTY() TArray<int32> TerrainIndices;
	UPROPERTY() TArray<int32> TerrainDeltaCm;
	/** M11. Additive: absent means none / full health. */
	UPROPERTY() TArray<FName> DefeatedCreatures;
	UPROPERTY() TArray<FName> Discoveries;
	UPROPERTY() TArray<FName> StormsOccurred;
	UPROPERTY() double ZennyHealth = -1.0;
	/** v2: one record per Grid cell that has ever been changed (loaded or not). */
	UPROPERTY() TArray<FGLSavedCell> Cells;
	UPROPERTY() FGLSavedTransform Zenny;

	const FGLSavedCell* FindCell(FName CellId) const { return Cells.FindByPredicate([CellId](const FGLSavedCell& C) { return C.Cell == CellId; }); }
	UPROPERTY() FGLSavedTransform Pehlichi;
};

/** JSON encoding, versions and migrations for FGLWorldSave (pure). */
namespace GLSaveCodec
{
	GRIDLANDSCORE_API FString ToJson(const FGLWorldSave& Save);

	/** Parses and migrates. Returns false (with Problem) for unreadable text, a missing version, or a newer version. */
	GRIDLANDSCORE_API bool FromJson(const FString& Text, FGLWorldSave& OutSave, FString& OutProblem);
}
