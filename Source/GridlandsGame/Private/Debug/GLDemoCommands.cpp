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
#include "AIController.h"
#include "Building/GLBuildingSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "TimerManager.h"
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

	/** Builds the 4 m timber shelter through the real transaction (dev: grants the style and planks). */
	void BuildShelter(UWorld* World)
	{
		AGLCharacter* Zenny = Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		UGLBuildingSubsystem* Building = World->GetSubsystem<UGLBuildingSubsystem>();
		if (!Zenny || !Building)
		{
			return;
		}
		World->GetSubsystem<UGLKnowledgeSubsystem>()->Learn(TEXT("knowledge.style.modern_timber_frame"));
		Zenny->GetInventory()->AddItem(TEXT("item.material.timber_plank"), 30);
		Zenny->GetInventory()->AddItem(TEXT("item.material.timber_plank"), 2);
		const FVector C(2200, -1800, 0);
		const FName Floor(TEXT("buildpiece.modern.timber_foundation")), Wall(TEXT("buildpiece.modern.timber_wall")),
			Door(TEXT("buildpiece.modern.timber_doorway")), Roof(TEXT("buildpiece.modern.timber_roof"));
		const double G = World->GetSubsystem<UGLTerrainSubsystem>()->HeightAt(FVector2D(C));
		const TArray<FGLPlacedPiece> Plan = {
			{ 0, Floor, C + FVector(-100, -100, G) }, { 0, Floor, C + FVector(100, -100, G) }, { 0, Floor, C + FVector(-100, 100, G) }, { 0, Floor, C + FVector(100, 100, G) },
			{ 0, Wall, C + FVector(-100, -200, G + 30) }, { 0, Door, C + FVector(100, -200, G + 30) }, { 0, Wall, C + FVector(-100, 200, G + 30) }, { 0, Wall, C + FVector(100, 200, G + 30) },
			{ 0, Wall, C + FVector(-200, -100, G + 30), 1 }, { 0, Wall, C + FVector(-200, 100, G + 30), 1 }, { 0, Wall, C + FVector(200, -100, G + 30), 1 }, { 0, Wall, C + FVector(200, 100, G + 30), 1 },
			{ 0, Roof, C + FVector(-100, -100, G + 280) }, { 0, Roof, C + FVector(100, -100, G + 280) }, { 0, Roof, C + FVector(-100, 100, G + 280), 2 }, { 0, Roof, C + FVector(100, 100, G + 280), 2 },
		};
		int32 Placed = 0;
		for (const FGLPlacedPiece& Piece : Plan)
		{
			const FGLBuildCheck Result = Building->Place(Zenny, Piece);
			Placed += Result.IsAllowed() ? 1 : 0;
			if (!Result.IsAllowed())
			{
				UE_LOG(LogGridlands, Warning, TEXT("gl.Demo.BuildShelter: %s refused: %s"), *Piece.Def.ToString(), *Result.Reason);
			}
		}
		UE_LOG(LogGridlands, Log, TEXT("gl.Demo.BuildShelter: %d/%d pieces placed, planks left %d"), Placed, Plan.Num(), Zenny->GetInventory()->CountOf(TEXT("item.material.timber_plank")));
	}

	FAutoConsoleCommandWithWorld BuildShelterCommand(
		TEXT("gl.Demo.BuildShelter"),
		TEXT("DEV ONLY: grants timber knowledge and planks, then builds a 4 m shelter near the start through the real building transaction."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&BuildShelter));

	/** Digs a pit and raises a mound beside the shelter with a (granted) shovel, through the real terraform rules. */
	void Terraform(UWorld* World)
	{
		AGLCharacter* Zenny = Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
		if (!Zenny || !Terrain)
		{
			return;
		}
		Zenny->GetInventory()->AddItem(TEXT("item.tool.shovel"), 1);
		int32 Dug = 0, Raised = 0;
		for (int32 Stroke = 0; Stroke < 4; ++Stroke)
		{
			Dug += Terrain->Terraform(Zenny, TEXT("terraform.shovel.dig"), FVector2D(1400, -1800)).bApplied ? 1 : 0;
			Raised += Terrain->Terraform(Zenny, TEXT("terraform.shovel.raise"), FVector2D(3000, -1800)).bApplied ? 1 : 0;
		}
		const bool bUnder = Terrain->Terraform(Zenny, TEXT("terraform.shovel.dig"), FVector2D(2200, -1800)).bApplied;
		UE_LOG(LogGridlands, Log, TEXT("gl.Demo.Terraform: dug %d, raised %d, soil left %d; pit %.0f cm, mound %.0f cm; dig under shelter applied=%d"),
			Dug, Raised, Zenny->GetInventory()->CountOf(TEXT("item.material.soil")), Terrain->HeightAt(FVector2D(1400, -1800)), Terrain->HeightAt(FVector2D(3000, -1800)), bUnder ? 1 : 0);
	}

	FAutoConsoleCommandWithWorld TerraformCommand(
		TEXT("gl.Demo.Terraform"),
		TEXT("DEV ONLY: grants a shovel, digs a pit and raises a mound beside the shelter, and tries to dig under it."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Terraform));

	/**
	 * M10 navigation gate in the real game world: raise a ridge across a straight route in the open
	 * field, then send an AI walker; log where it crosses the ridge line and whether it arrives.
	 */
	struct FNavProof
	{
		int32 Phase = 0; // 0 wait for navmesh, 1 wait for rebuild, 2 walking
		int32 Ticks = 0;
		TWeakObjectPtr<ACharacter> Walker;
		TWeakObjectPtr<AAIController> Brain;
		TArray<FVector> Trail;
		FTimerHandle Timer;
	};
	FNavProof Proof;

	/** Where a polyline first crosses x = X (its y there), or -1e9. */
	double CrossingAt(const TArray<FVector>& Points, double X)
	{
		for (int32 I = 1; I < Points.Num(); ++I)
		{
			const FVector& P = Points[I - 1];
			const FVector& Q = Points[I];
			if ((P.X - X) * (Q.X - X) <= 0.0 && P.X != Q.X)
			{
				return FMath::Lerp(P.Y, Q.Y, (X - P.X) / (Q.X - P.X));
			}
		}
		return -1e9;
	}

	void NavProof(UWorld* World)
	{
		UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!Terrain || !Nav)
		{
			UE_LOG(LogGridlands, Warning, TEXT("gl.Demo.NavProof: no terrain or navigation"));
			return;
		}
		Proof = FNavProof();
		World->GetTimerManager().SetTimer(Proof.Timer, FTimerDelegate::CreateLambda([World, Terrain, Nav]()
		{
			const FVector A(1500, -8000, 0), B(5000, -8000, 0);
			constexpr double RidgeX = 3250.0, GapFromY = -6000.0;
			++Proof.Ticks;
			if (Proof.Phase == 0 || Proof.Phase == 1)
			{
				if (Nav->IsNavigationBuildInProgress() || Nav->HasDirtyAreasQueued())
				{
					return;
				}
				UNavigationPath* Path = Nav->FindPathToLocationSynchronously(World, A, B);
				if (!Path || !Path->IsValid() || Path->IsPartial())
				{
					if (Proof.Ticks > 120)
					{
						UE_LOG(LogGridlands, Warning, TEXT("gl.Demo.NavProof: FAIL - no complete path (phase %d)"), Proof.Phase);
						World->GetTimerManager().ClearTimer(Proof.Timer);
					}
					return;
				}
				const double Crossing = CrossingAt(Path->PathPoints, RidgeX);
				if (Proof.Phase == 0)
				{
					UE_LOG(LogGridlands, Log, TEXT("gl.Demo.NavProof: before: route crosses x = %.0f at y = %.0f (straight)"), RidgeX, Crossing);
					for (double Y = -12000.0; Y <= GapFromY - 150.0; Y += 150.0)
					{
						FGLTerrainEdit Raise;
						Raise.Op = EGLTerrainOp::Raise;
						Raise.Centre = FVector2D(RidgeX, Y);
						Raise.RadiusCm = 200.0;
						Raise.AmountCm = 300.0;
						Terrain->ApplyEdit(Raise);
					}
					UE_LOG(LogGridlands, Log, TEXT("gl.Demo.NavProof: ridge raised to %.0f cm across the old route"), Terrain->HeightAt(FVector2D(RidgeX, A.Y)));
					Proof.Phase = 1;
					return;
				}
				UE_LOG(LogGridlands, Log, TEXT("gl.Demo.NavProof: after: route crosses x = %.0f at y = %.0f (gap from %.0f)"), RidgeX, Crossing, GapFromY);
				ACharacter* Walker = World->SpawnActor<ACharacter>(A + FVector(0, 0, 120), FRotator::ZeroRotator);
				Walker->AIControllerClass = AAIController::StaticClass();
				Walker->SpawnDefaultController();
				Proof.Walker = Walker;
				Proof.Brain = Cast<AAIController>(Walker->GetController());
				Proof.Brain->MoveToLocation(B, 100.f);
				Proof.Phase = 2;
				Proof.Ticks = 0;
				return;
			}
			if (!Proof.Walker.IsValid() || !Proof.Brain.IsValid())
			{
				return;
			}
			Proof.Trail.Add(Proof.Walker->GetActorLocation());
			if (Proof.Brain->GetMoveStatus() == EPathFollowingStatus::Moving && Proof.Ticks < 240)
			{
				return;
			}
			double MaxZ = -1e9;
			for (const FVector& P : Proof.Trail)
			{
				MaxZ = FMath::Max(MaxZ, P.Z);
			}
			const double CrossY = CrossingAt(Proof.Trail, RidgeX);
			const double Remaining = FVector::Dist2D(Proof.Walker->GetActorLocation(), B);
			const bool bPass = Remaining < 200.0 && CrossY > GapFromY - 100.0 && MaxZ < 250.0;
			UE_LOG(LogGridlands, Log, TEXT("gl.Demo.NavProof: %s - the AI walker arrived %.0f cm from B after %.1f s, crossed the ridge line at y = %.0f (gap from %.0f), max z %.0f"),
				bPass ? TEXT("PASS") : TEXT("FAIL"), Remaining, Proof.Trail.Num() * 0.25, CrossY, GapFromY, MaxZ);
			World->GetTimerManager().ClearTimer(Proof.Timer);
		}), 0.25f, true, 0.5f);
	}

	FAutoConsoleCommandWithWorld NavProofCommand(
		TEXT("gl.Demo.NavProof"),
		TEXT("DEV ONLY: raises a ridge across a route in the open field and sends an AI walker; logs PASS/FAIL (M10 navigation gate)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&NavProof));

	/** Puts Zenny at X Y (on the ground) facing Yaw, with the camera behind him (evidence framing). */
	void PlaceZenny(const TArray<FString>& Args, UWorld* World)
	{
		AGLCharacter* Zenny = Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		if (!Zenny || Args.Num() < 3)
		{
			UE_LOG(LogGridlands, Warning, TEXT("gl.Demo.PlaceZenny X Y Yaw [Pitch]"));
			return;
		}
		const double X = FCString::Atod(*Args[0]), Y = FCString::Atod(*Args[1]);
		const UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
		const double Z = (Terrain && Terrain->HasGround() ? Terrain->HeightAt(FVector2D(X, Y)) : 0.0) + 100.0;
		Zenny->SetActorLocation(FVector(X, Y, Z), false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* Controller = Zenny->GetController())
		{
			Controller->SetControlRotation(FRotator(Args.Num() > 3 ? FCString::Atod(*Args[3]) : -15.0, FCString::Atod(*Args[2]), 0.0));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs PlaceZennyCommand(
		TEXT("gl.Demo.PlaceZenny"),
		TEXT("DEV ONLY: gl.Demo.PlaceZenny X Y Yaw [Pitch] - moves Zenny onto the ground there (evidence framing)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PlaceZenny));

	FAutoConsoleCommandWithWorld RepairNearbyCommand(
		TEXT("gl.Demo.RepairNearby"),
		TEXT("DEV ONLY: clears blockers, then has Pehlichi scan and repair every glitch through the real loop (fast-forwarded)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&RepairNearby));
}

#endif
