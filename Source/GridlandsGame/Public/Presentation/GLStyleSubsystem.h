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
 */
UCLASS()
class GRIDLANDSGAME_API UGLStyleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void ApplyPreset(FName Preset);
	void SetPostEnabled(bool bEnabled);
	void SetParam(FName Name, float Value);
	FName GetPreset() const { return Preset; }
	bool IsPostEnabled() const { return bPost; }

private:
	UPROPERTY() TObjectPtr<APostProcessVolume> Volume;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Stylize;
	FName Preset = TEXT("day");
	bool bPost = true;
};
