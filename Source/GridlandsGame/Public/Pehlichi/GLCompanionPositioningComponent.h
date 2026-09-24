#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLCompanionPositioningComponent.generated.h"

UENUM()
enum class EGLCompanionMode : uint8
{
	Follow,
	Stay,
	MoveTo,
};

/**
 * Follow / stay / move-to for a companion. v0 moves kinematically in a straight line (no navmesh
 * yet; ADR-0022 lists navigation as open). Reachability is a straight-line check for now; a later
 * IGLReachability replaces it without touching the repair logic.
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLCompanionPositioningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLCompanionPositioningComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Follow(AActor* Target) { Mode = EGLCompanionMode::Follow; FollowTarget = Target; }
	void Stay() { Mode = EGLCompanionMode::Stay; }
	void MoveTo(const FVector& Location) { Mode = EGLCompanionMode::MoveTo; Destination = Location; }
	EGLCompanionMode GetMode() const { return Mode; }

	/** Steps the movement by DeltaTime (TickComponent calls this; tests call it directly). */
	void Advance(float DeltaTime);
	bool HasArrived() const;

	UPROPERTY(EditAnywhere, Category = "Gridlands") float Speed = 500.f;
	UPROPERTY(EditAnywhere, Category = "Gridlands") float FollowDistance = 250.f;
	UPROPERTY(EditAnywhere, Category = "Gridlands") float ArriveDistance = 120.f;

private:
	EGLCompanionMode Mode = EGLCompanionMode::Stay;
	TWeakObjectPtr<AActor> FollowTarget;
	FVector Destination = FVector::ZeroVector;
};
