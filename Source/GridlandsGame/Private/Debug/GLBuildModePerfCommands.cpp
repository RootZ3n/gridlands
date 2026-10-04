// P12 (ADR-0040) DEV ONLY: the quiet build-mode measurement. Beside the P11 309-piece player base (-GLPlayerDense), the
// same frames in each build-mode state: normal view, PLACE aimed at a wall top (snap, check, cost plan, ghost), the build
// camera, the browser, REMOVE aimed at a floor (the canonical collapse prediction every frame), and repeated placement.
// Per state: game-thread frame time and the build-mode component's own tick. The <= 2 ms incremental game-thread figure is
// a TARGET (operator), never a budget change. Results: Saved/Perf/buildmode.json, "gl.Perf.BuildMode" log lines.

#include "Building/GLBuildModeComponent.h"
#include "Building/GLBuildingSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/GLCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GridlandsGame.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderCore.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "World/GLGridSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace GLBuildModePerf
{
	const FName BPCell(TEXT("cell.outer.diner_lots"));
	const FVector BPBase(75000.0, -26000.0, 0.0); // the -GLPlayerDense fixture's first unit (a finished floor, four walls, a roof)
	const FName BPWall(TEXT("buildpiece.modern.timber_wall"));
	const FName BPFloor(TEXT("buildpiece.modern.timber_foundation"));
	constexpr double BPSettleSeconds = 2.0;
	constexpr double BPMeasureSeconds = 6.0;

	struct FBPState
	{
		FString Name;
		TFunction<void()> Enter;
		TFunction<FVector()> Aim; // where the camera points every frame (unset: wherever it was)
		TFunction<void(double)> During; // per frame while measuring (repeated placement)
	};

	struct FBPRun
	{
		TWeakObjectPtr<UWorld> World;
		TArray<FBPState> States;
		int32 Index = -1;
		double PhaseAt = 0.0;
		bool bReady = false;
		TArray<double> Game, Tick, Commit;
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		FTSTicker::FDelegateHandle Ticker;
		int32 Placed = 0;
		double NextPlace = 0.0;
		double NormalGame = 0.0;
		TArray<double> SnapMs, CheckMs, CostMs, PredictMs;
	};
	TSharedPtr<FBPRun> BP;

	AGLCharacter* BPZenny() { return BP && BP->World.IsValid() ? Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(BP->World.Get(), 0)) : nullptr; }
	UGLBuildModeComponent* BPBuild() { AGLCharacter* Z = BPZenny(); return Z ? Z->FindComponentByClass<UGLBuildModeComponent>() : nullptr; }
	double BPGround(double X, double Y) { return BP->World->GetSubsystem<UGLTerrainSubsystem>()->HeightAt(FVector2D(X, Y)); }
	FVector BPAt(double X, double Y, double AboveGround) { return FVector(BPBase.X + X, BPBase.Y + Y, BPGround(BPBase.X + X, BPBase.Y + Y) + AboveGround); }

	void BPStand(double X, double Y)
	{
		AGLCharacter* Zenny = BPZenny();
		Zenny->GetCharacterMovement()->DisableMovement();
		Zenny->SetActorLocation(BPAt(X, Y, Zenny->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.0), false, nullptr, ETeleportType::TeleportPhysics);
	}

	void BPPoint(const FVector& Target)
	{
		APlayerController* PC = BP->World->GetFirstPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			PC->SetControlRotation((Target - PC->PlayerCameraManager->GetCameraLocation()).Rotation());
		}
	}

	double Pct(TArray<double> V, double P)
	{
		if (V.Num() == 0)
		{
			return 0.0;
		}
		V.Sort();
		return V[FMath::Clamp(FMath::FloorToInt(V.Num() * P), 0, V.Num() - 1)];
	}

	double Mean(const TArray<double>& V)
	{
		double S = 0.0;
		for (const double X : V) { S += X; }
		return V.Num() ? S / V.Num() : 0.0;
	}

	void BPFinishState()
	{
		const FBPState& State = BP->States[BP->Index];
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("frames"), BP->Game.Num());
		O->SetNumberField(TEXT("gameThreadMsMean"), Mean(BP->Game));
		O->SetNumberField(TEXT("gameThreadMsP95"), Pct(BP->Game, 0.95));
		O->SetNumberField(TEXT("gameThreadMsP99"), Pct(BP->Game, 0.99));
		O->SetNumberField(TEXT("gameThreadMsWorst"), Pct(BP->Game, 1.0));
		O->SetNumberField(TEXT("buildTickMsMean"), Mean(BP->Tick));
		O->SetNumberField(TEXT("buildTickMsP99"), Pct(BP->Tick, 0.99));
		O->SetNumberField(TEXT("buildTickMsWorst"), Pct(BP->Tick, 1.0));
		if (State.Name == TEXT("normal"))
		{
			BP->NormalGame = Mean(BP->Game);
		}
		O->SetNumberField(TEXT("incrementalGameThreadMsMean"), Mean(BP->Game) - BP->NormalGame);
		if (BP->SnapMs.Num())
		{
			// Diagnostic only (this state repeats the view's calls once more each frame): where the PLACE frame goes.
			O->SetNumberField(TEXT("snapMsMean"), Mean(BP->SnapMs));
			O->SetNumberField(TEXT("checkMsMean"), Mean(BP->CheckMs));
			O->SetNumberField(TEXT("costViewMsMean"), Mean(BP->CostMs));
			O->SetNumberField(TEXT("predictionMsMean"), Mean(BP->PredictMs));
			UE_LOG(LogGridlands, Log, TEXT("gl.Perf.BuildMode breakdown: snap %.3f, check %.3f, cost view %.3f, removal prediction %.3f ms (means)"),
				Mean(BP->SnapMs), Mean(BP->CheckMs), Mean(BP->CostMs), Mean(BP->PredictMs));
		}
		if (BP->Commit.Num())
		{
			O->SetNumberField(TEXT("placements"), BP->Commit.Num());
			O->SetNumberField(TEXT("commitMsMean"), Mean(BP->Commit));
			O->SetNumberField(TEXT("commitMsWorst"), Pct(BP->Commit, 1.0));
		}
		const UGLBuildModeComponent* Build = BPBuild();
		O->SetStringField(TEXT("status"), Build ? Build->StatusLine() : FString());
		O->SetNumberField(TEXT("predicted"), Build ? Build->GetView().Predicted.Num() : 0);
		BP->Out->SetObjectField(State.Name, O);
		UE_LOG(LogGridlands, Log, TEXT("gl.Perf.BuildMode %s: GT mean %.2f p99 %.2f worst %.2f ms (incremental %+.2f); build tick mean %.3f p99 %.3f worst %.3f ms%s; %s"),
			*State.Name, Mean(BP->Game), Pct(BP->Game, 0.99), Pct(BP->Game, 1.0), Mean(BP->Game) - BP->NormalGame, Mean(BP->Tick), Pct(BP->Tick, 0.99), Pct(BP->Tick, 1.0),
			BP->Commit.Num() ? *FString::Printf(TEXT("; %d commits, mean %.2f worst %.2f ms"), BP->Commit.Num(), Mean(BP->Commit), Pct(BP->Commit, 1.0)) : TEXT(""),
			Build ? *Build->StatusLine() : TEXT(""));
	}

	void BPQuit()
	{
		BP->Out->SetNumberField(TEXT("pieces"), BP->World->GetSubsystem<UGLBuildingSubsystem>()->GetPieces().Num());
		BP->Out->SetNumberField(TEXT("targetIncrementalGameThreadMs"), 2.0);
		FString Text;
		FJsonSerializer::Serialize(BP->Out, TJsonWriterFactory<>::Create(&Text));
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Perf");
		IFileManager::Get().MakeDirectory(*Dir, true);
		FFileHelper::SaveStringToFile(Text, *(Dir / TEXT("buildmode.json")));
		UE_LOG(LogGridlands, Log, TEXT("gl.Perf.BuildMode done: %s"), *(Dir / TEXT("buildmode.json")));
		FTSTicker::GetCoreTicker().RemoveTicker(BP->Ticker);
		GEngine->DeferredCommands.Add(TEXT("quit"));
	}

	bool BPTick(float)
	{
		UWorld* World = BP.IsValid() ? BP->World.Get() : nullptr;
		if (!World || !BPZenny())
		{
			return true;
		}
		const double Now = World->GetTimeSeconds();
		if (!BP->bReady)
		{
			const UGLGridSubsystem* Grid = World->GetSubsystem<UGLGridSubsystem>();
			BPStand(-350, -350);
			if (Grid->IsComplete(BPCell) && World->GetSubsystem<UGLStructureSubsystem>()->IsCellPresented(BPCell))
			{
				BP->bReady = true;
				GEngine->Exec(World, TEXT("gl.Dev.BuildingStarterKit"));
				BPBuild()->SetProfilePath(FPaths::ProjectSavedDir() / TEXT("Perf") / TEXT("build-profile.json"));
				BP->PhaseAt = Now;
				UE_LOG(LogGridlands, Log, TEXT("gl.Perf.BuildMode: dense base presented (%d pieces)"), World->GetSubsystem<UGLBuildingSubsystem>()->GetPieces().Num());
			}
			else if (Now > 120.0)
			{
				UE_LOG(LogGridlands, Warning, TEXT("gl.Perf.BuildMode: the dense base never presented"));
				BPQuit();
				return false;
			}
			return true;
		}
		if (BP->Index < 0 || Now - BP->PhaseAt >= BPSettleSeconds + BPMeasureSeconds)
		{
			if (BP->Index >= 0)
			{
				BPFinishState();
			}
			if (++BP->Index >= BP->States.Num())
			{
				BPQuit();
				return false;
			}
			BP->Game.Reset();
			BP->Tick.Reset();
			BP->Commit.Reset();
			BP->SnapMs.Reset();
			BP->CheckMs.Reset();
			BP->CostMs.Reset();
			BP->PredictMs.Reset();
			BP->PhaseAt = Now;
			BP->States[BP->Index].Enter();
			UE_LOG(LogGridlands, Log, TEXT("gl.Perf.BuildMode: state %s"), *BP->States[BP->Index].Name);
			return true;
		}
		const FBPState& State = BP->States[BP->Index];
		if (State.Aim)
		{
			BPPoint(State.Aim());
		}
		if (Now - BP->PhaseAt >= BPSettleSeconds)
		{
			BP->Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
			BP->Tick.Add(BPBuild()->GetLastTickMs());
			if (State.During)
			{
				State.During(Now);
			}
		}
		return true;
	}

	void BPStates()
	{
		TArray<FBPState>& S = BP->States;
		auto WallTop = [] { return BPAt(0, -110, 30 + 245); };    // unit 0's south wall, just under its top (the roof's sockets)
		auto Floor = [] { return BPAt(-60, -60, 30); };            // unit 0's floor top inside the walls
		S.Add({ TEXT("normal"), [] { if (BPBuild()->GetMode() != EGLToolMode::None) { BPBuild()->ToggleBuild(); } }, WallTop, {} });
		S.Add({ TEXT("place"), [] { BPBuild()->ToggleBuild(); BPBuild()->SelectPiece(BPWall); }, WallTop, {} });
		S.Add({ TEXT("buildCamera"), [] { BPBuild()->BuildCamera(true); }, WallTop, {} });
		S.Add({ TEXT("browser"), [] { BPBuild()->BuildCamera(false); BPBuild()->ToggleBrowser(); }, WallTop, {} });
		S.Add({ TEXT("removePrediction"), [] { BPBuild()->ToggleBrowser(); BPBuild()->ToggleRemoveMode(); }, Floor, {} });
		S.Add({ TEXT("repeatedPlacement"), []
		{
			BPBuild()->ToggleRemoveMode();
			BPBuild()->SelectPiece(BPFloor);
			BP->Placed = 0;
			BP->NextPlace = 0.0;
		}, [] { return BPAt(-700 + 200.0 * (BP->Placed % 3), -700 - 200.0 * (BP->Placed / 3), 0); }, [](double Now)
		{
			if (BPBuild()->GetLastTickMs() > 5.0)
			{
				UE_LOG(LogGridlands, Log, TEXT("gl.Perf.BuildMode: build tick spike %.2f ms (GT %.2f ms), %d commits so far"), BPBuild()->GetLastTickMs(), FPlatformTime::ToMilliseconds(GGameThreadTime), BP->Placed);
			}
			if (Now < BP->NextPlace || BP->Placed >= 12)
			{
				return;
			}
			BP->NextPlace = Now + 0.4;
			UGLBuildingSubsystem* Building = BP->World->GetSubsystem<UGLBuildingSubsystem>();
			const int32 Before = Building->GetPieces().Num();
			const double Start = FPlatformTime::Seconds();
			BPBuild()->Primary();
			const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
			if (Building->GetPieces().Num() == Before + 1)
			{
				BP->Commit.Add(Ms);
				++BP->Placed;
			}
		} });
		S.Add({ TEXT("placeBreakdown"), [] { BPBuild()->SelectPiece(BPWall); }, WallTop, [](double)
		{
			UGLBuildModeComponent* Build = BPBuild();
			UGLBuildingSubsystem* Building = BP->World->GetSubsystem<UGLBuildingSubsystem>();
			const FGLBuildView& View = Build->GetView();
			FGLPlacedPiece Candidate;
			FGLSnapInfo Info;
			double T = FPlatformTime::Seconds();
			const bool bSnapped = Building->Snap(View.Piece, View.AimPoint, View.YawStep, Candidate, &Info, View.AimEnd - View.AimStart);
			BP->SnapMs.Add((FPlatformTime::Seconds() - T) * 1000.0);
			if (!bSnapped)
			{
				return;
			}
			T = FPlatformTime::Seconds();
			Building->Check(BPZenny(), Candidate);
			BP->CheckMs.Add((FPlatformTime::Seconds() - T) * 1000.0);
			T = FPlatformTime::Seconds();
			Building->CostView(BPZenny(), Candidate.Location, GLContent::Get().Find<FGLBuildPieceDef>(View.Piece)->Cost);
			BP->CostMs.Add((FPlatformTime::Seconds() - T) * 1000.0);
			const TArray<FGLPlacedPiece> Pieces = Building->GetPieces();
			T = FPlatformTime::Seconds();
			Building->PreviewRemoval(Pieces[0].Id);
			BP->PredictMs.Add((FPlatformTime::Seconds() - T) * 1000.0);
		} });
	}

	FAutoConsoleCommandWithWorldAndArgs BuildModePerfCommand(
		TEXT("gl.Perf.BuildMode"),
		TEXT("DEV ONLY (P12): build-mode frame cost beside the dense player base (launch with -GLPlayerDense; quiet machine)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			BP = MakeShared<FBPRun>();
			BP->World = World;
			BPStates();
			BP->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&BPTick));
			UE_LOG(LogGridlands, Log, TEXT("gl.Perf.BuildMode started (%d states)"), BP->States.Num());
		}));
}

#endif // !UE_BUILD_SHIPPING
