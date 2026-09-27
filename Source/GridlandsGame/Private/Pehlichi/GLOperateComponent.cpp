#include "Pehlichi/GLOperateComponent.h"

#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Mechanism/GLMechanismSubsystem.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"

UGLOperateComponent::UGLOperateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGLOperateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Advance(DeltaTime);
}

void UGLOperateComponent::Assign(FName Mechanism, AActor* InCommander)
{
	const UGLMechanismSubsystem* Mechanisms = GetWorld()->GetSubsystem<UGLMechanismSubsystem>();
	const FGLMechanismRecord* Record = Mechanisms ? Mechanisms->FindRecord(Mechanism) : nullptr;
	if (!Record)
	{
		return;
	}
	Target = Mechanism;
	Commander = InCommander;
	Progress = 0.0;
	if (UGLCompanionPositioningComponent* Positioning = GetOwner()->FindComponentByClass<UGLCompanionPositioningComponent>())
	{
		Positioning->MoveTo(Record->Location + FVector(0, 0, 60));
	}
}

void UGLOperateComponent::Stop()
{
	Target = NAME_None;
	Progress = 0.0;
}

void UGLOperateComponent::Advance(float DeltaTime)
{
	if (Target.IsNone())
	{
		return;
	}
	UGLMechanismSubsystem* Mechanisms = GetWorld()->GetSubsystem<UGLMechanismSubsystem>();
	const FGLMechanismRecord* Record = Mechanisms ? Mechanisms->FindRecord(Target) : nullptr;
	const FGLMechanismDef* Def = Record ? UGLMechanismSubsystem::DefOf(*Record) : nullptr;
	if (!Def || !Def->CanOperate() || Record->State != Def->Operate.From)
	{
		Stop(); // gone (streamed out) or no longer operable
		return;
	}
	const double Reach = (Def->Operate.Reach > 0.0 ? Def->Operate.Reach : 1.5) * 100.0;
	if (FVector::Dist2D(GetOwner()->GetActorLocation(), Record->Location) > Reach)
	{
		return; // still on the way
	}
	Progress += DeltaTime;
	if (Progress >= Def->Operate.Seconds)
	{
		const FName Done = Target;
		const FString To = Def->Operate.To;
		Stop();
		Mechanisms->Switch(Done, To, GetOwner());
		if (UGLCompanionPositioningComponent* Positioning = GetOwner()->FindComponentByClass<UGLCompanionPositioningComponent>(); Positioning && Commander.IsValid())
		{
			Positioning->Follow(Commander.Get());
		}
	}
}
