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

	FAutoConsoleCommandWithWorld RepairNearbyCommand(
		TEXT("gl.Demo.RepairNearby"),
		TEXT("DEV ONLY: clears blockers, then has Pehlichi scan and repair every glitch through the real loop (fast-forwarded)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&RepairNearby));
}

#endif
