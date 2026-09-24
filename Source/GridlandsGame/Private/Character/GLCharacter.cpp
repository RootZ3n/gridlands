#include "Character/GLCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Fabrication/GLFabricatorComponent.h"
#include "GridlandsGame.h"
#include "Interaction/GLInteractorComponent.h"
#include "Inventory/GLInventoryComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace GLCharacterInput
{
	const FName Move(TEXT("Move"));
	const FName Look(TEXT("Look"));
	const FName Jump(TEXT("Jump"));
	const FName Interact(TEXT("Interact"));
	const FName Fabricate(TEXT("Fabricate"));
}

AGLCharacter::AGLCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(40.f, 90.f);

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 450.f;
	GetCharacterMovement()->JumpZVelocity = 520.f;

	// Placeholder body until Zenny's model exists.
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.8f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->SocketOffset = FVector(0.f, 60.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	Interactor = CreateDefaultSubobject<UGLInteractorComponent>(TEXT("Interactor"));
	Inventory = CreateDefaultSubobject<UGLInventoryComponent>(TEXT("Inventory"));
	Fabricator = CreateDefaultSubobject<UGLFabricatorComponent>(TEXT("Fabricator"));
}

void AGLCharacter::BuildInput()
{
	if (MappingContext)
	{
		return;
	}
	auto MakeAction = [this](FName Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		Actions.Add(Name, Action);
		return Action;
	};
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("GridlandsDefaultInput"));

	// Move: WASD as a 2D axis. W/S drive Y (forward), A/D drive X (right).
	UInputAction* Move = MakeAction(GLCharacterInput::Move, EInputActionValueType::Axis2D);
	{
		FEnhancedActionKeyMapping& Forward = MappingContext->MapKey(Move, EKeys::W);
		Forward.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		FEnhancedActionKeyMapping& Back = MappingContext->MapKey(Move, EKeys::S);
		Back.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		Back.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		MappingContext->MapKey(Move, EKeys::D);
		FEnhancedActionKeyMapping& Left = MappingContext->MapKey(Move, EKeys::A);
		Left.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		MappingContext->MapKey(Move, EKeys::Gamepad_Left2D);
	}
	// Look: mouse (Y inverted to feel natural) and right stick.
	UInputAction* Look = MakeAction(GLCharacterInput::Look, EInputActionValueType::Axis2D);
	{
		FEnhancedActionKeyMapping& Mouse = MappingContext->MapKey(Look, EKeys::Mouse2D);
		UInputModifierNegate* InvertY = NewObject<UInputModifierNegate>(this);
		InvertY->bX = false;
		InvertY->bZ = false;
		Mouse.Modifiers.Add(InvertY);
		MappingContext->MapKey(Look, EKeys::Gamepad_Right2D);
	}
	UInputAction* Jump = MakeAction(GLCharacterInput::Jump, EInputActionValueType::Boolean);
	MappingContext->MapKey(Jump, EKeys::SpaceBar);
	MappingContext->MapKey(Jump, EKeys::Gamepad_FaceButton_Bottom);

	UInputAction* Use = MakeAction(GLCharacterInput::Interact, EInputActionValueType::Boolean);
	MappingContext->MapKey(Use, EKeys::E);
	MappingContext->MapKey(Use, EKeys::Gamepad_FaceButton_Left);

	UInputAction* Make = MakeAction(GLCharacterInput::Fabricate, EInputActionValueType::Boolean);
	MappingContext->MapKey(Make, EKeys::F);
}

const UInputAction* AGLCharacter::FindInputAction(FName Name) const
{
	const TObjectPtr<UInputAction>* Found = Actions.Find(Name);
	return Found ? Found->Get() : nullptr;
}

void AGLCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	BuildInput();
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Input->AddMappingContext(MappingContext, 0);
		}
	}
}

void AGLCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(FindInputAction(GLCharacterInput::Move), ETriggerEvent::Triggered, this, &AGLCharacter::Move);
		Input->BindAction(FindInputAction(GLCharacterInput::Look), ETriggerEvent::Triggered, this, &AGLCharacter::Look);
		Input->BindAction(FindInputAction(GLCharacterInput::Jump), ETriggerEvent::Started, this, &ACharacter::Jump);
		Input->BindAction(FindInputAction(GLCharacterInput::Jump), ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		Input->BindAction(FindInputAction(GLCharacterInput::Interact), ETriggerEvent::Started, this, &AGLCharacter::Interact);
		Input->BindAction(FindInputAction(GLCharacterInput::Fabricate), ETriggerEvent::Started, this, &AGLCharacter::FabricateFirstAvailable);
	}
}

void AGLCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void AGLCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AGLCharacter::Interact()
{
	Interactor->TryInteract();
}

void AGLCharacter::FabricateFirstAvailable()
{
	const TArray<FName> Recipes = Fabricator->CraftableRecipes();
	if (Recipes.Num() > 0)
	{
		Fabricator->Fabricate(Recipes[0]);
		UE_LOG(LogGridlands, Log, TEXT("Fabricated %s"), *Recipes[0].ToString());
	}
}

void AGLCharacter::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	// Interaction aims where the camera looks, starting at the character so it cannot hit things behind Zenny.
	OutRotation = Camera ? Camera->GetComponentRotation() : GetActorRotation();
	OutLocation = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
}
