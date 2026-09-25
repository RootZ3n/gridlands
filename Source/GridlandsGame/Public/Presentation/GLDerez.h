#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLDerez.generated.h"

/**
 * The digital dissolve (P4): a thing leaving the simulation flickers, glitches thin and sheds
 * pixels, then is gone. Presentation only: gameplay state changes first (a defeated creature is
 * defeated at once); this just shows it. Reused by creatures and Glitch Storm artifacts.
 */
namespace GLDerez
{
	/** Applies the look for Alpha in 1 (whole) .. 0 (gone) to Target's primitives. */
	GRIDLANDSGAME_API void Apply(AActor* Target, float Alpha, FRandomStream& Random, const FVector& BaseScale);
}

UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLDerezComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLDerezComponent();
	/** Starts the dissolve; the owner is hidden when it ends. */
	void Start(float Seconds = 1.2f);
	/** Advances it (Tick calls it; tests call it directly). Returns true while still dissolving. */
	bool Advance(float DeltaSeconds);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool IsRunning() const { return Left > 0.f; }

private:
	void Shed();

	float Duration = 1.2f;
	float Left = 0.f;
	float ShedTimer = 0.f;
	FVector BaseScale = FVector::OneVector;
	FRandomStream Random{ 0x0DE2E2 };
	UPROPERTY() TArray<TObjectPtr<AActor>> Pixels;
};
