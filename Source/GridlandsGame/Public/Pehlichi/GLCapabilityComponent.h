#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GLCapabilityComponent.generated.h"

/** Pehlichi's permanent capability levels (capability.* ids). World-save-bound (ADR-0019). */
UCLASS(ClassGroup = (Gridlands))
class GRIDLANDSGAME_API UGLCapabilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	int32 Level(FName CapabilityId) const { return Levels.FindRef(CapabilityId); }
	/** Raises a capability by Delta, clamped to its highest defined level. Returns the gain. Emits Event.Pehlichi.CapabilityRaised. */
	int32 Raise(FName CapabilityId, int32 Delta);
	/** Sets a starting level without announcing it. */
	void Grant(FName CapabilityId, int32 InLevel) { Levels.Add(CapabilityId, InLevel); }

private:
	UPROPERTY(VisibleAnywhere) TMap<FName, int32> Levels;
};
