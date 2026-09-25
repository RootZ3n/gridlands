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
