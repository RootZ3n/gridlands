// DEV ONLY (P11, ADR-0039): the WINCHESTER / REAL HOUSE-0 proof in the real game (GLWinchesterHouse). Each run writes
// Saved/P11/building-<mode>.json and logs "gl.Building.Proof:" lines; screenshots are taken with HighResShot.
//   gl.Building.Proof build    a fresh world: pad, base (claim, stocked crates), FRAME (with the snapped 45 degree bay),
//                              sawing, FINISH; screenshots of the framing and of the finished house; quits (autosave)
//   gl.Building.Proof restart  after a restart: the house is back exactly; streamed away and back it is still exactly
//                              the same; the porch post comes out (the roof stands), then the Roman column (the roof
//                              falls); quits MID-FALL (the autosave keeps the fall)
//   gl.Building.Proof resume   the fall resumes and lands once; its debris salvages into scrap; a careful dismantle
//                              (intact studs) and a smash (degraded); quits

#include "Building/GLBuildingSubsystem.h"
#include "Character/GLCharacter.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GridlandsGame.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "World/GLGridSubsystem.h"
#include "World/GLWinchesterHouse.h"

#if !UE_BUILD_SHIPPING

namespace
{
	const FName BPOrigin(TEXT("cell.home.origin"));
	const FVector BPAnchor(18000.0, 18000.0, 0.0); // a flat-enough open site in the home cell (the grid test proves it too)
	const FVector BPFarAway(120000.0, 18000.0, 0.0);

	struct FBuildingProofRun
	{
		FString Mode;
		TWeakObjectPtr<UWorld> World;
		int32 Phase = 0;
		double PhaseAt = 0.0;
		double Started = 0.0;
		FGLWinchesterHouse House;
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		FTSTicker::FDelegateHandle Ticker;
		int32 Shots = 0;
		int32 ImpactsAtStart = 0;
	};
	TSharedPtr<FBuildingProofRun> BPRun;

	FString BPDir() { return FPaths::ProjectSavedDir() / TEXT("P11"); }

	AGLCharacter* BPZenny(UWorld* World) { return Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)); }

	void BPPhase(UWorld* World, const FString& Why)
	{
		++BPRun->Phase;
		BPRun->PhaseAt = World->GetTimeSeconds();
		UE_LOG(LogGridlands, Log, TEXT("gl.Building.Proof: %s phase %d: %s"), *BPRun->Mode, BPRun->Phase, *Why);
	}

	/** Stands Zenny (movement held) and aims the camera at a point: the evidence frames. */
	void BPShot(UWorld* World, const FVector& Stand, const FVector& LookAt, const TCHAR* Name)
	{
		AGLCharacter* Zenny = BPZenny(World);
		Zenny->GetCharacterMovement()->DisableMovement();
		Zenny->SetActorLocation(Stand, false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* Controller = Zenny->GetController())
		{
			Controller->SetControlRotation((LookAt - Stand).Rotation());
		}
		// The camera sits behind Zenny: he is hidden for the frame so the architecture is what is seen.
		Zenny->SetActorHiddenInGame(true);
		GEngine->DeferredCommands.Add(TEXT("HighResShot 1600x900"));
		UE_LOG(LogGridlands, Log, TEXT("gl.Building.Proof: %s screenshot %d %s"), *BPRun->Mode, BPRun->Shots++, Name);
	}

	/** Holds Zenny where the house is built (inside the claim), with movement held while the ground is ready. */
	void BPHold(UWorld* World, const FVector& Where)
	{
		AGLCharacter* Zenny = BPZenny(World);
		Zenny->GetCharacterMovement()->DisableMovement();
		Zenny->SetActorLocation(Where + FVector(0, 0, 150), false, nullptr, ETeleportType::TeleportPhysics);
	}

	void BPWrite()
	{
		TArray<TSharedPtr<FJsonValue>> Checks;
		for (const FGLWinchesterCheck& Check : BPRun->House.Checks)
		{
			TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
			C->SetStringField(TEXT("check"), Check.Name);
			C->SetBoolField(TEXT("pass"), Check.bPass);
			C->SetStringField(TEXT("detail"), Check.Detail);
			Checks.Add(MakeShared<FJsonValueObject>(C));
		}
		BPRun->Out->SetStringField(TEXT("mode"), BPRun->Mode);
		BPRun->Out->SetArrayField(TEXT("checks"), Checks);
		BPRun->Out->SetBoolField(TEXT("pass"), BPRun->House.AllPassed());
		BPRun->Out->SetNumberField(TEXT("placements"), BPRun->House.Placements);
		BPRun->Out->SetNumberField(TEXT("previewMismatches"), BPRun->House.PreviewMismatches);
		TSharedRef<FJsonObject> Ids = MakeShared<FJsonObject>();
		for (const TPair<FName, int32>& Id : BPRun->House.Ids)
		{
			Ids->SetNumberField(Id.Key.ToString(), Id.Value);
		}
		BPRun->Out->SetObjectField(TEXT("ids"), Ids);
		TArray<TSharedPtr<FJsonValue>> Bay;
		for (const int32 Id : BPRun->House.BayIds)
		{
			Bay.Add(MakeShared<FJsonValueNumber>(Id));
		}
		BPRun->Out->SetArrayField(TEXT("bay"), Bay);
		FString Text;
		FJsonSerializer::Serialize(BPRun->Out, TJsonWriterFactory<>::Create(&Text));
		IFileManager::Get().MakeDirectory(*BPDir(), true);
		FFileHelper::SaveStringToFile(Text, *(BPDir() / FString::Printf(TEXT("building-%s.json"), *BPRun->Mode)));
		UE_LOG(LogGridlands, Log, TEXT("gl.Building.Proof: %s RESULT %s (%d checks)"), *BPRun->Mode, BPRun->House.AllPassed() ? TEXT("PASS") : TEXT("FAIL"), BPRun->House.Checks.Num());
	}

	/** The previous run's ids and fingerprint (the house's identity across processes). */
	bool BPReadPrevious(const TCHAR* Mode, FString& OutFingerprint)
	{
		FString Text;
		TSharedPtr<FJsonObject> O;
		if (!FFileHelper::LoadFileToString(Text, *(BPDir() / FString::Printf(TEXT("building-%s.json"), Mode))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O)
		{
			return false;
		}
		OutFingerprint = O->GetStringField(TEXT("fingerprintAtEnd"));
		for (const auto& Id : O->GetObjectField(TEXT("ids"))->Values)
		{
			BPRun->House.Ids.Add(FName(*Id.Key), static_cast<int32>(Id.Value->AsNumber()));
		}
		for (const TSharedPtr<FJsonValue>& Id : O->GetArrayField(TEXT("bay")))
		{
			BPRun->House.BayIds.Add(static_cast<int32>(Id->AsNumber()));
		}
		return true;
	}

	bool BPCellReady(UWorld* World)
	{
		const UGLGridSubsystem* Grid = World->GetSubsystem<UGLGridSubsystem>();
		const UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
		return Grid && Grid->IsComplete(BPOrigin) && Structures && Structures->IsCellPresented(BPOrigin);
	}

	void BPQuit(UWorld* World)
	{
		BPWrite();
		FTSTicker::GetCoreTicker().RemoveTicker(BPRun->Ticker);
		GEngine->DeferredCommands.Add(TEXT("quit")); // quitting autosaves
	}

	bool BPTick(float)
	{
		UWorld* World = BPRun.IsValid() ? BPRun->World.Get() : nullptr;
		AGLCharacter* Zenny = World ? BPZenny(World) : nullptr;
		if (!Zenny)
		{
			return true;
		}
		FGLWinchesterHouse& House = BPRun->House;
		UGLStructureSubsystem* Structures = World->GetSubsystem<UGLStructureSubsystem>();
		const double Since = World->GetTimeSeconds() - BPRun->PhaseAt;
		if (World->GetTimeSeconds() - BPRun->Started > 240.0)
		{
			House.Note(TEXT("finished in time"), false, FString::Printf(TEXT("stuck in phase %d"), BPRun->Phase));
			BPQuit(World);
			return false;
		}
		if (BPRun->Mode == TEXT("build"))
		{
			switch (BPRun->Phase)
			{
			case 0:
				if (BPCellReady(World))
				{
					BPHold(World, BPAnchor + FVector(-500, 0, World->GetSubsystem<UGLTerrainSubsystem>()->HeightAt(FVector2D(BPAnchor))));
					BPPhase(World, TEXT("home cell complete: building"));
				}
				return true;
			case 1:
				House.Anchor = BPAnchor;
				House.PreparePad(World);
				House.EstablishBase(World, Zenny);
				House.Frame(World, Zenny);
				House.Saw(World, Zenny);
				BPShot(World, House.At(150, -1050, 330), House.At(150, 0, 140), TEXT("frame-south"));
				BPPhase(World, TEXT("framed"));
				return true;
			case 2:
				if (Since > 2.0)
				{
					BPShot(World, House.At(-950, 700, 420), House.At(150, 0, 140), TEXT("frame-northwest"));
					BPPhase(World, TEXT("framing inspected"));
				}
				return true;
			case 3:
				if (Since > 2.0)
				{
					House.Finish(World, Zenny);
					BPShot(World, House.At(150, -1050, 330), House.At(150, 0, 140), TEXT("finished-south"));
					BPPhase(World, TEXT("finished"));
				}
				return true;
			case 4:
				if (Since > 2.0)
				{
					BPShot(World, House.At(-30, -650, 230), House.At(0, -260, 110), TEXT("bay-closeup"));
					BPPhase(World, TEXT("bay"));
				}
				return true;
			case 5:
				if (Since > 2.0)
				{
					BPShot(World, House.At(1300, -1100, 450), House.At(500, -250, 150), TEXT("porch-and-log-wing"));
					BPPhase(World, TEXT("porch"));
				}
				return true;
			default:
				if (Since > 2.0)
				{
					BPRun->Out->SetStringField(TEXT("fingerprintAtEnd"), House.Fingerprint(World));
					BPHold(World, House.At(-500, 0));
					BPQuit(World);
					return false;
				}
				return true;
			}
		}
		if (BPRun->Mode == TEXT("restart"))
		{
			switch (BPRun->Phase)
			{
			case 0:
				if (BPCellReady(World))
				{
					FString Before;
					const bool bRead = BPReadPrevious(TEXT("build"), Before);
					House.Anchor = BPAnchor;
					House.Anchor.Z = World->GetSubsystem<UGLTerrainSubsystem>()->HeightAt(FVector2D(BPAnchor));
					const FString Now = House.Fingerprint(World);
					House.Note(TEXT("after a restart the house is back exactly (pieces, yaw, layers, contents, owners)"), bRead && Now == Before, Now);
					BPRun->Out->SetStringField(TEXT("fingerprintAfterRestart"), Now);
					BPHold(World, BPFarAway);
					BPPhase(World, TEXT("streaming away"));
				}
				return true;
			case 1:
				if (!World->GetSubsystem<UGLGridSubsystem>()->IsLoaded(BPOrigin))
				{
					House.Note(TEXT("streamed away: the home cell (and the house) unloaded"), Structures->PlayerPieces(BPOrigin).Num() == 0);
					BPHold(World, House.At(-500, 0));
					BPPhase(World, TEXT("coming back"));
				}
				return true;
			case 2:
				if (BPCellReady(World))
				{
					const FString Now = House.Fingerprint(World);
					House.Note(TEXT("streamed back: still exactly the same house"), Now == BPRun->Out->GetStringField(TEXT("fingerprintAfterRestart")), Now);
					BPShot(World, House.At(1300, -1100, 450), House.At(500, -250, 150), TEXT("porch-before-collapse"));
					BPPhase(World, TEXT("porch"));
				}
				return true;
			case 3:
				if (Since > 1.5)
				{
					House.PorchCollapse(World, Zenny);
					BPShot(World, House.At(1300, -1100, 450), House.At(500, -250, 150), TEXT("porch-mid-fall"));
					House.Note(TEXT("quitting mid-fall"), Structures->ActiveCollapses() == 1, FString::Printf(TEXT("%d in flight"), Structures->ActiveCollapses()));
					BPRun->Out->SetStringField(TEXT("fingerprintAtEnd"), House.Fingerprint(World));
					BPQuit(World);
					return false;
				}
				return true;
			}
			return true;
		}
		// resume
		switch (BPRun->Phase)
		{
		case 0:
			if (BPCellReady(World))
			{
				FString Before;
				BPReadPrevious(TEXT("restart"), Before);
				House.Anchor = BPAnchor;
				House.Anchor.Z = World->GetSubsystem<UGLTerrainSubsystem>()->HeightAt(FVector2D(BPAnchor));
				House.Note(TEXT("the fall resumes after the restart"), Structures->ActiveCollapses() == 1, FString::Printf(TEXT("%d in flight"), Structures->ActiveCollapses()));
				BPRun->ImpactsAtStart = Structures->ImpactCount();
				BPHold(World, House.At(500, -800));
				BPPhase(World, TEXT("waiting for the impact"));
			}
			return true;
		case 1:
			if (Structures->ActiveCollapses() == 0 && Since > 1.0)
			{
				House.Note(TEXT("it lands exactly once"), Structures->ImpactCount() == BPRun->ImpactsAtStart + 1, FString::Printf(TEXT("%d impact(s)"), Structures->ImpactCount() - BPRun->ImpactsAtStart));
				BPShot(World, House.At(1300, -1100, 450), House.At(500, -300, 50), TEXT("porch-debris"));
				BPPhase(World, TEXT("landed"));
			}
			return true;
		case 2:
			if (Since > 2.0)
			{
				const int32 Roof = House.Ids.FindRef(TEXT("porch_roof"));
				AGLStructurePart* Debris = Cast<AGLStructurePart>(World->GetSubsystem<UGLBuildingSubsystem>()->FindActor(Roof));
				UGLInventoryComponent* Inventory = Zenny->GetInventory();
				const int32 Scrap = Inventory->CountOf(TEXT("item.material.scrap_timber")), Studs = Inventory->CountOf(TEXT("item.component.stud"));
				int32 Hits = 0;
				while (Debris && !Debris->GetSalvageable()->IsSalvaged() && Hits++ < 60)
				{
					Debris->GetSalvageable()->Interact(Zenny, FGameplayTag::RequestGameplayTag(TEXT("Interact.Salvage")));
				}
				House.Note(TEXT("collapse debris salvages into scrap, not pristine components"), Debris && Debris->GetSalvageable()->IsSalvaged()
					&& Inventory->CountOf(TEXT("item.material.scrap_timber")) - Scrap >= 4 && Inventory->CountOf(TEXT("item.component.stud")) == Studs,
					FString::Printf(TEXT("scrap +%d, studs +%d"), Inventory->CountOf(TEXT("item.material.scrap_timber")) - Scrap, Inventory->CountOf(TEXT("item.component.stud")) - Studs));
				House.Salvage(World, Zenny);
				BPShot(World, House.At(150, -1050, 330), House.At(150, 0, 140), TEXT("after-salvage-south"));
				BPPhase(World, TEXT("salvaged"));
			}
			return true;
		default:
			if (Since > 2.0)
			{
				BPRun->Out->SetStringField(TEXT("fingerprintAtEnd"), House.Fingerprint(World));
				BPHold(World, House.At(-500, 0));
				BPQuit(World);
				return false;
			}
			return true;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs BuildingProofCommand(TEXT("gl.Building.Proof"),
		TEXT("DEV ONLY (P11): build | restart | resume. The WINCHESTER / REAL HOUSE-0 proof; writes Saved/P11/building-<mode>.json and quits (autosave)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			BPRun = MakeShared<FBuildingProofRun>();
			BPRun->Mode = Args.Num() > 0 ? Args[0] : TEXT("build");
			BPRun->World = World;
			BPRun->Started = World->GetTimeSeconds();
			BPRun->PhaseAt = BPRun->Started;
			BPRun->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&BPTick));
		}));
}

#endif
