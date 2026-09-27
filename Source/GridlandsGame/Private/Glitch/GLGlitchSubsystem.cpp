#include "Glitch/GLGlitchSubsystem.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchRules.h"
#include "GridlandsGame.h"
#include "Inventory/GLInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Puzzle/GLPuzzleSubsystem.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "World/GLPlacementSubsystem.h"

void UGLGlitchSubsystem::Tick(float DeltaTime)
{
	SinceEvaluation += DeltaTime;
	if (SinceEvaluation >= 0.25)
	{
		SinceEvaluation = 0.0;
		EvaluateRequirements();
	}
}

void UGLGlitchSubsystem::Register(AGLGlitch* Glitch)
{
	Glitches.AddUnique(Glitch);
}

bool UGLGlitchSubsystem::AddRecord(FName Placement, FName GlitchId, FName Cell, const FVector& Location, double Yaw, const TMap<FString, FName>& Bindings)
{
	if (Records.Contains(Placement) || !GLContent::Get().Find<FGLGlitchDef>(GlitchId))
	{
		return false;
	}
	FGLGlitchRecord& Record = Records.Add(Placement);
	Record.Placement = Placement;
	Record.Glitch = GlitchId;
	Record.Cell = Cell;
	Record.Location = Location;
	Record.Yaw = Yaw;
	Record.Bindings = Bindings;
	return true;
}

AGLGlitch* UGLGlitchSubsystem::Present(FName Placement)
{
	FGLGlitchRecord* Record = Records.Find(Placement);
	if (!Record || Record->Actor.IsValid())
	{
		return nullptr;
	}
	AGLGlitch* Glitch = GetWorld()->SpawnActor<AGLGlitch>(Record->Location, FRotator(0.0, Record->Yaw, 0.0));
	if (!Glitch || !Glitch->GetGlitch()->Setup(Record->Glitch, Placement, Record->Bindings))
	{
		UE_LOG(LogGridlands, Error, TEXT("%s: could not present glitch %s"), *Placement.ToString(), *Record->Glitch.ToString());
		if (Glitch)
		{
			Glitch->Destroy();
		}
		return nullptr;
	}
	Record->Actor = Glitch;
	Glitch->GetGlitch()->PresentFromModel(Record->State, Record->ProgressSeconds, Record->bItemsDelivered, FGLPresentAuthority());
	Register(Glitch);
	return Glitch;
}

TArray<AGLGlitch*> UGLGlitchSubsystem::RemoveCell(FName Cell)
{
	TArray<AGLGlitch*> Retired;
	for (auto It = Records.CreateIterator(); It; ++It)
	{
		if (It.Value().Cell == Cell)
		{
			if (AGLGlitch* Actor = It.Value().Actor.Get())
			{
				Glitches.Remove(Actor); // out of scans, repairs and requirement evaluation at once
				Retired.Add(Actor);
			}
			It.RemoveCurrent();
		}
	}
	Compact();
	return Retired;
}

bool UGLGlitchSubsystem::RestoreRecord(FName Placement, EGLGlitchState State, double ProgressSeconds, bool bItemsDelivered, const FGLRestoreAuthority& Authority)
{
	FGLGlitchRecord* Record = Records.Find(Placement);
	if (!Record || State == EGLGlitchState::Repairing || State >= EGLGlitchState::Count)
	{
		return false;
	}
	const FGLGlitchDef* Def = GLContent::Get().Find<FGLGlitchDef>(Record->Glitch);
	Record->State = State;
	Record->ProgressSeconds = FMath::Clamp(ProgressSeconds, 0.0, Def ? Def->Repair.Seconds : 0.0);
	Record->bItemsDelivered = bItemsDelivered;
	if (AGLGlitch* Actor = Record->Actor.Get())
	{
		return Actor->GetGlitch()->RestoreFromSave(State, ProgressSeconds, bItemsDelivered, Authority);
	}
	return true;
}

void UGLGlitchSubsystem::SyncFromComponent(const UGLGlitchComponent& Component)
{
	if (FGLGlitchRecord* Record = Records.Find(Component.GetPlacementId()))
	{
		Record->State = Component.GetState();
		Record->ProgressSeconds = Component.GetProgressSeconds();
		Record->bItemsDelivered = Component.AreItemsDelivered();
	}
}

AGLGlitch* UGLGlitchSubsystem::FindByPlacement(FName PlacementId) const
{
	for (const TWeakObjectPtr<AGLGlitch>& Glitch : Glitches)
	{
		if (Glitch.IsValid() && Glitch->GetGlitch()->GetPlacementId() == PlacementId)
		{
			return Glitch.Get();
		}
	}
	return nullptr;
}

TArray<AGLGlitch*> UGLGlitchSubsystem::GlitchesNear(const FVector& Origin, double RadiusCm) const
{
	TArray<AGLGlitch*> Near;
	for (const TWeakObjectPtr<AGLGlitch>& Glitch : Glitches)
	{
		if (Glitch.IsValid() && FVector::Dist(Glitch->GetActorLocation(), Origin) <= RadiusCm)
		{
			Near.Add(Glitch.Get());
		}
	}
	Near.Sort([&Origin](const AGLGlitch& A, const AGLGlitch& B) { return FVector::DistSquared(A.GetActorLocation(), Origin) < FVector::DistSquared(B.GetActorLocation(), Origin); });
	return Near;
}

AActor* UGLGlitchSubsystem::GetCommander() const
{
	return CommanderOverride.IsValid() ? CommanderOverride.Get() : UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

void UGLGlitchSubsystem::EvaluateRequirements()
{
	const UGLPlacementSubsystem* Placements = GetWorld()->GetSubsystem<UGLPlacementSubsystem>();
	const AActor* Commander = GetCommander();
	const UGLInventoryComponent* Inventory = Commander ? Commander->FindComponentByClass<UGLInventoryComponent>() : nullptr;
	FGLRequirementFacts Facts;
	Facts.IsSalvaged = [Placements](FName PlacementId) { return Placements && Placements->IsSalvaged(PlacementId); }; // the model, not an actor (P8)
	Facts.CarriedCount = [Inventory](FName Item) { return Inventory ? Inventory->CountOf(Item) : 0; };
	const UGLPuzzleSubsystem* Puzzles = GetWorld()->GetSubsystem<UGLPuzzleSubsystem>();
	Facts.IsPuzzleSolved = [Puzzles](FName Puzzle) { return Puzzles && Puzzles->IsSolved(Puzzle); };

	for (const TWeakObjectPtr<AGLGlitch>& Actor : Glitches)
	{
		UGLGlitchComponent* Glitch = Actor.IsValid() ? Actor->GetGlitch() : nullptr;
		const FGLGlitchDef* Def = Glitch ? GLContent::Get().Find<FGLGlitchDef>(Glitch->GetGlitchId()) : nullptr;
		if (!Def)
		{
			continue;
		}
		const EGLGlitchState State = Glitch->GetState();
		if (State == EGLGlitchState::Latent || State == EGLGlitchState::Repaired)
		{
			continue;
		}
		// Items Pehlichi has already been given stay delivered.
		FGLRequirementFacts GlitchFacts = Facts;
		if (Glitch->AreItemsDelivered())
		{
			GlitchFacts.CarriedCount = [](FName) { return MAX_int32; };
		}
		const bool bMet = GLGlitchRules::RequirementsMet(*Def, Glitch->GetBindings(), GlitchFacts);
		if (State == EGLGlitchState::Repairing)
		{
			if (!bMet)
			{
				Glitch->Interrupt(FGLWorldAuthority());
			}
			continue;
		}
		Glitch->SetRequirementsMet(bMet, FGLWorldAuthority());
	}
}
