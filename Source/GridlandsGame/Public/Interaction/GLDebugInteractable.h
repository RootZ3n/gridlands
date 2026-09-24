#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/GLInteractable.h"
#include "GLDebugInteractable.generated.h"

class UStaticMeshComponent;

/** A placeholder interactable used by the blockout and tests: counts how often it was used. */
UCLASS()
class GRIDLANDSGAME_API AGLDebugInteractable : public AActor, public IGLInteractable
{
	GENERATED_BODY()

public:
	AGLDebugInteractable();

	virtual void GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const override;
	virtual bool Interact(AActor* Interactor, FGameplayTag Verb) override;

	UPROPERTY(EditAnywhere, Category = "Debug") bool bEnabled = true;
	UPROPERTY(VisibleAnywhere, Category = "Debug") int32 TimesUsed = 0;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
};
