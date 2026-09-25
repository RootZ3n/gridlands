#pragma once

#include "Building/GLStructureRules.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLBuildModeComponent.generated.h"

class AGLBuildPiece;

UENUM()
enum class EGLToolMode : uint8
{
	None,
	Build,
	Dig,
	Raise,
	Flatten,
};

/**
 * Zenny's building and terraforming hands (M10). Aims from the camera, previews a ghost piece,
 * and forwards to the building and terrain subsystems, which own every rule and transaction.
 * Bindings and feel are provisional pending operator playtest.
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLBuildModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLBuildModeComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	void ToggleBuild();
	void CycleTerraform();
	void CyclePiece(int32 Direction);
	void Rotate();
	/** Place (build mode) or apply the terraform stroke. */
	void Primary();
	/** Demolish the aimed piece (build mode). */
	void Demolish();

	EGLToolMode GetMode() const { return Mode; }
	FName GetSelectedPiece() const;
	/** One line for the HUD: mode, selection, and why a placement would be refused. */
	FString StatusLine() const { return Status; }

	UPROPERTY(EditAnywhere, Category = "Building") float ReachCm = 800.f;

private:
	bool Aim(FHitResult& OutHit) const;
	void UpdatePreview();
	void ClearGhost();

	EGLToolMode Mode = EGLToolMode::None;
	TArray<FName> Pieces;
	int32 Selected = 0;
	int32 YawQuarter = 0;
	bool bHasCandidate = false;
	FGLPlacedPiece Candidate;
	FString Status;
	UPROPERTY(Transient) TObjectPtr<AGLBuildPiece> Ghost;
};
