#include "Pehlichi/GLRepairComponent.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "GridlandsGame.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Pehlichi/GLCapabilityComponent.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"

UGLRepairComponent::UGLRepairComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGLRepairComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Advance(DeltaTime);
}

void UGLRepairComponent::AssignTarget(AGLGlitch* Glitch, AActor* InCommander)
{
	Target = Glitch;
	Commander = InCommander;
	if (UGLCompanionPositioningComponent* Positioning = GetOwner()->FindComponentByClass<UGLCompanionPositioningComponent>())
	{
		Positioning->MoveTo(Glitch->GetRepairPoint());
	}
}

void UGLRepairComponent::Stop()
{
	if (AGLGlitch* Glitch = Target.Get())
	{
		if (Glitch->GetGlitch()->GetState() == EGLGlitchState::Repairing)
		{
			Glitch->GetGlitch()->Interrupt(FGLRepairAuthority());
		}
	}
	Target = nullptr;
}

bool UGLRepairComponent::TakeDeliveredItems(UGLGlitchComponent& Glitch)
{
	const FGLGlitchDef* Def = GLContent::Get().Find<FGLGlitchDef>(Glitch.GetGlitchId());
	if (!Def || Glitch.AreItemsDelivered())
	{
		return true;
	}
	UGLInventoryComponent* Inventory = Commander.IsValid() ? Commander->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	for (const FGLGlitchRequirementDef& Requirement : Def->Requirements)
	{
		if (Requirement.Kind == TEXT("Requirement.ItemDelivered") && (!Inventory || Inventory->CountOf(Requirement.Item) < FMath::Max(1, Requirement.Count)))
		{
			return false;
		}
	}
	bool bAny = false;
	for (const FGLGlitchRequirementDef& Requirement : Def->Requirements)
	{
		if (Requirement.Kind == TEXT("Requirement.ItemDelivered"))
		{
			Inventory->RemoveItem(Requirement.Item, FMath::Max(1, Requirement.Count));
			bAny = true;
		}
	}
	if (bAny)
	{
		Glitch.MarkItemsDelivered(FGLRepairAuthority());
	}
	return true;
}

void UGLRepairComponent::ApplyRewards(const UGLGlitchComponent& Glitch)
{
	const FGLGlitchDef* Def = GLContent::Get().Find<FGLGlitchDef>(Glitch.GetGlitchId());
	if (!Def)
	{
		return;
	}
	for (const FGLGlitchRewardDef& Reward : Def->Rewards)
	{
		if (!Reward.Capability.IsNone())
		{
			// Capability rewards go to Pehlichi; a player-held capability component would receive "Zenny" rewards later.
			if (UGLCapabilityComponent* Capabilities = GetOwner()->FindComponentByClass<UGLCapabilityComponent>())
			{
				Capabilities->Raise(Reward.Capability, FMath::Max(1, Reward.Delta));
			}
		}
		else if (!Reward.Knowledge.IsNone())
		{
			if (UGLKnowledgeSubsystem* Knowledge = GetWorld() ? GetWorld()->GetSubsystem<UGLKnowledgeSubsystem>() : nullptr)
			{
				Knowledge->Learn(Reward.Knowledge);
			}
		}
		else if (!Reward.Item.IsNone())
		{
			if (UGLInventoryComponent* Inventory = Commander.IsValid() ? Commander->FindComponentByClass<UGLInventoryComponent>() : nullptr)
			{
				Inventory->AddItem(Reward.Item, FMath::Max(1, Reward.Count)); // glitch rewards never scale (ADR-0016)
			}
		}
	}
}

void UGLRepairComponent::Advance(float DeltaTime)
{
	AGLGlitch* Glitch = Target.Get();
	if (!Glitch)
	{
		return;
	}
	UGLGlitchComponent& Component = *Glitch->GetGlitch();
	const EGLGlitchState State = Component.GetState();
	if (State == EGLGlitchState::Repaired || State == EGLGlitchState::Latent || State == EGLGlitchState::Detected)
	{
		Target = nullptr; // done, or no longer repairable: nothing to do
		return;
	}
	if (FVector::Dist2D(GetOwner()->GetActorLocation(), Glitch->GetRepairPoint()) > RepairReach)
	{
		return; // still on the way
	}
	if (State == EGLGlitchState::Repairable || State == EGLGlitchState::Interrupted)
	{
		if (!TakeDeliveredItems(Component) || !Component.BeginRepair(FGLRepairAuthority()))
		{
			Target = nullptr;
			return;
		}
	}
	if (Component.AddRepairProgress(DeltaTime, FGLRepairAuthority()))
	{
		UE_LOG(LogGridlands, Log, TEXT("Pehlichi repaired %s"), *Component.GetGlitchId().ToString());
		ApplyRewards(Component);
		Target = nullptr;
	}
}
