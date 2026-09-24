#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLWorldSettingsSubsystem.generated.h"

struct FGLSettingsPresetDef;

/**
 * The world's settings (ADR-0016): which settings preset applies to yields. Persisted with the
 * world save in M9; until then every world starts on settings.preset.default.
 */
UCLASS()
class GRIDLANDSGAME_API UGLWorldSettingsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Returns false (and keeps the current preset) if Id is not a settings preset. */
	bool SetPreset(FName Id);
	FName GetPresetId() const { return PresetId; }
	/** The active preset definition; never null while content loads cleanly. */
	const FGLSettingsPresetDef* GetPreset() const;

private:
	FName PresetId = TEXT("settings.preset.default");
};
