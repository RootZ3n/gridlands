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

public:

	/** Show "Grid: Hazy (0.31) | NICE composure 100%" top-left. */
	UPROPERTY(EditAnywhere, Category = "Gridlands") bool bShowReadout = true;

private:
	FRandomStream Noise{ 1234 };
};
