#include "Pehlichi/GLCompanionPositioningComponent.h"

#include "GameFramework/Actor.h"

UGLCompanionPositioningComponent::UGLCompanionPositioningComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGLCompanionPositioningComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Advance(DeltaTime);
}

bool UGLCompanionPositioningComponent::HasArrived() const
{
	const AActor* Owner = GetOwner();
	return Owner && Mode == EGLCompanionMode::MoveTo && FVector::Dist2D(Owner->GetActorLocation(), Destination) <= ArriveDistance;
}

void UGLCompanionPositioningComponent::Advance(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || Mode == EGLCompanionMode::Stay)
	{
		return;
	}
	FVector Goal = Destination;
	float StopAt = ArriveDistance * 0.5f;
	if (Mode == EGLCompanionMode::Follow)
	{
		if (!FollowTarget.IsValid())
		{
			return;
		}
		Goal = FollowTarget->GetActorLocation();
		StopAt = FollowDistance;
	}
	const FVector Here = Owner->GetActorLocation();
	const FVector Flat(Goal.X - Here.X, Goal.Y - Here.Y, 0.0);
	const double Distance = Flat.Size();
	if (Distance <= StopAt)
	{
		return;
	}
	const double Step = FMath::Min<double>(Speed * DeltaTime, Distance - StopAt);
	Owner->SetActorLocation(Here + Flat.GetSafeNormal() * Step);
	Owner->SetActorRotation(Flat.Rotation());
}
