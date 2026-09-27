#include "Character/GLFootstepsComponent.h"

#include "Building/GLBuildPiece.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Noise/GLNoiseSubsystem.h"

UGLFootstepsComponent::UGLFootstepsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

FName UGLFootstepsComponent::FloorMaterial() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const AGLBuildPiece* Piece = Movement && Movement->CurrentFloor.IsWalkableFloor() ? Cast<AGLBuildPiece>(Movement->CurrentFloor.HitResult.GetActor()) : nullptr;
	const FGLBuildPieceDef* Def = Piece ? GLContent::Get().Find<FGLBuildPieceDef>(Piece->GetPiece().Def) : nullptr;
	return Def ? Def->Material : NAME_None;
}

int32 UGLFootstepsComponent::EmitStep()
{
	return UGLNoiseSubsystem::EmitAction(this, TEXT("Noise.Move.Walk"), GetOwner()->GetActorLocation(), GetOwner(), FloorMaterial());
}

int32 UGLFootstepsComponent::EmitLanding()
{
	return UGLNoiseSubsystem::EmitAction(this, TEXT("Noise.Move.Land"), GetOwner()->GetActorLocation(), GetOwner(), FloorMaterial());
}

void UGLFootstepsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}
	if (Movement->IsFalling())
	{
		bWasFalling = true;
		FallSpeed = FMath::Max(FallSpeed, -Movement->Velocity.Z);
		return;
	}
	if (bWasFalling && Movement->IsMovingOnGround())
	{
		if (FallSpeed >= LandingSpeedCm)
		{
			EmitLanding();
		}
		bWasFalling = false;
		FallSpeed = 0.0;
	}
	if (Movement->IsMovingOnGround())
	{
		Walked += Movement->Velocity.Size2D() * DeltaTime;
		if (Walked >= StrideCm)
		{
			Walked = 0.0;
			EmitStep();
		}
	}
}
