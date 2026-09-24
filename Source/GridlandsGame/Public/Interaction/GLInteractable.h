#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "GLInteractable.generated.h"

/** One thing the player could do to an interactable, e.g. Interact.Salvage. */
USTRUCT(BlueprintType)
struct GRIDLANDSGAME_API FGLInteractionOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) FGameplayTag Verb;
	UPROPERTY(BlueprintReadWrite) FText Label;
	UPROPERTY(BlueprintReadWrite) bool bEnabled = true;
	/** Why the option is disabled (shown to the player), e.g. "Needs a pry tool". */
	UPROPERTY(BlueprintReadWrite) FText DisabledReason;
};

UINTERFACE(MinimalAPI)
class UGLInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by an actor or by one of its components. Glitches deliberately do NOT
 * implement this: the player never repairs a glitch (ADR-0005).
 */
class GRIDLANDSGAME_API IGLInteractable
{
	GENERATED_BODY()

public:
	virtual void GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const = 0;
	/** Performs Verb. Returns false if it is not currently possible. */
	virtual bool Interact(AActor* Interactor, FGameplayTag Verb) = 0;
};
