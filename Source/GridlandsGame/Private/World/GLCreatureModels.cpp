// P9 (ADR-0037): creature models. A creature's gameplay facts live in its placement's model, so streaming,
// presentation recreation and save/restart never reset what it knows, where it is, its health or how it left
// the encounter. The actor senses and moves, and writes its facts through every behaviour step.

#include "World/GLPlacementSubsystem.h"

#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "GridlandsGame.h"
#include "Inventory/GLInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Noise/GLNoiseSubsystem.h"

namespace
{
	/** Feet, not capsule centre: what a model keeps (the actor is made 70 cm above it). */
	FVector FeetOf(const AGLCreature& Creature)
	{
		const UCapsuleComponent* Capsule = Creature.GetCapsuleComponent();
		return Creature.GetActorLocation() - FVector(0, 0, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 60.0);
	}

	void Announce(UObject* Context, const TCHAR* Tag, FName Subject, AActor* Instigator, TFunctionRef<void(FGLGameplayEvent&)> Fill)
	{
		FGLGameplayEvent Event;
		Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(Tag);
		Event.Subject = Subject;
		Event.Instigator = Instigator;
		Fill(Event);
		UGLEventSubsystem::Emit(Context, MoveTemp(Event));
	}
}

void UGLPlacementSubsystem::SyncCreatureFromActor(const AGLCreature& Creature)
{
	FGLActorPlacement* Model = ActorModels.Find(Creature.GetPlacementId());
	if (!Model || Model->Actor.Get() != &Creature || !Model->Creature.IsActiveHostile())
	{
		return; // not its model's presentation (a proof creature, a retired actor), or already out of the encounter
	}
	FGLCreatureModel& M = Model->Creature;
	Model->Location = FeetOf(Creature);
	M.Yaw = Creature.GetActorRotation().Yaw;
	M.State = Creature.GetState();
	M.PatrolIndex = Creature.GetPatrolIndex();
	M.LastKnown = Creature.GetLastKnown();
	M.SearchSeconds = Creature.GetSearchSecondsLeft();
	M.Noise = Creature.GetNoise();
	M.NoiseSeconds = Creature.GetNoiseSecondsLeft();
	M.Lure = Creature.GetLure();
	M.LureSeconds = Creature.GetLureSecondsLeft();
	M.StampWorldSeconds = GetWorld()->GetTimeSeconds();
	if (const UGLHealthComponent* Health = Creature.GetHealth())
	{
		M.Health = Health->GetCurrent() < Health->GetMax() ? Health->GetCurrent() : -1.0;
	}
}

bool UGLPlacementSubsystem::CreatureLocation(FName PlacementId, FVector& OutLocation) const
{
	const FGLActorPlacement* Model = ActorModels.Find(PlacementId);
	if (!Model || Model->Kind != TEXT("spawn"))
	{
		return false;
	}
	const AGLCreature* Creature = Cast<AGLCreature>(Model->Actor.Get());
	OutLocation = Creature && Model->Creature.IsActiveHostile() ? FeetOf(*Creature) : Model->Location;
	return true;
}

FGLSavedCreature UGLPlacementSubsystem::CaptureCreature(const FGLActorPlacement& Model) const
{
	const FGLCreatureModel& M = Model.Creature;
	// Without an actor its timers ran on against the world clock since they were last written.
	const bool bDriven = Model.Actor.IsValid() && M.IsActiveHostile();
	const double Passed = bDriven ? 0.0 : FMath::Max(0.0, GetWorld()->GetTimeSeconds() - M.StampWorldSeconds);
	FGLSavedCreature Saved;
	Saved.Placement = Model.Placement;
	Saved.State = static_cast<uint8>(M.State);
	Saved.Location = Model.Location;
	Saved.Yaw = M.Yaw;
	Saved.Health = M.Health;
	Saved.PatrolIndex = M.PatrolIndex;
	Saved.LastKnown = M.LastKnown;
	Saved.SearchSeconds = FMath::Max(0.0, M.SearchSeconds - Passed);
	Saved.Noise = M.Noise;
	Saved.NoiseSeconds = FMath::Max(0.0, M.NoiseSeconds - Passed);
	Saved.Lure = M.Lure;
	Saved.LureSeconds = FMath::Max(0.0, M.LureSeconds - Passed);
	Saved.Outcome = M.Outcome;
	Saved.NeutralizedHow = M.NeutralizedHow;
	Saved.NeutralizedBy = M.NeutralizedBy;
	Saved.HeldAt = M.HeldAt;
	Saved.HeldYaw = M.HeldYaw;
	return Saved;
}

bool UGLPlacementSubsystem::RestoreCreature(const FGLSavedCreature& Saved, double ElapsedSeconds)
{
	FGLActorPlacement* Model = ActorModels.Find(Saved.Placement);
	if (!Model || Model->Kind != TEXT("spawn"))
	{
		return false;
	}
	FGLCreatureModel& M = Model->Creature;
	const double Elapsed = FMath::Max(0.0, ElapsedSeconds);
	Model->Location = Saved.Location;
	M.State = static_cast<EGLCreatureState>(Saved.State);
	M.Yaw = Saved.Yaw;
	M.Health = Saved.Health;
	M.PatrolIndex = Saved.PatrolIndex;
	M.LastKnown = Saved.LastKnown;
	M.SearchSeconds = FMath::Max(0.0, Saved.SearchSeconds - Elapsed);
	M.Noise = Saved.Noise;
	M.NoiseSeconds = FMath::Max(0.0, Saved.NoiseSeconds - Elapsed);
	M.Lure = Saved.Lure;
	M.LureSeconds = FMath::Max(0.0, Saved.LureSeconds - Elapsed);
	M.StampWorldSeconds = GetWorld()->GetTimeSeconds();
	M.Outcome = Saved.Outcome;
	M.NeutralizedHow = Saved.NeutralizedHow;
	M.NeutralizedBy = Saved.NeutralizedBy;
	M.HeldAt = Saved.HeldAt;
	M.HeldYaw = Saved.HeldYaw;
	if (AGLCreature* Creature = Cast<AGLCreature>(Model->Actor.Get()))
	{
		if (M.Outcome == EGLCreatureOutcome::Defeated)
		{
			Creature->RestoreDefeated();
		}
		else
		{
			Creature->SetActorLocation(Model->Location + FVector(0, 0, 70), false, nullptr, ETeleportType::TeleportPhysics);
			Creature->RestoreFromModel(M, GetWorld()->GetTimeSeconds());
		}
	}
	return true;
}

bool UGLPlacementSubsystem::TryNeutralize(FName PlacementId, FName How, FName By, const FVector& HeldAt, double HeldYaw)
{
	FGLActorPlacement* Model = ActorModels.Find(PlacementId);
	const FGLCreatureDef* Def = Model && Model->Kind == TEXT("spawn") ? GLContent::Get().Find<FGLCreatureDef>(Model->Definition) : nullptr;
	if (!Def || !Model->Creature.IsActiveHostile() || !Def->NeutralizableBy.Contains(How))
	{
		return false; // unknown, already out of the encounter, or not susceptible to this outcome
	}
	FGLCreatureModel& M = Model->Creature;
	M.Outcome = EGLCreatureOutcome::Neutralized;
	M.State = EGLCreatureState::Neutralized;
	M.NeutralizedHow = How;
	M.NeutralizedBy = By;
	M.HeldAt = HeldAt;
	M.HeldYaw = HeldYaw;
	M.SearchSeconds = M.NoiseSeconds = M.LureSeconds = 0.0;
	Model->Location = HeldAt;
	AGLCreature* Creature = Cast<AGLCreature>(Model->Actor.Get());
	if (Creature)
	{
		Creature->PresentNeutralized(HeldAt, HeldYaw);
	}
	// No damage, no death, no kill: its own event. Health is untouched.
	Announce(this, TEXT("Event.Creature.Neutralized"), Model->Definition, Creature, [How](FGLGameplayEvent& Event)
	{
		Event.Numbers.Add(How, 1.0); // how it was neutralized (Neutralize.*), for dialogue, statistics and achievements
	});
	ResolveEncounter(*Model, true);
	return true;
}

void UGLPlacementSubsystem::MarkDefeated(FName PlacementId)
{
	FGLActorPlacement* Model = ActorModels.Find(PlacementId);
	if (!Model || !Model->Creature.IsActiveHostile())
	{
		return; // an outcome is final: a neutralized creature is never re-scored as defeated
	}
	Model->Creature.Outcome = EGLCreatureOutcome::Defeated;
	Model->Creature.State = EGLCreatureState::Defeated;
	ResolveEncounter(*Model, false);
}

void UGLPlacementSubsystem::ResolveEncounter(const FGLActorPlacement& Model, bool bNeutralized)
{
	// Called only on the transition out of None: a restore never gets here, so nothing replays.
	const FGLCreatureDef* Def = GLContent::Get().Find<FGLCreatureDef>(Model.Definition);
	if (!Def || !Def->IsEncounter())
	{
		return;
	}
	const UGLGlitchSubsystem* Glitches = GetWorld()->GetSubsystem<UGLGlitchSubsystem>();
	AActor* Zenny = Glitches && Glitches->GetCommander() ? Glitches->GetCommander() : UGameplayStatics::GetPlayerPawn(this, 0);
	if (UGLInventoryComponent* Inventory = Zenny ? Zenny->FindComponentByClass<UGLInventoryComponent>() : nullptr)
	{
		for (const FGLSalvageYieldDef& Reward : Def->Encounter.Rewards)
		{
			Inventory->AddItem(Reward.Item, Reward.Count);
		}
	}
	Announce(this, TEXT("Event.Encounter.Resolved"), Model.Placement, Zenny, [bNeutralized](FGLGameplayEvent& Event)
	{
		Event.Numbers.Add(TEXT("neutralized"), bNeutralized ? 1.0 : 0.0);
	});
}

int32 UGLPlacementSubsystem::DeliverNoise(const FGLNoiseEvent& Noise)
{
	const UGLNoiseSubsystem* NoiseWorld = GetWorld()->GetSubsystem<UGLNoiseSubsystem>();
	const double Now = GetWorld()->GetTimeSeconds();
	int32 Heard = 0;
	for (TPair<FName, FGLActorPlacement>& Entry : ActorModels)
	{
		FGLActorPlacement& Model = Entry.Value;
		if (Model.Kind != TEXT("spawn") || !Model.Creature.IsActiveHostile())
		{
			continue;
		}
		if (AGLCreature* Creature = Cast<AGLCreature>(Model.Actor.Get()))
		{
			Heard += Creature->HearNoise(Noise) ? 1 : 0; // presented: its actor hears (same rule)
			continue;
		}
		// Waiting for presentation, or far: its model hears by the same rule, at its own position.
		const FGLCreatureDef* Def = GLContent::Get().Find<FGLCreatureDef>(Model.Definition);
		const double Mask = NoiseWorld ? NoiseWorld->MaskAt(Model.Location) : 0.0;
		if (!Def || !GLCreatureRules::Hears(*Def, Model.Location, Noise.Location, Noise.RadiusCm, Mask))
		{
			continue;
		}
		FGLCreatureModel& M = Model.Creature;
		// Bring its other timers up to now before stamping them.
		const double Passed = FMath::Max(0.0, Now - M.StampWorldSeconds);
		M.SearchSeconds = FMath::Max(0.0, M.SearchSeconds - Passed);
		M.NoiseSeconds = FMath::Max(0.0, M.NoiseSeconds - Passed);
		M.LureSeconds = FMath::Max(0.0, M.LureSeconds - Passed);
		if (Noise.bDistraction)
		{
			M.Lure = Noise.Location;
			M.LureSeconds = Noise.InvestigateSeconds;
		}
		else
		{
			M.Noise = Noise.Location;
			M.NoiseSeconds = Noise.InvestigateSeconds;
		}
		if (M.State == EGLCreatureState::Idle || M.State == EGLCreatureState::Patrol || M.State == EGLCreatureState::Return || Noise.bDistraction)
		{
			M.State = EGLCreatureState::Investigate;
		}
		M.StampWorldSeconds = Now;
		++Heard;
	}
	return Heard;
}
