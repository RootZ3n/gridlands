#include "Combat/GLCombatComponent.h"

#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Inventory/GLInventoryComponent.h"

UGLCombatComponent::UGLCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UGLHealthComponent* UGLCombatComponent::Health() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UGLHealthComponent>() : nullptr;
}

void UGLCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	RespawnPoint = GetOwner()->GetActorLocation();
	if (UGLHealthComponent* H = Health(); H && !bBound)
	{
		H->SetMax(MaxHealth);
		H->OnDamaged.AddUObject(this, &UGLCombatComponent::HandleDamaged);
		H->OnDied.AddUObject(this, &UGLCombatComponent::HandleDied);
		bBound = true;
	}
}

void UGLCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Advance(DeltaTime);
}

AGLCreature* UGLCombatComponent::Attack()
{
	UGLHealthComponent* H = Health();
	if (Cooldown > 0.0 || !H || H->IsDead())
	{
		return nullptr;
	}
	// The best weapon Zenny carries, else fists.
	double Damage = FistDamage, Reach = FistReachCm, Rest = FistCooldown;
	FName Weapon = NAME_None;
	if (const UGLInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UGLInventoryComponent>())
	{
		for (const FGLInventoryStack& Stack : Inventory->GetInventory().GetStacks())
		{
			const FGLItemDef* Item = GLContent::Get().Find<FGLItemDef>(Stack.Item);
			if (Item && Item->Weapon.Damage > Damage)
			{
				Damage = Item->Weapon.Damage;
				Reach = Item->Weapon.Reach * 100.0;
				Rest = Item->Weapon.CooldownSeconds;
				Weapon = Stack.Item;
			}
		}
	}
	Cooldown = Rest;
	AGLCreature* Best = nullptr;
	double BestDistance = Reach;
	const FVector Origin = GetOwner()->GetActorLocation();
	const FVector2D Facing = FVector2D(GetOwner()->GetActorForwardVector()).GetSafeNormal();
	for (TActorIterator<AGLCreature> It(GetWorld()); It; ++It)
	{
		const FVector To = It->GetActorLocation() - Origin;
		const double Distance = To.Size2D();
		const bool bInFront = FVector2D::DotProduct(Facing, FVector2D(To).GetSafeNormal()) > 0.5; // ~60 degrees either side
		if (!It->IsDefeated() && Distance <= BestDistance && bInFront)
		{
			Best = *It;
			BestDistance = Distance;
		}
	}
	if (Best)
	{
		Best->GetHealth()->ApplyDamage(Damage, GetOwner());
		Emit(TEXT("Event.Creature.Hurt"), Best->GetDefId(), Damage);
	}
	return Best;
}

void UGLCombatComponent::Advance(float DeltaSeconds)
{
	Cooldown = FMath::Max(0.0, Cooldown - DeltaSeconds);
	if (RespawnIn > 0.0)
	{
		RespawnIn -= DeltaSeconds;
		if (RespawnIn <= 0.0)
		{
			RespawnIn = 0.0;
			GetOwner()->SetActorLocation(RespawnPoint, false, nullptr, ETeleportType::TeleportPhysics);
			if (UGLHealthComponent* H = Health())
			{
				H->Revive();
			}
			Emit(TEXT("Event.Player.Respawned"), NAME_None);
		}
	}
}

void UGLCombatComponent::HandleDamaged(double Taken, AActor* Instigator)
{
	Emit(TEXT("Event.Player.Hurt"), NAME_None, Taken);
}

void UGLCombatComponent::HandleDied(AActor* Killer)
{
	RespawnIn = RespawnSeconds;
	const AGLCreature* Creature = Cast<AGLCreature>(Killer);
	Emit(TEXT("Event.Player.Died"), Creature ? Creature->GetDefId() : NAME_None);
}

void UGLCombatComponent::Emit(const TCHAR* Tag, FName Subject, double Number)
{
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(Tag);
	Event.Subject = Subject;
	Event.Instigator = GetOwner();
	if (Number != 0.0)
	{
		Event.Numbers.Add(TEXT("amount"), Number);
	}
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
}
