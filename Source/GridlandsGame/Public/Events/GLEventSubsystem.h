#pragma once

#include "CoreMinimal.h"
#include "Events/GLGameplayEvent.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLEventSubsystem.generated.h"

DECLARE_DELEGATE_OneParam(FGLGameplayEventDelegate, const FGLGameplayEvent&);

/**
 * Per-world gameplay event bus. A subscription to a tag also receives its children:
 * subscribing to Event.Salvage receives Event.Salvage.WireStripped.
 */
UCLASS()
class GRIDLANDSGAME_API UGLEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Emits Event through the world's bus. Safe to call with a null or world-less context (does nothing). */
	static void Emit(const UObject* WorldContext, FGLGameplayEvent Event);

	FDelegateHandle Subscribe(FGameplayTag Filter, FGLGameplayEventDelegate Delegate);
	void Unsubscribe(FDelegateHandle Handle);
	void Broadcast(FGLGameplayEvent Event);

	/** The most recent events, oldest first (bounded). Listeners use it for history-aware decisions. */
	const TArray<FGLGameplayEvent>& GetRecentEvents() const { return Recent; }

	static constexpr int32 MaxRecentEvents = 64;

private:
	struct FSubscription
	{
		FDelegateHandle Handle;
		FGameplayTag Filter;
		FGLGameplayEventDelegate Delegate;
	};

	TArray<FSubscription> Subscriptions;
	TArray<FGLGameplayEvent> Recent;
};
