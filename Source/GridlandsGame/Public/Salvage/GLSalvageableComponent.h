#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Interaction/GLInteractable.h"
#include "GLSalvageableComponent.generated.h"

/**
 * Makes its owner salvageable by a definition (salvage.*). Each Interact.Salvage is one hit
 * (GLSalvageRules); when integrity runs out, yields go to the interactor's inventory scaled by
 * the world's settings (ADR-0016), events fire, and the owner (and any linked visual actor) is removed.
 */
UCLASS(ClassGroup = (Gridlands), meta = (BlueprintSpawnableComponent))
class GRIDLANDSGAME_API UGLSalvageableComponent : public UActorComponent, public IGLInteractable
{
	GENERATED_BODY()

public:
	/** Initializes from a salvage definition; returns false if SalvageId is not one. */
	bool Setup(FName InSalvageId, AActor* InLinkedVisual = nullptr);

	virtual void GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const override;
	virtual bool Interact(AActor* Interactor, FGameplayTag Verb) override;

	FName GetSalvageId() const { return SalvageId; }
	double GetIntegrity() const { return Integrity; }
	bool IsSalvaged() const { return bSalvaged; }

private:
	void Complete(AActor* Interactor);

	UPROPERTY(VisibleAnywhere) FName SalvageId;
	UPROPERTY(VisibleAnywhere) double Integrity = 0.0;
	UPROPERTY(VisibleAnywhere) bool bSalvaged = false;
	/** The anchored visual actor this salvage stands for (a fence, a wall); hidden when salvaged. */
	TWeakObjectPtr<AActor> LinkedVisual;
};
