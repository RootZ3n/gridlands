#include "Events/GLEventSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

void UGLEventSubsystem::Emit(const UObject* WorldContext, FGLGameplayEvent Event)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (UGLEventSubsystem* Bus = World ? World->GetSubsystem<UGLEventSubsystem>() : nullptr)
	{
		Bus->Broadcast(MoveTemp(Event));
	}
}

FDelegateHandle UGLEventSubsystem::Subscribe(FGameplayTag Filter, FGLGameplayEventDelegate Delegate)
{
	FSubscription& Subscription = Subscriptions.AddDefaulted_GetRef();
	Subscription.Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
	Subscription.Filter = Filter;
	Subscription.Delegate = MoveTemp(Delegate);
	return Subscription.Handle;
}

void UGLEventSubsystem::Unsubscribe(FDelegateHandle Handle)
{
	Subscriptions.RemoveAll([Handle](const FSubscription& S) { return S.Handle == Handle; });
}

void UGLEventSubsystem::Broadcast(FGLGameplayEvent Event)
{
	Event.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Recent.Add(Event);
	if (Recent.Num() > MaxRecentEvents)
	{
		Recent.RemoveAt(0, Recent.Num() - MaxRecentEvents);
	}
	// Copy: a listener may subscribe or unsubscribe while being notified.
	const TArray<FSubscription> Snapshot = Subscriptions;
	for (const FSubscription& Subscription : Snapshot)
	{
		if (Event.Tag.MatchesTag(Subscription.Filter))
		{
			Subscription.Delegate.ExecuteIfBound(Event);
		}
	}
}
