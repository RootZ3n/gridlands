#include "Character/GLFootstepsComponent.h"

#include "Building/GLBuildPiece.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Noise/GLNoiseSubsystem.h"
#include "Structure/GLStructureSubsystem.h"

UGLFootstepsComponent::UGLFootstepsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

FName UGLFootstepsComponent::FloorMaterial() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || !Movement->CurrentFloor.IsWalkableFloor())
	{
		return NAME_None;
	}
	const FHitResult& Floor = Movement->CurrentFloor.HitResult;
	FName PieceDef;
	if (const AGLBuildPiece* Piece = Cast<AGLBuildPiece>(Floor.GetActor()))
	{
		PieceDef = Piece->GetPiece().Def; // an authored part, or a player piece with its own actor
	}
	else if (const UGLStructureSubsystem* Structures = GetWorld()->GetSubsystem<UGLStructureSubsystem>())
	{
		const FGLStructurePartRuntime* Part = Structures->FindPlayerPiece(Structures->PlayerPieceAt(Floor)); // an instanced player floor
		PieceDef = Part ? Part->Piece.Def : NAME_None;
	}
	const FGLBuildPieceDef* Def = PieceDef.IsNone() ? nullptr : GLContent::Get().Find<FGLBuildPieceDef>(PieceDef);
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
