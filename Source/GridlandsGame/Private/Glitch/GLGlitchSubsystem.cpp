#include "Glitch/GLGlitchSubsystem.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchRules.h"
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
	Facts.IsSalvaged = [Placements](FName PlacementId)
	{
		const AGLSalvageNode* Node = Placements ? Placements->FindSalvageNode(PlacementId) : nullptr;
		return Node && Node->GetSalvageable()->IsSalvaged();
	};
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
