#include "Combat/GLCreature.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Noise/GLNoiseSubsystem.h"
#include "NavigationInvokerComponent.h"
#include "Combat/GLHealthComponent.h"
#include "Presentation/GLDerez.h"
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
	Derez = CreateDefaultSubobject<UGLDerezComponent>(TEXT("Derez"));
	// A creature carries its own navigation (ADR-0029), so it paths wherever it is, player or not.
	// Its reach (sight, hearing, leash) is at most 16 m in data: 32 m covers twice that.
	NavInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvoker"));
	NavInvoker->SetGenerationRadii(3200.f, 4800.f);
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
	return true;
}

void AGLCreature::BeginPlay()
{
	Super::BeginPlay();
}

void AGLCreature::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
}

void AGLCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Think(DeltaSeconds);
}

bool AGLCreature::HearNoise(const FGLNoiseEvent& Heard)
{
	const FGLCreatureDef* Def = GLContent::Get().Find<FGLCreatureDef>(DefId);
	if (!Def || IsDefeated() || !GLCreatureRules::Hears(*Def, GetActorLocation(), Heard.Location, Heard.RadiusCm))
	{
		return false;
	}
	if (Heard.bDistraction)
	{
		Lure = Heard.Location; // where Pehlichi made the noise
		LureLeft = Heard.InvestigateSeconds;
	}
	else
	{
		Noise = Heard.Location;
		NoiseLeft = Heard.InvestigateSeconds;
	}
	return true;
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
	NoiseLeft = FMath::Max(0.0, NoiseLeft - DeltaSeconds);
	if (State == EGLCreatureState::Search)
	{
		SearchLeft = FMath::Max(0.0, SearchLeft - DeltaSeconds);
	}
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
	Facts.NoiseSecondsLeft = NoiseLeft;
	Facts.Noise = Noise;
	Facts.SearchSecondsLeft = SearchLeft;
	Facts.LastKnown = LastKnown;
	Facts.SecondsSinceAttack = SinceAttack;
	const FGLCreatureDecision Decision = GLCreatureRules::Decide(*Def, State, Facts);
	if ((Decision.State == EGLCreatureState::Chase || Decision.State == EGLCreatureState::Attack) && Zenny)
	{
		// It sees Zenny now: remember where, for when it stops seeing (P6).
		LastKnown = Zenny->GetActorLocation();
		SearchLeft = Def->Perception.MemorySeconds > 0.0 ? Def->Perception.MemorySeconds : GLContent::Tuning().Noise.MemorySeconds;
		NoiseLeft = 0.0;
	}
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
		// Pehlichi's distraction and an ordinary noise are different stories (dialogue tells them apart).
		Emit(LureLeft > 0.0 ? TEXT("Event.Creature.Distracted") : TEXT("Event.Creature.Heard"));
	}
	else if (Next == EGLCreatureState::Search)
	{
		Emit(TEXT("Event.Creature.Searching"));
	}
	else if ((Next == EGLCreatureState::Return || Next == EGLCreatureState::Idle)
		&& (Was == EGLCreatureState::Chase || Was == EGLCreatureState::Attack || Was == EGLCreatureState::Search))
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
	// Gameplay: defeated now (no collision, no behaviour). Presentation: it de-rezzes, then is hidden.
	State = EGLCreatureState::Defeated;
	Health->Restore(0.0);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	Derez->Start(1.2f);
}

void AGLCreature::RestoreDefeated()
{
	State = EGLCreatureState::Defeated;
	Health->Restore(0.0);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true); // restored as already gone (no replayed de-rez)
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
