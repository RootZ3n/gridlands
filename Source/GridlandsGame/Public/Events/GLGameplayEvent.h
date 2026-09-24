#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GLGameplayEvent.generated.h"

/**
 * A gameplay event (ADR-0015). Systems emit these and never call listeners such as the
 * dialogue director directly. Tags live under Event.* (Config/Tags/Gridlands.ini).
 */
USTRUCT(BlueprintType)
struct GRIDLANDSGAME_API FGLGameplayEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) FGameplayTag Tag;
	/** Content id or anchor id the event is about, e.g. "salvage.house.wiring_run". */
	UPROPERTY(BlueprintReadWrite) FName Subject;
	/** Who caused it; None for the world. */
	UPROPERTY(BlueprintReadWrite) TWeakObjectPtr<AActor> Instigator;
	/** Small numeric payload, e.g. {"count": 5}. */
	UPROPERTY(BlueprintReadWrite) TMap<FName, double> Numbers;
	/** World time in seconds, stamped by the event subsystem. */
	UPROPERTY(BlueprintReadOnly) double Time = 0.0;
};
