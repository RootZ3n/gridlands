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

USTRUCT()
struct GRIDLANDSCORE_API FGLWorldSave
{
	GENERATED_BODY()

	static constexpr int32 CurrentVersion = 1;

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
	UPROPERTY() FGLSavedTransform Zenny;
	UPROPERTY() FGLSavedTransform Pehlichi;
};

/** JSON encoding, versions and migrations for FGLWorldSave (pure). */
namespace GLSaveCodec
{
	GRIDLANDSCORE_API FString ToJson(const FGLWorldSave& Save);

	/** Parses and migrates. Returns false (with Problem) for unreadable text, a missing version, or a newer version. */
	GRIDLANDSCORE_API bool FromJson(const FString& Text, FGLWorldSave& OutSave, FString& OutProblem);
}
