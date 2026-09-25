// Development-only performance harness (P2: full-cell terrain scaling). Builds representative
// authored ground (rolling hills with ridges) at a chosen cell size through the real terrain
// subsystem, then measures build, navigation, rendering, edits, navigation updates, save size and
// reload cost in the running game. Writes Saved/Perf/terrain-<metres>m.json. Not in shipping builds.

#include "Character/GLCharacter.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GridlandsGame.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "RenderTimer.h"
#include "Save/GLWorldSave.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Terrain/GLTerrainChunk.h"
#include "Terrain/GLTerrainSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace GLPerf
{
	struct FFrameStats
	{
		TArray<double> Frame, Game, Render, Gpu;

		void Add()
		{
			Frame.Add(FApp::GetDeltaTime() * 1000.0);
			Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
			Render.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
			Gpu.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
		}

		static double Mean(const TArray<double>& V) { double S = 0; for (double X : V) S += X; return V.Num() ? S / V.Num() : 0.0; }
		static double P95(TArray<double> V) { if (!V.Num()) return 0.0; V.Sort(); return V[FMath::Min(V.Num() - 1, FMath::FloorToInt(V.Num() * 0.95))]; }

		TSharedRef<FJsonObject> Json() const
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetNumberField(TEXT("frames"), Frame.Num());
			O->SetNumberField(TEXT("frameMsMean"), Mean(Frame));
			O->SetNumberField(TEXT("frameMsP95"), P95(Frame));
			O->SetNumberField(TEXT("fpsMean"), Mean(Frame) > 0 ? 1000.0 / Mean(Frame) : 0.0);
			O->SetNumberField(TEXT("gameThreadMsMean"), Mean(Game));
			O->SetNumberField(TEXT("renderThreadMsMean"), Mean(Render));
			O->SetNumberField(TEXT("gpuMsMean"), Mean(Gpu));
			O->SetNumberField(TEXT("gpuMsP95"), P95(Gpu));
			return O;
		}
	};

	struct FRun
	{
		int32 Metres = 0;
		FString Suffix;
		int32 Phase = 0;
		int32 Frames = 0;
		double PhaseStart = 0.0;
		double NavStart = 0.0;
		uint64 MemBefore = 0;
		FFrameStats Ground, Overlook;
		TArray<double> EditMs, NavUpdateSeconds;
		int32 NavProbes = 0;
		FRandomStream Random{ 20260925 };
		TSharedPtr<FJsonObject> Out;
		FTSTicker::FDelegateHandle Ticker;
		TWeakObjectPtr<UWorld> World;
	};
	FRun Run;

	bool NavBusy(UNavigationSystemV1* Nav)
	{
		return Nav && (Nav->IsNavigationBuildInProgress() || Nav->HasDirtyAreasQueued() || Nav->GetNumRemainingBuildTasks() > 0);
	}

	double UsedMb() { return FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0); }

	void Place(UWorld* World, const FVector2D& At, double Yaw, double Pitch)
	{
		APawn* Zenny = UGameplayStatics::GetPlayerPawn(World, 0);
		const UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
		if (Zenny && Terrain)
		{
			Zenny->SetActorLocation(FVector(At.X, At.Y, Terrain->HeightAt(At) + 120.0), false, nullptr, ETeleportType::TeleportPhysics);
			if (AController* C = Zenny->GetController())
			{
				C->SetControlRotation(FRotator(Pitch, Yaw, 0.0));
			}
		}
	}

	void Finish(UWorld* World)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Run.Ticker);
		Run.Out->SetObjectField(TEXT("viewGround"), Run.Ground.Json());
		Run.Out->SetObjectField(TEXT("viewOverlook"), Run.Overlook.Json());
		Run.Out->SetNumberField(TEXT("editMsMean"), FFrameStats::Mean(Run.EditMs));
		Run.Out->SetNumberField(TEXT("editMsP95"), FFrameStats::P95(Run.EditMs));
		Run.Out->SetNumberField(TEXT("edits"), Run.EditMs.Num());
		Run.Out->SetNumberField(TEXT("navUpdateAfterEditSecondsMean"), FFrameStats::Mean(Run.NavUpdateSeconds));
		Run.Out->SetNumberField(TEXT("navUpdateAfterEditSecondsMax"), Run.NavUpdateSeconds.Num() ? FMath::Max(Run.NavUpdateSeconds) : 0.0);

		// Save and reload implications of the edits made.
		UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
		FGLWorldSave Save;
		Terrain->CaptureDelta(Save.TerrainIndices, Save.TerrainDeltaCm);
		const FString Json = GLSaveCodec::ToJson(Save);
		const FString EmptyJson = GLSaveCodec::ToJson(FGLWorldSave());
		Run.Out->SetNumberField(TEXT("saveDeltaVertices"), Save.TerrainIndices.Num());
		Run.Out->SetNumberField(TEXT("saveTerrainBytes"), Json.Len() - EmptyJson.Len());
		const double Load = FPlatformTime::Seconds();
		Terrain->RestoreDelta(Save.TerrainIndices, Save.TerrainDeltaCm);
		Run.Out->SetNumberField(TEXT("reloadAllChunksSeconds"), FPlatformTime::Seconds() - Load);
		Run.Out->SetNumberField(TEXT("memUsedMbAtEnd"), UsedMb());

		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		FJsonSerializer::Serialize(Run.Out.ToSharedRef(), Writer);
		const FString Path = FPaths::ProjectSavedDir() / TEXT("Perf") / FString::Printf(TEXT("terrain-%dm%s.json"), Run.Metres, *Run.Suffix);
		FFileHelper::SaveStringToFile(Text, *Path);
		UE_LOG(LogGridlands, Log, TEXT("gl.Perf.Result %s"), *Text);
		GEngine->DeferredCommands.Add(TEXT("quit"));
	}

	bool Tick(float)
	{
		UWorld* World = Run.World.Get();
		if (!World)
		{
			return false;
		}
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
		const double Now = FPlatformTime::Seconds();
		const double Half = Run.Metres * 50.0;
		++Run.Frames;
		switch (Run.Phase)
		{
		case 0: // full navigation build over the whole cell
			if (Run.Frames > 3 && !NavBusy(Nav))
			{
				Run.Out->SetNumberField(TEXT("navFullBuildSeconds"), Now - Run.NavStart);
				Run.Out->SetNumberField(TEXT("memUsedMbAfterNav"), UsedMb());
				Run.Out->SetNumberField(TEXT("memDeltaMbTerrainAndNav"), UsedMb() - Run.MemBefore / (1024.0 * 1024.0));
				Place(World, FVector2D(-Half + 3000.0, -Half + 3000.0), 45.0, -4.0); // low, looking across the whole cell
				Run.Phase = 1;
				Run.Frames = 0;
			}
			else if (Now - Run.NavStart > 900.0)
			{
				Run.Out->SetNumberField(TEXT("navFullBuildSeconds"), -1.0);
				Run.Phase = 1;
				Run.Frames = 0;
			}
			break;
		case 1:
			if (Run.Frames > 90) { Run.Ground.Add(); }
			if (Run.Frames >= 90 + 400)
			{
				Place(World, FVector2D(0.0, -Half * 0.6), 90.0, -28.0); // an overlook: most of the cell in view, steeply
				Run.Phase = 2;
				Run.Frames = 0;
			}
			break;
		case 2:
			if (Run.Frames > 90) { Run.Overlook.Add(); }
			if (Run.Frames >= 90 + 400)
			{
				Run.Phase = 3;
				Run.Frames = 0;
			}
			break;
		case 3: // runtime edits: dig strokes scattered over the cell (chunk rebuild + collision + nav dirty)
		{
			FGLTerrainEdit Edit;
			Edit.Op = Run.Random.FRand() < 0.5f ? EGLTerrainOp::Dig : EGLTerrainOp::Raise;
			Edit.Centre = FVector2D(Run.Random.FRandRange(-Half * 0.9, Half * 0.9), Run.Random.FRandRange(-Half * 0.9, Half * 0.9));
			Edit.RadiusCm = 150.0;
			Edit.AmountCm = 50.0;
			const double Start = FPlatformTime::Seconds();
			Terrain->ApplyEdit(Edit);
			Run.EditMs.Add((FPlatformTime::Seconds() - Start) * 1000.0);
			if (Run.EditMs.Num() >= 60)
			{
				Run.Phase = 4;
				Run.Frames = 0;
				Run.PhaseStart = Now;
			}
			break;
		}
		case 4: // navigation update latency after a single edit (settle, edit, time until settled again)
			if (NavBusy(Nav))
			{
				break;
			}
			if (Run.NavProbes > 0 && Run.PhaseStart > 0.0)
			{
				Run.NavUpdateSeconds.Add(Now - Run.PhaseStart);
			}
			if (Run.NavProbes >= 8)
			{
				Finish(World);
				return false;
			}
			{
				FGLTerrainEdit Edit;
				Edit.Op = EGLTerrainOp::Dig;
				Edit.Centre = FVector2D(Run.Random.FRandRange(-Half * 0.8, Half * 0.8), Run.Random.FRandRange(-Half * 0.8, Half * 0.8));
				Edit.RadiusCm = 200.0;
				Edit.AmountCm = 100.0;
				Terrain->ApplyEdit(Edit);
				Run.PhaseStart = FPlatformTime::Seconds();
				++Run.NavProbes;
			}
			break;
		}
		return true;
	}

	void Terrain(const TArray<FString>& Args, UWorld* World)
	{
		UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
		if (!Terrain || Args.Num() < 1)
		{
			UE_LOG(LogGridlands, Warning, TEXT("gl.Perf.Terrain Metres [SpacingMetres=1] [ChunkMetres=64]"));
			return;
		}
		Run = FRun();
		Run.World = World;
		Run.Metres = FCString::Atoi(*Args[0]);
		const double Spacing = Args.Num() > 1 ? FCString::Atod(*Args[1]) : 1.0;
		const int32 ChunkMetres = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 64;
		Run.Suffix = (Spacing != 1.0 || ChunkMetres != 64) ? FString::Printf(TEXT("-s%g-c%d"), Spacing, ChunkMetres) : FString();
		const int32 Chunks = FMath::Max(1, FMath::CeilToInt(static_cast<double>(Run.Metres) / ChunkMetres));
		const int32 ChunkVerts = FMath::RoundToInt(ChunkMetres / Spacing) + 1;
		const int32 Verts = Chunks * (ChunkVerts - 1) + 1;
		Run.Metres = Chunks * ChunkMetres;
		const double Half = Run.Metres * 50.0;
		Run.Out = MakeShared<FJsonObject>();
		Run.Out->SetNumberField(TEXT("cellMetres"), Run.Metres);
		Run.Out->SetNumberField(TEXT("spacingMetres"), Spacing);
		Run.Out->SetNumberField(TEXT("chunkMetres"), ChunkMetres);
		Run.Out->SetNumberField(TEXT("chunks"), Chunks * Chunks);
		Run.Out->SetNumberField(TEXT("vertices"), static_cast<double>(Verts) * Verts);
		Run.Out->SetNumberField(TEXT("triangles"), 2.0 * (ChunkVerts - 1) * (ChunkVerts - 1) * Chunks * Chunks);
		Run.Out->SetStringField(TEXT("rhi"), GDynamicRHI ? GDynamicRHI->GetName() : TEXT("none"));
		Run.Out->SetStringField(TEXT("gpu"), GRHIAdapterName);
		Run.Out->SetNumberField(TEXT("resX"), GSystemResolution.ResX);
		Run.Out->SetNumberField(TEXT("resY"), GSystemResolution.ResY);

		Run.MemBefore = FPlatformMemory::GetStats().UsedPhysical;
		const double Gen = FPlatformTime::Seconds();
		TArray<float> Hills = GLTerrainGen::Rolling(20260925, Verts, Verts, Spacing * 100.0, 1500.0);
		Run.Out->SetNumberField(TEXT("generateSeconds"), FPlatformTime::Seconds() - Gen);
		AGLTerrainChunk::MeshSeconds = AGLTerrainChunk::CollisionSeconds = AGLTerrainChunk::NavigationSeconds = 0.0;
		AGLTerrainChunk::Rebuilds = 0;
		const double Build = FPlatformTime::Seconds();
		Terrain->Setup(FVector2D(-Half, -Half), Chunks, Chunks, ChunkVerts, Spacing * 100.0, 0.f, 400.0, 400.0, &Hills);
		Run.Out->SetNumberField(TEXT("buildSeconds"), FPlatformTime::Seconds() - Build);
		Run.Out->SetNumberField(TEXT("buildMeshSeconds"), AGLTerrainChunk::MeshSeconds);
		Run.Out->SetNumberField(TEXT("buildCollisionSeconds"), AGLTerrainChunk::CollisionSeconds);
		Run.Out->SetNumberField(TEXT("heightfieldMb"), Terrain->GetField().GetAllocatedBytes() / (1024.0 * 1024.0));
		Run.Out->SetNumberField(TEXT("memUsedMbBefore"), Run.MemBefore / (1024.0 * 1024.0));
		Run.Out->SetNumberField(TEXT("memUsedMbAfterBuild"), UsedMb());
		Run.NavStart = FPlatformTime::Seconds();
		Run.Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
		UE_LOG(LogGridlands, Log, TEXT("gl.Perf.Terrain: %d m cell, %d chunks, built in %.2f s"), Run.Metres, Chunks * Chunks, FPlatformTime::Seconds() - Build);
	}

	FAutoConsoleCommandWithWorldAndArgs TerrainCommand(
		TEXT("gl.Perf.Terrain"),
		TEXT("DEV ONLY: gl.Perf.Terrain Metres [SpacingMetres] [ChunkMetres] - terrain scaling harness; writes Saved/Perf/terrain-<m>m.json and quits."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Terrain));
}

#endif
