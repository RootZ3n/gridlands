// Development-only console commands for demos and rendered evidence. They drive the REAL loop:
// salvage through the salvageable component, items through the inventory, and repairs through
// Pehlichi's own components and passkeys. Nothing here bypasses an authority. Not in shipping builds.

#include "Character/GLCharacter.h"
#include "Engine/World.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
#include "Pehlichi/GLRepairComponent.h"
#include "Pehlichi/GLScanComponent.h"
#include "EngineUtils.h"
#include "Puzzle/GLPuzzleSite.h"
#include "Puzzle/GLPuzzleSubsystem.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "World/GLPlacementSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace GLDemo
{
	void RepairNearby(UWorld* World)
	{
		AGLCharacter* Zenny = Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		AGLPehlichi* Pehlichi = Zenny ? Zenny->GetPehlichi() : nullptr;
		if (!Pehlichi)
		{
			UE_LOG(LogGridlands, Warning, TEXT("gl.Demo.RepairNearby: no Zenny or Pehlichi"));
			return;
		}
		UGLGlitchSubsystem* Glitches = World->GetSubsystem<UGLGlitchSubsystem>();
		UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>();
		const FGameplayTag Salvage = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Salvage"));
		const FTransform ZennyHome = Zenny->GetActorTransform();
		// Two passes: the lamp's scan upgrade makes the level-2 glitch findable on the second.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (const TWeakObjectPtr<AGLGlitch>& Actor : TArray<TWeakObjectPtr<AGLGlitch>>(Glitches->GetAll()))
			{
				AGLGlitch* Glitch = Actor.Get();
				if (!Glitch || Glitch->GetGlitch()->GetState() == EGLGlitchState::Repaired)
				{
					continue;
				}
				// Zenny's part: go there, clear blockers, carry what is needed.
				Zenny->SetActorLocation(Glitch->GetActorLocation() + FVector(0, -300, 100));
				for (const TPair<FString, FName>& Binding : Glitch->GetGlitch()->GetBindings())
				{
					if (AGLSalvageNode* Node = Placements->FindSalvageNode(Binding.Value))
					{
						while (!Node->GetSalvageable()->IsSalvaged())
						{
							Node->GetSalvageable()->Interact(Zenny, Salvage);
						}
					}
				}
				if (Zenny->GetInventory()->CountOf(TEXT("item.part.fuse")) == 0)
				{
					Zenny->GetInventory()->AddItem(TEXT("item.part.fuse"), 1);
				}
				// Pehlichi's part: scan, then repair (fast-forwarded) through his own authority.
				Pehlichi->SetActorLocation(Glitch->GetActorLocation() + FVector(0, -100, 0));
				Pehlichi->GetScan()->Scan();
				Glitches->EvaluateRequirements();
				if (Pehlichi->GetCommands()->Issue(TEXT("Command.Pehlichi.Repair"), Zenny) == EGLCommandRejection::None)
				{
					for (int32 Step = 0; Step < 400 && Pehlichi->GetRepair()->IsWorking(); ++Step)
					{
						Glitches->EvaluateRequirements();
						Pehlichi->GetPositioning()->Advance(0.1f);
						Pehlichi->GetRepair()->Advance(0.1f);
					}
				}
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.RepairNearby: %s -> %s"), *Glitch->GetGlitch()->GetGlitchId().ToString(),
					FGLGlitchLifecycle::StateName(Glitch->GetGlitch()->GetState()));
			}
		}
		Zenny->SetActorTransform(ZennyHome);
		Pehlichi->SetActorLocation(Zenny->GetActorLocation() + FVector(150, 0, -60));
		Pehlichi->GetPositioning()->Follow(Zenny);
	}

	/** Plays the map riddle the way a player would: reveal, ask for hints, fetch the map, place it. */
	void SolveRiddle(UWorld* World)
	{
		AGLCharacter* Zenny = Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		AGLPehlichi* Pehlichi = Zenny ? Zenny->GetPehlichi() : nullptr;
		AGLGlitch* Cartographer = World->GetSubsystem<UGLGlitchSubsystem>()->FindByPlacement(TEXT("placement.origin.glitch_cartographer"));
		AGLSalvageNode* Glovebox = World->GetSubsystem<UGLPlacementSubsystem>()->FindSalvageNode(TEXT("placement.origin.glovebox_01"));
		TActorIterator<AGLPuzzleSite> Site(World);
		UGLPuzzleSubsystem* Puzzles = World->GetSubsystem<UGLPuzzleSubsystem>();
		if (!Pehlichi || !Cartographer || !Glovebox || !Site)
		{
			UE_LOG(LogGridlands, Warning, TEXT("gl.Demo.SolveRiddle: scene incomplete"));
			return;
		}
		Zenny->SetActorLocation(Cartographer->GetActorLocation() + FVector(0, -300, 100));
		Pehlichi->SetActorLocation(Cartographer->GetActorLocation() + FVector(0, -100, 0));
		Pehlichi->GetScan()->Scan();
		for (int32 Ask = 0; Ask < 3; ++Ask)
		{
			UE_LOG(LogGridlands, Log, TEXT("gl.Demo.SolveRiddle: hint request -> tier %d"), Puzzles->RequestHint(Pehlichi));
		}
		Zenny->SetActorLocation(Glovebox->GetActorLocation() + FVector(0, -150, 100));
		const FGameplayTag Salvage = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Salvage"));
		while (!Glovebox->GetSalvageable()->IsSalvaged())
		{
			Glovebox->GetSalvageable()->Interact(Zenny, Salvage);
		}
		Zenny->SetActorLocation(Site->GetActorLocation() + FVector(0, -150, 100));
		const bool bPlaced = Site->Interact(Zenny, UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Present")));
		UE_LOG(LogGridlands, Log, TEXT("gl.Demo.SolveRiddle: placed map=%d solved=%d"), bPlaced ? 1 : 0, Puzzles->IsSolved(Site->GetPuzzleId()) ? 1 : 0);
		Pehlichi->GetPositioning()->Follow(Zenny);
	}

	FAutoConsoleCommandWithWorld SolveRiddleCommand(
		TEXT("gl.Demo.SolveRiddle"),
		TEXT("DEV ONLY: plays the map riddle through the real interactions (scan, hints, glovebox, place the map)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&SolveRiddle));

	FAutoConsoleCommandWithWorld RepairNearbyCommand(
		TEXT("gl.Demo.RepairNearby"),
		TEXT("DEV ONLY: clears blockers, then has Pehlichi scan and repair every glitch through the real loop (fast-forwarded)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&RepairNearby));
}

#endif
