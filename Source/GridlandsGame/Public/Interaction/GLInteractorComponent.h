#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Interaction/GLInteractable.h"
#include "GLInteractorComponent.generated.h"

/**
 * Finds what the owner is looking at (a trace from the view point) and performs interactions.
 * Emits Event.Interaction.Performed on success.
 */
UCLASS(ClassGroup = (Gridlands), meta = (BlueprintSpawnableComponent))
class GRIDLANDSGAME_API UGLInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLInteractorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Traces from ViewLocation along ViewDirection and updates the focus. Returns the new focus. */
	UObject* UpdateFocus(const FVector& ViewLocation, const FVector& ViewDirection);

	/** The focused interactable (actor or component), or null. */
	UObject* GetFocus() const { return Focus.Get(); }
	void GetFocusOptions(TArray<FGLInteractionOption>& OutOptions) const;

	/** Performs Verb on the focus; with an invalid Verb, performs the first enabled option. */
	bool TryInteract(FGameplayTag Verb = FGameplayTag());

	/** Interaction reach in cm. */
	UPROPERTY(EditAnywhere, Category = "Interaction") float Reach = 300.f;

private:
	/** The interactable implementing IGLInteractable on HitActor: the actor itself, else its first such component. */
	static UObject* FindInteractable(AActor* HitActor);

	TWeakObjectPtr<UObject> Focus;
};
