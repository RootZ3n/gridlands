#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/GLInteractable.h"
#include "GLPuzzleSite.generated.h"

class UStaticMeshComponent;

/**
 * Where Zenny answers a puzzle through gameplay (ADR-0023). v0 supports PRESENT: place the answer
 * item here. Spawned from a puzzle_site placement. The glitch itself stays non-interactable.
 */
UCLASS()
class GRIDLANDSGAME_API AGLPuzzleSite : public AActor, public IGLInteractable
{
	GENERATED_BODY()

public:
	AGLPuzzleSite();
	bool Setup(FName InPuzzleId);

	virtual void GetInteractionOptions(const AActor* Interactor, TArray<FGLInteractionOption>& OutOptions) const override;
	virtual bool Interact(AActor* Interactor, FGameplayTag Verb) override;

	FName GetPuzzleId() const { return PuzzleId; }

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) FName PuzzleId;
};
