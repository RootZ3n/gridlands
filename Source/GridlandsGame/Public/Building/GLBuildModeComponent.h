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
	/** Rotates the selection a quarter turn (Z). */
	void Rotate();
	/** P11: rotates the selection by the fine step, 15 degrees (C). Angled snaps take their yaw from data instead. */
	void RotateFine();
	/** Place (build mode) or apply the terraform stroke. */
	void Primary();
	/** Careful dismantle of the aimed piece (build mode, X). What would fall is highlighted before (removal preview). */
	void Demolish();
	/** P11: destructive smash of the aimed piece (N). */
	void Smash();
	/** P11: installs the next finish this piece accepts and Zenny can pay for (Y). */
	void InstallFinish();

	EGLToolMode GetMode() const { return Mode; }
	FName GetSelectedPiece() const;
	/** One line for the HUD: mode, selection, and why a placement would be refused. */
	FString StatusLine() const { return Status; }

	UPROPERTY(EditAnywhere, Category = "Building") float ReachCm = 800.f;

private:
	bool Aim(FHitResult& OutHit) const;
	void UpdatePreview();
	void ClearGhost();
	void UpdateRemovalPreview(const FHitResult* Hit);

	EGLToolMode Mode = EGLToolMode::None;
	TArray<FName> Pieces;
	int32 Selected = 0;
	int32 YawStep = 0;
	/** P11: the pieces highlighted by the removal preview. */
	TArray<int32> Highlighted;
	bool bHasCandidate = false;
	FGLPlacedPiece Candidate;
	FString Status;
	UPROPERTY(Transient) TObjectPtr<AGLBuildPiece> Ghost;
};
