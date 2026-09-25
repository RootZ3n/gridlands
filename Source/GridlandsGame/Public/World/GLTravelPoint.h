#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/GLInteractable.h"
#include "GLTravelPoint.generated.h"

/**
 * A way into or out of an authored space (M11: the storm drain; ADR-0022 keeps sewers and caves as
 * authored geometry, not terrain). Interact moves Zenny, and Pehlichi with him, to the destination
 * and emits Event.Area.Entered with AreaName.
 */
UCLASS()
class GRIDLANDSGAME_API AGLTravelPoint : public AActor, public IGLInteractable
{
	GENERATED_BODY()

public:
	AGLTravelPoint();

	virtual void GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const override;
	virtual bool Interact(AActor* Interactor, FGameplayTag Verb) override;

	UPROPERTY(EditAnywhere, Category = "Travel") FVector Destination = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Travel") float DestinationYaw = 0.f;
	UPROPERTY(EditAnywhere, Category = "Travel") FText Label;
	UPROPERTY(EditAnywhere, Category = "Travel") FName AreaName;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Marker;
};
