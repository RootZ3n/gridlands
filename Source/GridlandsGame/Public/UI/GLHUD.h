#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GLHUD.generated.h"

/**
 * Placeholder HUD: digital static proportional to the interference where Zenny stands (the soft
 * barrier made visible), plus a debug readout. Drawn in C++ with no assets; the finished static
 * shader and UI come later.
 */
UCLASS()
class GRIDLANDSGAME_API AGLHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	/** NICE and Pehlichi, bottom centre (P4). */
	void DrawSubtitles();
	/** What E will do to the thing in reach (P4); nothing when nothing is in reach. */
	void DrawInteractionPrompt(const APawn* Zenny);
	/** P12 build mode (ADR-0040): the build bar, the reason, the browser, finish / remove panels, snap and claim markers. */
	void DrawBuildMode(const APawn* Zenny);
	void DrawWorldMarkers(const struct FGLBuildView& View, float Scale);
	void Line(const FString& Text, const FLinearColor& Colour, float X, float& Y, float Scale);

public:

	/** Show "Grid: Hazy (0.31) | NICE composure 100%" top-left. */
	UPROPERTY(EditAnywhere, Category = "Gridlands") bool bShowReadout = true;

private:
	FRandomStream Noise{ 1234 };
};
