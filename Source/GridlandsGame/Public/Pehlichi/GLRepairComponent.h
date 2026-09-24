#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLRepairComponent.generated.h"

class AGLGlitch;

/**
 * Pehlichi's repair system: the ONLY holder of repair authority (ADR-0005, invariant P-1).
 * Goes to the glitch, takes any ItemDelivered items from the commander, repairs over time,
 * and on completion applies the glitch's rewards. Deals no damage of any kind (ADR-0017).
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLRepairComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLRepairComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Starts (or resumes) work on Glitch; Commander is whose items are handed over and who receives item rewards. */
	void AssignTarget(AGLGlitch* Glitch, AActor* Commander);
	/** Stops working. An active repair becomes Interrupted (progress per the glitch's policy). */
	void Stop();
	/** Steps the repair by DeltaTime (TickComponent calls this; tests call it directly). */
	void Advance(float DeltaTime);

	AGLGlitch* GetTarget() const { return Target.Get(); }
	bool IsWorking() const { return Target.IsValid(); }

	/** How close Pehlichi must be to the repair point. */
	UPROPERTY(EditAnywhere, Category = "Gridlands") float RepairReach = 150.f;

private:
	bool TakeDeliveredItems(class UGLGlitchComponent& Glitch);
	void ApplyRewards(const class UGLGlitchComponent& Glitch);

	TWeakObjectPtr<AGLGlitch> Target;
	TWeakObjectPtr<AActor> Commander;
};
