#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLStyleSubsystem.generated.h"

class APostProcessVolume;
class UMaterialInstanceDynamic;

/**
 * The Gridlands look at runtime (P7 visual spike): the stylize post-process (graphic outlines, light
 * banding) and colourful lighting presets. Every knob is a console command so each feature's cost can
 * be measured and the operator can compare. Presentation only; nothing here changes gameplay.
 *   gl.Style.Preset day|dusk|night    gl.Style.Post 0|1    gl.Style.Outline 0|1    gl.Style.Cel 0|1
 *   gl.Style.Param <name> <value>     (any PP_GLStylize scalar parameter)
 *   gl.Style.Variant P7|A|B|C          (P7.1 visual-direction experiments, layered on every preset)
 * P7.1 variants (operator review; none approved): P7 = the P7 treatment (outlines everywhere, light
 * banding); A = dimensional, environment outlines off, characters lightly outlined; B = dimensional,
 * environment silhouettes only, lighter and fading with distance; C = a more grounded dimensional
 * treatment (softer contrast, deeper atmosphere) that keeps the strong palette.
 */
UCLASS()
class GRIDLANDSGAME_API UGLStyleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void ApplyPreset(FName Preset);
	/** Chooses a P7.1 variant and re-applies the current preset with it. */
	void SetVariant(FName InVariant);
	FName GetVariant() const { return Variant; }
	void SetPostEnabled(bool bEnabled);
	void SetParam(FName Name, float Value);
	FName GetPreset() const { return Preset; }
	bool IsPostEnabled() const { return bPost; }

private:
	UPROPERTY() TObjectPtr<APostProcessVolume> Volume;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Stylize;
	FName Preset = TEXT("day");
	FName Variant = TEXT("B");
	bool bPost = true;
};
