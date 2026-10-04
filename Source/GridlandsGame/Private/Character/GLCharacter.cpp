#include "Character/GLCharacter.h"

#include "Presentation/GLVisuals.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "NavigationInvokerComponent.h"
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
#include "Character/GLFootstepsComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
#include "GridlandsGame.h"
#include "Interaction/GLInteractorComponent.h"
#include "Inventory/GLInventoryComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Playtest/GLFrictionLog.h"

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
	const FName PieceRotateFine(TEXT("PieceRotateFine"));
	const FName PieceSmash(TEXT("PieceSmash"));
	const FName PieceFinish(TEXT("PieceFinish"));
	const FName TakeAll(TEXT("TakeAll"));
	// P12 build mode (ADR-0040).
	const FName PrimaryRelease(TEXT("ToolPrimaryRelease"));
	const FName BuildBrowser(TEXT("BuildBrowser"));
	const FName BuildBack(TEXT("BuildBack"));
	const FName BuildCamera(TEXT("BuildCamera"));
	const FName Friction(TEXT("FrictionNote"));
	const FName Favorite[8] = { TEXT("Favorite1"), TEXT("Favorite2"), TEXT("Favorite3"), TEXT("Favorite4"), TEXT("Favorite5"), TEXT("Favorite6"), TEXT("Favorite7"), TEXT("Favorite8") };
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
	Footsteps = CreateDefaultSubobject<UGLFootstepsComponent>(TEXT("Footsteps")); // P9: movement is world noise
	Combat = CreateDefaultSubobject<UGLCombatComponent>(TEXT("Combat"));

	// Navigation exists around Zenny, not across the whole cell (ADR-0029): one chunk (64 m) out,
	// kept until 96 m so walking back and forth does not rebuild the same tiles.
	NavInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvoker"));
	NavInvoker->SetGenerationRadii(6400.f, 9600.f);
}

void AGLCharacter::BeginPlay()
{
	Super::BeginPlay();
	// P7 style proxy (visual.character.zenny) replaces the blockout body when it is imported.
	const float Half = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	if (GLVisuals::Attach(this, GetCapsuleComponent(), TEXT("visual.character.zenny"), FTransform(FVector(0, 0, -Half))))
	{
		Body->SetVisibility(false);
	}
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
	// P11 (provisional bindings): fine rotation (15 degrees), destructive smash, install a finish.
	MappingContext->MapKey(MakeAction(GLCharacterInput::PieceRotateFine, EInputActionValueType::Boolean), EKeys::C);
	MappingContext->MapKey(MakeAction(GLCharacterInput::PieceSmash, EInputActionValueType::Boolean), EKeys::N);
	MappingContext->MapKey(MakeAction(GLCharacterInput::PieceFinish, EInputActionValueType::Boolean), EKeys::Y);
	MappingContext->MapKey(MakeAction(GLCharacterInput::TakeAll, EInputActionValueType::Boolean), EKeys::L); // P11: take everything from a storage crate
	// P12 build mode: Tab browser, Esc / right mouse back, Alt build camera, 1-8 favorites (Ctrl: pin), F8 friction note.
	MappingContext->MapKey(MakeAction(GLCharacterInput::BuildBrowser, EInputActionValueType::Boolean), EKeys::Tab);
	UInputAction* BackAction = MakeAction(GLCharacterInput::BuildBack, EInputActionValueType::Boolean);
	MappingContext->MapKey(BackAction, EKeys::Escape);
	MappingContext->MapKey(BackAction, EKeys::RightMouseButton);
	MappingContext->MapKey(MakeAction(GLCharacterInput::BuildCamera, EInputActionValueType::Boolean), EKeys::LeftAlt);
	MappingContext->MapKey(MakeAction(GLCharacterInput::Friction, EInputActionValueType::Boolean), EKeys::F8);
	const FKey Digits[8] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight };
	for (int32 I = 0; I < 8; ++I)
	{
		MappingContext->MapKey(MakeAction(GLCharacterInput::Favorite[I], EInputActionValueType::Boolean), Digits[I]);
	}
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
		Input->BindAction(FindInputAction(GLCharacterInput::ToolPrimary), ETriggerEvent::Completed, BuildMode.Get(), &UGLBuildModeComponent::PrimaryReleased);
		Input->BindAction(FindInputAction(GLCharacterInput::BuildBrowser), ETriggerEvent::Started, BuildMode.Get(), &UGLBuildModeComponent::ToggleBrowser);
		Input->BindAction(FindInputAction(GLCharacterInput::BuildBack), ETriggerEvent::Started, this, &AGLCharacter::BuildBack);
		Input->BindAction(FindInputAction(GLCharacterInput::BuildCamera), ETriggerEvent::Started, this, &AGLCharacter::BuildCameraPressed);
		Input->BindAction(FindInputAction(GLCharacterInput::BuildCamera), ETriggerEvent::Completed, this, &AGLCharacter::BuildCameraReleased);
		Input->BindAction(FindInputAction(GLCharacterInput::Friction), ETriggerEvent::Started, this, &AGLCharacter::FrictionNote);
		for (int32 I = 0; I < 8; ++I)
		{
			Input->BindAction(FindInputAction(GLCharacterInput::Favorite[I]), ETriggerEvent::Started, this, &AGLCharacter::Favorite, I);
		}
		Input->BindAction(FindInputAction(GLCharacterInput::Distract), ETriggerEvent::Started, this, &AGLCharacter::DistractCommand);
		Input->BindAction(FindInputAction(GLCharacterInput::PieceNext), ETriggerEvent::Started, this, &AGLCharacter::NextPiece);
		Input->BindAction(FindInputAction(GLCharacterInput::PiecePrevious), ETriggerEvent::Started, this, &AGLCharacter::PreviousPiece);
		Input->BindAction(FindInputAction(GLCharacterInput::PieceRotate), ETriggerEvent::Started, this, &AGLCharacter::RotatePiece);
		Input->BindAction(FindInputAction(GLCharacterInput::PieceDemolish), ETriggerEvent::Started, BuildMode.Get(), &UGLBuildModeComponent::ToggleRemoveMode);
		Input->BindAction(FindInputAction(GLCharacterInput::PieceRotateFine), ETriggerEvent::Started, BuildMode.Get(), &UGLBuildModeComponent::Rotate15);
		Input->BindAction(FindInputAction(GLCharacterInput::PieceSmash), ETriggerEvent::Started, BuildMode.Get(), &UGLBuildModeComponent::TogglePath);
		Input->BindAction(FindInputAction(GLCharacterInput::PieceFinish), ETriggerEvent::Started, BuildMode.Get(), &UGLBuildModeComponent::ToggleFinishMode);
		Input->BindAction(FindInputAction(GLCharacterInput::TakeAll), ETriggerEvent::Started, this, &AGLCharacter::TakeAll);
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

void AGLCharacter::TakeAll()
{
	Interactor->TryInteract(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Take")));
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
	Wheel(1);
}

void AGLCharacter::PreviousPiece()
{
	Wheel(-1);
}

bool AGLCharacter::IsDown(const FKey& A, const FKey& B) const
{
	const APlayerController* PC = Cast<APlayerController>(Controller);
	return PC && (PC->IsInputKeyDown(A) || PC->IsInputKeyDown(B));
}

void AGLCharacter::Wheel(int32 Direction)
{
	// P12: the wheel means what the build sub-state says; Ctrl is one canonical 2.5 degree step everywhere in build mode.
	if (BuildMode->GetMode() != EGLToolMode::Build)
	{
		return;
	}
	const bool bShift = IsDown(EKeys::LeftShift, EKeys::RightShift);
	if (IsDown(EKeys::LeftControl, EKeys::RightControl))
	{
		BuildMode->RotateFine(Direction);
		return;
	}
	if (BuildMode->GetView().bBuildCamera && bShift)
	{
		BuildMode->AdjustCameraHeight(Direction * 50.f);
		return;
	}
	switch (BuildMode->GetView().State)
	{
	case EGLBuildState::Browse: bShift ? BuildMode->BrowseCategory(Direction) : BuildMode->BrowseMove(Direction); break;
	case EGLBuildState::Place: bShift ? BuildMode->CycleCategory(Direction) : BuildMode->CycleVariant(Direction); break;
	case EGLBuildState::Finish: BuildMode->CycleFinish(Direction); break;
	case EGLBuildState::Remove: break;
	}
}

void AGLCharacter::RotatePiece()
{
	BuildMode->RotateQuarter(IsDown(EKeys::LeftShift, EKeys::RightShift) ? -1 : 1);
}

void AGLCharacter::Favorite(int32 Slot)
{
	if (BuildMode->GetMode() == EGLToolMode::Build)
	{
		IsDown(EKeys::LeftControl, EKeys::RightControl) ? BuildMode->PinFavorite(Slot) : BuildMode->SelectFavorite(Slot);
	}
}

void AGLCharacter::BuildBack()
{
	if (BuildMode->GetMode() != EGLToolMode::None)
	{
		BuildMode->Back();
	}
}

void AGLCharacter::BuildCameraPressed()
{
	BuildMode->BuildCamera(true);
}

void AGLCharacter::BuildCameraReleased()
{
	BuildMode->BuildCamera(false);
}

void AGLCharacter::FrictionNote()
{
	GLFrictionLog::OpenPicker(GetWorld());
}

void AGLCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// P12 build camera: pulled back and raised around Zenny (bounded; never free flight). The aim is the camera's view,
	// so what it shows is what build mode targets.
	const FGLBuildView& View = BuildMode->GetView();
	const bool bPulled = View.bBuildCamera && View.Mode == EGLToolMode::Build;
	const float Alpha = FMath::Clamp(DeltaSeconds * 8.f, 0.f, 1.f);
	CameraBoom->TargetArmLength = FMath::Lerp(CameraBoom->TargetArmLength, bPulled ? BuildCameraArmCm : DefaultArmCm, Alpha);
	const FVector Offset = bPulled ? FVector(0.f, 60.f, BuildCameraRaiseCm + View.CameraHeightCm) : DefaultSocketOffset;
	CameraBoom->SocketOffset = FMath::Lerp(CameraBoom->SocketOffset, Offset, Alpha);
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
