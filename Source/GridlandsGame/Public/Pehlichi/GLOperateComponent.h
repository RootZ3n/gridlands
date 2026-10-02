#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLOperateComponent.generated.h"

/**
 * P9 (ADR-0037): Pehlichi operates a mechanism's control. He goes there (his positioning ignores navigation, so
 * he reaches controls Zenny cannot), works it for the mechanism's operate seconds, then switches it: the
 * mechanism decides its effects at that moment. He never deals damage (ADR-0017): operating is an interaction;
 * whatever the mechanism does is the environment's outcome. Progress is not persisted: an interrupted or
 * reloaded operation must be commanded again (a reload never replays one).
 */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLOperateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLOperateComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Assign(FName Mechanism, AActor* Commander);
	void Stop();
	/** Steps the operation (TickComponent calls it; tests call it directly). */
	void Advance(float DeltaTime);

	bool IsWorking() const { return !Target.IsNone(); }
	FName GetTarget() const { return Target; }
	double GetProgressSeconds() const { return Progress; }

private:
	FName Target;
	TWeakObjectPtr<AActor> Commander;
	double Progress = 0.0;
};
