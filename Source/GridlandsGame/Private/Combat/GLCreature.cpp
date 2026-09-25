#include "Combat/GLCreature.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Combat/GLHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"
#include "Inventory/GLInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AGLCreature::AGLCreature()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCapsuleComponent()->InitCapsuleSize(40.f, 60.f);
	Health = CreateDefaultSubobject<UGLHealthComponent>(TEXT("Health"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.8f, 0.7f, 0.9f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
	}
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0, 540.0, 0.0);
	bUseControllerRotationYaw = false;
}

bool AGLCreature::Setup(FName InDefId, FName InPlacementId)
{
	const FGLCreatureDef* Def = GLContent::Get().Find<FGLCreatureDef>(InDefId);
	if (!Def)
	{
		return false;
	}
	DefId = InDefId;
	PlacementId = InPlacementId;
	Home = GetActorLocation();
	Health->SetMax(Def->Health);
	GetCharacterMovement()->MaxWalkSpeed = Def->WalkSpeed * 100.0;
	Health->OnDied.AddUObject(this, &AGLCreature::HandleDied);
	if (UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* Paint = Body->CreateDynamicMaterialInstance(0, Shape))
		{
			Paint->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.05f, 0.45f)); // corrupted violet
		}
	}
	if (UGLEventSubsystem* Bus = GetWorld()->GetSubsystem<UGLEventSubsystem>())
	{
		LureSubscription = Bus->Subscribe(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Pehlichi.Lure")),
			FGLGameplayEventDelegate::CreateUObject(this, &AGLCreature::HandleLure));
	}
	return true;
}

void AGLCreature::BeginPlay()
{
	Super::BeginPlay();
}

void AGLCreature::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UGLEventSubsystem* Bus = GetWorld() ? GetWorld()->GetSubsystem<UGLEventSubsystem>() : nullptr)
	{
		Bus->Unsubscribe(LureSubscription);
	}
	Super::EndPlay(Reason);
}

void AGLCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Think(DeltaSeconds);
}

void AGLCreature::HandleLure(const FGLGameplayEvent& Event)
{
	const FGLCreatureDef* Def = GLContent::Get().Find<FGLCreatureDef>(DefId);
	const AActor* Source = Event.Instigator.Get();
	if (!Def || IsDefeated() || !Source)
	{
		return;
	}
	// Heard only within hearing radius; the lure is where Pehlichi made the noise.
	if (FVector::Dist(Source->GetActorLocation(), GetActorLocation()) <= Def->Perception.HearingRadius * 100.0)
	{
		Lure = Source->GetActorLocation();
		LureLeft = Event.Numbers.FindRef(TEXT("seconds"));
	}
}

bool AGLCreature::LineOfSightTo(const AActor* Target) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("GLCreatureSight"), false, this);
	Params.AddIgnoredActor(Target);
	const FVector Eyes = GetActorLocation() + FVector(0, 0, 40);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Target->GetActorLocation() + FVector(0, 0, 40), ECC_Visibility, Params);
}

void AGLCreature::Think(float DeltaSeconds)
{
	const FGLCreatureDef* Def = GLContent::Get().Find<FGLCreatureDef>(DefId);
	if (!Def || IsDefeated())
	{
		return;
	}
	SinceAttack += DeltaSeconds;
	LureLeft = FMath::Max(0.0, LureLeft - DeltaSeconds);
	APawn* Zenny = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Zenny)
	{
		// Tests and tools may name the target explicitly: the first actor carrying health and an inventory.
		for (TActorIterator<APawn> It(GetWorld()); It && !Zenny; ++It)
		{
			if (*It != this && It->FindComponentByClass<UGLHealthComponent>() && It->FindComponentByClass<UGLInventoryComponent>())
			{
				Zenny = *It;
			}
		}
	}
	const UGLHealthComponent* ZennyHealth = Zenny ? Zenny->FindComponentByClass<UGLHealthComponent>() : nullptr;

	FGLCreatureFacts Facts;
	Facts.Self = GetActorLocation();
	Facts.Forward = GetActorForwardVector();
	Facts.Home = Home;
	Facts.Zenny = Zenny ? Zenny->GetActorLocation() : FVector(1e9);
	Facts.bZennyAlive = Zenny && (!ZennyHealth || !ZennyHealth->IsDead());
	Facts.bLineOfSight = Zenny && LineOfSightTo(Zenny);
	Facts.LureSecondsLeft = LureLeft;
	Facts.Lure = Lure;
	Facts.SecondsSinceAttack = SinceAttack;
	const FGLCreatureDecision Decision = GLCreatureRules::Decide(*Def, State, Facts);
	Enter(Decision.State);

	AAIController* Brain = Cast<AAIController>(GetController());
	if (Decision.bMove && Brain)
	{
		GetCharacterMovement()->MaxWalkSpeed = Decision.Speed;
		if (FVector::Dist2D(Decision.MoveTo, LastMoveTarget) > 50.0 || Brain->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			Brain->MoveToLocation(Decision.MoveTo, 50.f, false, true, false, true);
			LastMoveTarget = Decision.MoveTo;
		}
	}
	else if (Brain && Brain->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		Brain->StopMovement();
		LastMoveTarget = FVector(1e12);
	}
	if (Decision.State == EGLCreatureState::Attack && Zenny)
	{
		SetActorRotation(FRotator(0.0, (Zenny->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.0));
	}
	if (Decision.bStrike && Zenny)
	{
		if (UGLHealthComponent* Target = Zenny->FindComponentByClass<UGLHealthComponent>())
		{
			Target->ApplyDamage(Def->Attack.Damage, this);
		}
		SinceAttack = 0.0;
	}
}

void AGLCreature::Enter(EGLCreatureState Next)
{
	if (Next == State)
	{
		return;
	}
	const EGLCreatureState Was = State;
	State = Next;
	UE_LOG(LogGridlands, Verbose, TEXT("Creature %s: %s -> %s"), *PlacementId.ToString(), GLCreatureRules::StateName(Was), GLCreatureRules::StateName(Next));
	if (Next == EGLCreatureState::Chase && Was != EGLCreatureState::Attack)
	{
		Emit(TEXT("Event.Creature.Spotted"));
	}
	else if (Next == EGLCreatureState::Investigate)
	{
		Emit(TEXT("Event.Creature.Distracted"));
	}
	else if (Next == EGLCreatureState::Return && (Was == EGLCreatureState::Chase || Was == EGLCreatureState::Attack))
	{
		Emit(TEXT("Event.Creature.Lost"));
	}
}

void AGLCreature::HandleDied(AActor* Killer)
{
	const FGLCreatureDef* Def = GLContent::Get().Find<FGLCreatureDef>(DefId);
	Enter(EGLCreatureState::Defeated);
	if (AAIController* Brain = Cast<AAIController>(GetController()))
	{
		Brain->StopMovement();
	}
	// Drops go straight to the one who won (combat sources, CR-1; never the only way, NC-2).
	if (UGLInventoryComponent* Inventory = Killer ? Killer->FindComponentByClass<UGLInventoryComponent>() : nullptr; Inventory && Def)
	{
		for (const FGLSalvageYieldDef& Drop : Def->Drops)
		{
			Inventory->AddItem(Drop.Item, Drop.Count);
		}
	}
	Emit(TEXT("Event.Creature.Defeated"));
	RestoreDefeated();
}

void AGLCreature::RestoreDefeated()
{
	State = EGLCreatureState::Defeated;
	Health->Restore(0.0);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true); // it de-rezzes (a presentation pass can animate this)
	SetActorTickEnabled(false);
}

void AGLCreature::Emit(const TCHAR* Tag)
{
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(Tag);
	Event.Subject = DefId;
	Event.Instigator = this;
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
}
