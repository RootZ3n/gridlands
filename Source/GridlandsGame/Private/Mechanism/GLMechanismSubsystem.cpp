#include "Mechanism/GLMechanismSubsystem.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"
#include "Mechanism/GLMechanism.h"
#include "Noise/GLNoiseSubsystem.h"
#include "World/GLPlacementSubsystem.h"

namespace
{
	FVector Metres(const TArray<double>& V)
	{
		return V.Num() >= 3 ? FVector(V[0], V[1], V[2]) * 100.0 : FVector::ZeroVector;
	}

	FVector Local(const FGLMechanismRecord& Record, const TArray<double>& Offset)
	{
		return Record.Location + FRotator(0.0, Record.Yaw, 0.0).RotateVector(Metres(Offset));
	}
}

const FGLMechanismDef* UGLMechanismSubsystem::DefOf(const FGLMechanismRecord& Record)
{
	return GLContent::Get().Find<FGLMechanismDef>(Record.Definition);
}

bool UGLMechanismSubsystem::AddRecord(FName Placement, FName Definition, FName Cell, const FVector& Location, double Yaw)
{
	const FGLMechanismDef* Def = GLContent::Get().Find<FGLMechanismDef>(Definition);
	if (!Def || Placement.IsNone())
	{
		return false;
	}
	FGLMechanismRecord& Record = Records.Add(Placement);
	Record.Placement = Placement;
	Record.Definition = Definition;
	Record.Cell = Cell;
	Record.Location = Location;
	Record.Yaw = Yaw;
	Record.State = Def->Initial;
	return true;
}

AGLMechanism* UGLMechanismSubsystem::Present(FName Placement)
{
	FGLMechanismRecord* Record = Records.Find(Placement);
	if (!Record || Record->Actor.IsValid())
	{
		return nullptr;
	}
	AGLMechanism* Actor = GetWorld()->SpawnActor<AGLMechanism>(Record->Location, FRotator(0.0, Record->Yaw, 0.0));
	if (Actor)
	{
		Actor->Setup(*Record);
		Record->Actor = Actor;
	}
	return Actor;
}

TArray<AGLMechanism*> UGLMechanismSubsystem::RemoveCell(FName Cell)
{
	TArray<AGLMechanism*> Out;
	for (auto It = Records.CreateIterator(); It; ++It)
	{
		if (It.Value().Cell == Cell)
		{
			if (AGLMechanism* Actor = It.Value().Actor.Get())
			{
				Out.Add(Actor);
			}
			It.RemoveCurrent();
		}
	}
	return Out;
}

bool UGLMechanismSubsystem::Switch(FName Placement, const FString& To, AActor* Instigator)
{
	FGLMechanismRecord* Record = Records.Find(Placement);
	const FGLMechanismDef* Def = Record ? DefOf(*Record) : nullptr;
	if (!Def || !Def->States.Contains(To) || Record->State == To)
	{
		return false;
	}
	UWorld* World = GetWorld();
	Record->State = To;
	++Record->Switches;
	Record->SwitchedAt = World->GetTimeSeconds();
	// The decision: who is inside the box at this instant, and susceptible, is neutralized. Nobody who enters later.
	int32 Held = 0;
	if (Def->HasNeutralize() && Def->Neutralize.InState == To)
	{
		const FBox Box = NeutralizeBox(*Record);
		if (UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>())
		{
			TArray<FName> Inside;
			for (const TPair<FName, FGLActorPlacement>& Entry : Placements->GetActorModels())
			{
				FVector At;
				if (Entry.Value.Kind == TEXT("spawn") && Placements->CreatureLocation(Entry.Key, At) && Box.IsInsideOrOn(At))
				{
					Inside.Add(Entry.Key);
				}
			}
			for (const FName& Id : Inside)
			{
				FVector At;
				Placements->CreatureLocation(Id, At);
				Held += Placements->TryNeutralize(Id, Def->Neutralize.Tag, Placement, At, Record->Yaw) ? 1 : 0;
			}
		}
	}
	if (const FName* Noise = Def->EnterNoise.Find(To))
	{
		const FVector Where = Def->HasNeutralize() ? NeutralizeBox(*Record).GetCenter() : Record->Location;
		UGLNoiseSubsystem::EmitAction(this, *Noise, Where, Instigator);
	}
	FGLGameplayEvent Event;
	Event.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Mechanism.Switched"));
	Event.Subject = Record->Definition;
	Event.Instigator = Instigator;
	Event.Numbers.Add(TEXT("neutralized"), Held);
	UGLEventSubsystem::Emit(this, MoveTemp(Event));
	UE_LOG(LogGridlands, Log, TEXT("Mechanism %s -> %s (%d neutralized)"), *Placement.ToString(), *To, Held);
	if (AGLMechanism* Actor = Record->Actor.Get())
	{
		Actor->PresentState(*Record);
	}
	return true;
}

bool UGLMechanismSubsystem::Restore(const FGLSavedMechanism& Saved)
{
	FGLMechanismRecord* Record = Records.Find(Saved.Placement);
	const FGLMechanismDef* Def = Record ? DefOf(*Record) : nullptr;
	if (!Def || !Def->States.Contains(Saved.State))
	{
		return false;
	}
	Record->State = Saved.State;
	Record->Switches = Saved.Switches;
	Record->SwitchedAt = -1e9; // at rest: nothing replays
	if (AGLMechanism* Actor = Record->Actor.Get())
	{
		Actor->PresentState(*Record);
	}
	return true;
}

void UGLMechanismSubsystem::CaptureCell(FName Cell, TArray<FGLSavedMechanism>& Out) const
{
	for (const TPair<FName, FGLMechanismRecord>& Entry : Records)
	{
		const FGLMechanismDef* Def = DefOf(Entry.Value);
		if (Entry.Value.Cell == Cell && Def && (Entry.Value.State != Def->Initial || Entry.Value.Switches > 0))
		{
			Out.Add({ Entry.Key, Entry.Value.State, Entry.Value.Switches });
		}
	}
}

const FGLMechanismRecord* UGLMechanismSubsystem::FindOperable(const FVector& Near, double RadiusCm) const
{
	const FGLMechanismRecord* Best = nullptr;
	double BestDistance = RadiusCm;
	for (const TPair<FName, FGLMechanismRecord>& Entry : Records)
	{
		const FGLMechanismDef* Def = DefOf(Entry.Value);
		const double Distance = FVector::Dist(Near, Entry.Value.Location);
		if (Def && Def->CanOperate() && Entry.Value.State == Def->Operate.From && Distance <= BestDistance)
		{
			Best = &Entry.Value;
			BestDistance = Distance;
		}
	}
	return Best;
}

FBox UGLMechanismSubsystem::NeutralizeBox(const FGLMechanismRecord& Record)
{
	const FGLMechanismDef* Def = DefOf(Record);
	if (!Def || !Def->HasNeutralize())
	{
		return FBox(ForceInit);
	}
	const FVector Centre = Local(Record, Def->Neutralize.Offset);
	return FBox::BuildAABB(Centre, Metres(Def->Neutralize.Extent));
}

bool UGLMechanismSubsystem::IsAmbientOn(const FGLMechanismRecord& Record, double Seconds)
{
	const FGLMechanismDef* Def = DefOf(Record);
	if (!Def || !Def->HasAmbient() || !Def->Ambient.ActiveIn.Contains(Record.State))
	{
		return false;
	}
	const FGLMechanismAmbientDef& A = Def->Ambient;
	return A.Period <= 0.0 || FMath::Fmod(Seconds + A.Phase, A.Period) < A.OnSeconds;
}

double UGLMechanismSubsystem::MaskAt(const FVector& Listener) const
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	double Mask = 0.0;
	for (const TPair<FName, FGLMechanismRecord>& Entry : Records)
	{
		const FGLMechanismDef* Def = DefOf(Entry.Value);
		if (Def && IsAmbientOn(Entry.Value, Now) && FVector::Dist(Listener, Local(Entry.Value, Def->Ambient.Offset)) <= Def->Ambient.Radius * 100.0)
		{
			Mask = FMath::Max(Mask, Def->Ambient.Mask);
		}
	}
	return Mask;
}
