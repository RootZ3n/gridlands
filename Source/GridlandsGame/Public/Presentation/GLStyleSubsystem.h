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
 *   gl.Style.Variant A|P7|B|C          (P7.1 visual-direction variants, layered on every preset)
 * CANONICAL (operator, P7.1 review 2026-09-26): A, the dimensional, highly colourful WildStar-leaning
 * treatment with no environment outlines (characters and creatures lightly outlined, to be re-judged
 * with production characters). The others are kept only as the review record and are not shipping looks:
 * P7 = the superseded illustrated treatment (outlines everywhere, light banding); B = environment
 * silhouettes (rejected); C = grounded (rejected). Environment outlines are not reintroduced without
 * operator visual review.
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
	FName Variant = TEXT("A"); // P7.1 canonical
	bool bPost = true;
};
