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
#include "Pehlichi/GLPehlichi.h"
#include "Puzzle/GLPuzzleSubsystem.h"
#include "Building/GLBuildModeComponent.h"
#include "Combat/GLCombatComponent.h"
#include "Combat/GLHealthComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
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
	const FName Scan(TEXT("CommandScan"));
	const FName Repair(TEXT("CommandRepair"));
	const FName Follow(TEXT("CommandFollowToggle"));
	const FName QuickSave(TEXT("QuickSave"));
	const FName Hint(TEXT("AskPehlichiForHint"));
	// M10 building and terraforming (provisional bindings, pending operator playtest).
	const FName BuildToggle(TEXT("BuildToggle"));
	const FName TerraformCycle(TEXT("TerraformCycle"));
	const FName ToolPrimary(TEXT("ToolPrimary"));
	const FName PieceNext(TEXT("PieceNext"));
	const FName PiecePrevious(TEXT("PiecePrevious"));
	const FName PieceRotate(TEXT("PieceRotate"));
	const FName PieceDemolish(TEXT("PieceDemolish"));
	const FName Distract(TEXT("PehlichiDistract"));
	const FName QuickLoad(TEXT("QuickLoad"));
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
	BuildMode = CreateDefaultSubobject<UGLBuildModeComponent>(TEXT("BuildMode"));
	Health = CreateDefaultSubobject<UGLHealthComponent>(TEXT("Health"));
	Combat = CreateDefaultSubobject<UGLCombatComponent>(TEXT("Combat"));
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

	MappingContext->MapKey(MakeAction(GLCharacterInput::Scan, EInputActionValueType::Boolean), EKeys::Q);
	MappingContext->MapKey(MakeAction(GLCharacterInput::Repair, EInputActionValueType::Boolean), EKeys::R);
	MappingContext->MapKey(MakeAction(GLCharacterInput::Follow, EInputActionValueType::Boolean), EKeys::G);
	MappingContext->MapKey(MakeAction(GLCharacterInput::QuickSave, EInputActionValueType::Boolean), EKeys::F5);
	MappingContext->MapKey(MakeAction(GLCharacterInput::Hint, EInputActionValueType::Boolean), EKeys::H);
	MappingContext->MapKey(MakeAction(GLCharacterInput::BuildToggle, EInputActionValueType::Boolean), EKeys::B);
	MappingContext->MapKey(MakeAction(GLCharacterInput::TerraformCycle, EInputActionValueType::Boolean), EKeys::T);
	MappingContext->MapKey(MakeAction(GLCharacterInput::ToolPrimary, EInputActionValueType::Boolean), EKeys::LeftMouseButton);
	MappingContext->MapKey(MakeAction(GLCharacterInput::PieceNext, EInputActionValueType::Boolean), EKeys::MouseScrollUp);
	MappingContext->MapKey(MakeAction(GLCharacterInput::PiecePrevious, EInputActionValueType::Boolean), EKeys::MouseScrollDown);
	MappingContext->MapKey(MakeAction(GLCharacterInput::PieceRotate, EInputActionValueType::Boolean), EKeys::Z);
	MappingContext->MapKey(MakeAction(GLCharacterInput::PieceDemolish, EInputActionValueType::Boolean), EKeys::X);
	MappingContext->MapKey(MakeAction(GLCharacterInput::Distract, EInputActionValueType::Boolean), EKeys::V);
	MappingContext->MapKey(MakeAction(GLCharacterInput::QuickLoad, EInputActionValueType::Boolean), EKeys::F9);
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
		Input->BindAction(FindInputAction(GLCharacterInput::Scan), ETriggerEvent::Started, this, &AGLCharacter::CommandPehlichi, FName(TEXT("Command.Pehlichi.Scan")));
		Input->BindAction(FindInputAction(GLCharacterInput::Repair), ETriggerEvent::Started, this, &AGLCharacter::CommandPehlichi, FName(TEXT("Command.Pehlichi.Repair")));
		Input->BindAction(FindInputAction(GLCharacterInput::Follow), ETriggerEvent::Started, this, &AGLCharacter::ToggleFollow);
		Input->BindAction(FindInputAction(GLCharacterInput::QuickSave), ETriggerEvent::Started, this, &AGLCharacter::QuickSave);
		Input->BindAction(FindInputAction(GLCharacterInput::Hint), ETriggerEvent::Started, this, &AGLCharacter::AskForHint);
		Input->BindAction(FindInputAction(GLCharacterInput::BuildToggle), ETriggerEvent::Started, BuildMode.Get(), &UGLBuildModeComponent::ToggleBuild);
		Input->BindAction(FindInputAction(GLCharacterInput::TerraformCycle), ETriggerEvent::Started, BuildMode.Get(), &UGLBuildModeComponent::CycleTerraform);
		Input->BindAction(FindInputAction(GLCharacterInput::ToolPrimary), ETriggerEvent::Started, this, &AGLCharacter::PrimaryAction);
		Input->BindAction(FindInputAction(GLCharacterInput::Distract), ETriggerEvent::Started, this, &AGLCharacter::DistractCommand);
		Input->BindAction(FindInputAction(GLCharacterInput::PieceNext), ETriggerEvent::Started, this, &AGLCharacter::NextPiece);
		Input->BindAction(FindInputAction(GLCharacterInput::PiecePrevious), ETriggerEvent::Started, this, &AGLCharacter::PreviousPiece);
		Input->BindAction(FindInputAction(GLCharacterInput::PieceRotate), ETriggerEvent::Started, BuildMode.Get(), &UGLBuildModeComponent::Rotate);
		Input->BindAction(FindInputAction(GLCharacterInput::PieceDemolish), ETriggerEvent::Started, BuildMode.Get(), &UGLBuildModeComponent::Demolish);
		Input->BindAction(FindInputAction(GLCharacterInput::QuickLoad), ETriggerEvent::Started, this, &AGLCharacter::QuickLoad);
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
	const FName Recipe = Fabricator->PreferredRecipe();
	if (!Recipe.IsNone())
	{
		Fabricator->Fabricate(Recipe);
		UE_LOG(LogGridlands, Log, TEXT("Fabricated %s"), *Recipe.ToString());
	}
}

void AGLCharacter::CommandPehlichi(FName Command)
{
	if (AGLPehlichi* Companion = Pehlichi.Get())
	{
		const EGLCommandRejection Result = Companion->GetCommands()->Issue(Command, this);
		UE_LOG(LogGridlands, Log, TEXT("Command %s -> %s"), *Command.ToString(), *StaticEnum<EGLCommandRejection>()->GetNameStringByValue(static_cast<int64>(Result)));
	}
}

void AGLCharacter::PrimaryAction()
{
	if (BuildMode->GetMode() != EGLToolMode::None)
	{
		BuildMode->Primary();
	}
	else
	{
		Combat->Attack();
	}
}

void AGLCharacter::DistractCommand()
{
	CommandPehlichi(TEXT("Command.Pehlichi.Distract"));
}

void AGLCharacter::NextPiece()
{
	BuildMode->CyclePiece(1);
}

void AGLCharacter::PreviousPiece()
{
	BuildMode->CyclePiece(-1);
}

void AGLCharacter::AskForHint()
{
	if (UGLPuzzleSubsystem* Puzzles = GetWorld()->GetSubsystem<UGLPuzzleSubsystem>())
	{
		Puzzles->RequestHint(Pehlichi.Get());
	}
}

void AGLCharacter::QuickSave()
{
	if (UGLSaveSubsystem* Saves = GetWorld()->GetSubsystem<UGLSaveSubsystem>())
	{
		Saves->SaveToSlot(Saves->AutosaveSlot);
	}
}

void AGLCharacter::QuickLoad()
{
	if (UGLSaveSubsystem* Saves = GetWorld()->GetSubsystem<UGLSaveSubsystem>())
	{
		Saves->LoadFromSlot(Saves->AutosaveSlot);
	}
}

void AGLCharacter::ToggleFollow()
{
	bPehlichiStaying = !bPehlichiStaying;
	CommandPehlichi(bPehlichiStaying ? FName(TEXT("Command.Pehlichi.Stay")) : FName(TEXT("Command.Pehlichi.Follow")));
}

void AGLCharacter::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	// Interaction aims where the camera looks, starting at the character so it cannot hit things behind Zenny.
	OutRotation = Camera ? Camera->GetComponentRotation() : GetActorRotation();
	OutLocation = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
}
