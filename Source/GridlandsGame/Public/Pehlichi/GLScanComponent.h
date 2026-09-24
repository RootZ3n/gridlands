#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLScanComponent.generated.h"

class AGLGlitch;

/** One thing a scan found. Kinds beyond Glitch (Trace, Node, WeakPoint, Decoy) arrive later. */
struct GRIDLANDSGAME_API FGLScanFinding
{
	FName Kind;
	FName SubjectId;
	FVector Location = FVector::ZeroVector;
	/** 0..1; interference will lower it (M8). */
	double Confidence = 1.0;
};

struct GRIDLANDSGAME_API FGLScanResult
{
	TArray<FGLScanFinding> Findings;
	int32 NewlyRevealed = 0;
};

/**
 * Pehlichi's scan: the only holder of scan authority (ADR-0005). Reveals Latent glitches within
 * its capability's range and strength (GLGlitchRules::CanDetect). Emits Event.Pehlichi.Scanned.
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLScanComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	FGLScanResult Scan();

	/** The capability that powers scanning. */
	UPROPERTY(EditAnywhere, Category = "Gridlands") FName ScanCapability = TEXT("capability.pehlichi.scan");
};
