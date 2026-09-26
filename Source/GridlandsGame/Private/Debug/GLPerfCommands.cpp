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
#include "NavMesh/RecastNavMesh.h"
#include "RenderTimer.h"
#include "Save/GLWorldSave.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Terrain/GLTerrainChunk.h"
#include "UObject/GarbageCollection.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "World/GLGridSubsystem.h"
#include "World/GLGridCells.h"

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

	ARecastNavMesh* Recast(UWorld* World)
	{
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		return Nav ? Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
	}

	/** Navigation state for comparing localized (invoker) and whole-cell navigation (ADR-0029). */
	void AddNavigation(UWorld* World, FJsonObject& Out, const TCHAR* Prefix)
	{
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const ARecastNavMesh* R = Recast(World);
		Out.SetBoolField(TEXT("navInvokersOnly"), Nav && Nav->IsActiveTilesGenerationEnabled());
		Out.SetNumberField(FString(Prefix) + TEXT("NavActiveTiles"), R ? R->GetNumActiveTiles() : 0);
		// The engine's memory walk asserts while tile tasks are queued: only read it when idle.
		const bool bIdle = Nav && !Nav->IsNavigationBuildInProgress() && Nav->GetNumRemainingBuildTasks() == 0;
		Out.SetNumberField(FString(Prefix) + TEXT("NavTileMb"), R && bIdle ? R->LogMemUsed() / (1024.0 * 1024.0) : -1.0);
		Out.SetNumberField(FString(Prefix) + TEXT("NavPendingTasks"), Nav ? Nav->GetNumRemainingBuildTasks() : 0);
	}

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
		AddNavigation(World, *Run.Out, TEXT("final"));
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
				AddNavigation(World, *Run.Out, TEXT("initial"));
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
				// Within 40 m of Zenny: an edit the player makes, where navigation exists in both modes.
				const APawn* Zenny = UGameplayStatics::GetPlayerPawn(World, 0);
				const FVector2D Near = Zenny ? FVector2D(Zenny->GetActorLocation()) : FVector2D::ZeroVector;
				Edit.Centre = Near + FVector2D(Run.Random.FRandRange(-4000.0, 4000.0), Run.Random.FRandRange(-4000.0, 4000.0));
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

	/**
	 * Seamless-streaming measurement (P5): Zenny walks across canonical 1 km cells at a set speed
	 * (real engine ticking, real streaming, no teleports) and every frame is recorded.
	 * Modes: straight (origin centre -> deep in the lots), reversal (turn back while the lots load,
	 * then cross), sprint (straight at 3x speed). Writes Saved/Perf/crossing-<mode>.json and quits.
	 */
	struct FCrossing
	{
		FString Mode;
		double Speed = 600.0; // cm/s
		TArray<double> Route; // x waypoints (cm) along y = RouteY
		int32 TeleportLeg = -1; // this leg is a jump (fast travel, respawn), not a walk
		double RouteY = 0.0;
		int32 Leg = 0;
		double X = 0.0;
		int32 Frames = 0;
		int32 Warmup = 0;
		TArray<double> FrameMs, AdvanceMs, GameMs, GpuMs;
		double PeakMb = 0.0;
		int32 PeakNavTiles = 0;
		int32 PeakNavTasks = 0;
		double StartMb = 0.0;
		int32 EmergencyAtStart = 0;
		bool bArrivalLogged = false;
		TArray<TSharedPtr<FJsonValue>> Hitches;
		int32 GcCount = 0;
		int32 GcSeen = 0;
		FString Arrival;
		TWeakObjectPtr<UWorld> World;
		FTSTicker::FDelegateHandle Ticker;
	};
	FCrossing Crossing;

	bool CrossingTick(float Dt)
	{
		UWorld* World = Crossing.World.Get();
		APawn* Zenny = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		UGLTerrainSubsystem* Terrain = World ? World->GetSubsystem<UGLTerrainSubsystem>() : nullptr;
		UGLGridSubsystem* Grid = World ? World->GetSubsystem<UGLGridSubsystem>() : nullptr;
		if (!Zenny || !Terrain || !Grid)
		{
			return false;
		}
		// Wait until the starting cell is complete (startup is not what is being measured), except
		// when resuming a save, where startup is the measurement.
		if (Crossing.Mode != TEXT("resume") && Crossing.Warmup < 1000 && !Grid->IsComplete(TEXT("cell.home.origin")))
		{
			++Crossing.Warmup;
			return true;
		}
		if (Crossing.Frames == 0)
		{
			Crossing.StartMb = FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0);
			Crossing.EmergencyAtStart = Terrain->GetStats().EmergencyChunks;
		}
		++Crossing.Frames;
		if (Crossing.Frames > 5 && FApp::GetDeltaTime() * 1000.0 > 16.7 && Crossing.Hitches.Num() < 60)
		{
			// What was going on in a frame slower than 60 fps (the previous frame's work shows in this delta).
			TSharedRef<FJsonObject> Hitch = MakeShared<FJsonObject>();
			Hitch->SetNumberField(TEXT("frame"), Crossing.Frames);
			Hitch->SetNumberField(TEXT("engineFrame"), static_cast<double>(GFrameCounter));
			Hitch->SetNumberField(TEXT("ms"), FApp::GetDeltaTime() * 1000.0);
			Hitch->SetNumberField(TEXT("xMetres"), Crossing.X / 100.0);
			Hitch->SetNumberField(TEXT("streamingMs"), Grid->GetLastAdvanceSeconds() * 1000.0);
			Hitch->SetNumberField(TEXT("gameThreadMs"), FPlatformTime::ToMilliseconds(GGameThreadTime));
			Hitch->SetNumberField(TEXT("renderThreadMs"), FPlatformTime::ToMilliseconds(GRenderThreadTime));
			Hitch->SetBoolField(TEXT("garbageCollected"), Crossing.GcCount != Crossing.GcSeen);
			Hitch->SetBoolField(TEXT("levelStreamingPending"), World->HasStreamingLevelsToConsider());
			Crossing.Hitches.Add(MakeShared<FJsonValueObject>(Hitch));
		}
		Crossing.GcSeen = Crossing.GcCount;
		if (Crossing.Frames > 5)
		{
			Crossing.FrameMs.Add(FApp::GetDeltaTime() * 1000.0);
			Crossing.AdvanceMs.Add(Grid->GetLastAdvanceSeconds() * 1000.0);
			Crossing.GameMs.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
			Crossing.GpuMs.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
		}
		Crossing.PeakMb = FMath::Max(Crossing.PeakMb, FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0));
		if (Crossing.Frames % 30 == 0)
		{
			if (const ARecastNavMesh* R = Recast(World))
			{
				Crossing.PeakNavTiles = FMath::Max(Crossing.PeakNavTiles, R->GetNumActiveTiles());
			}
			if (const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				Crossing.PeakNavTasks = FMath::Max(Crossing.PeakNavTasks, Nav->GetNumRemainingBuildTasks());
			}
		}
		// Walk (a fixed step per frame at the measured frame time: real speed, no teleport jumps).
		const double Target = Crossing.Route[Crossing.Leg];
		const double Step = Crossing.Leg == Crossing.TeleportLeg ? 1.0e9 : Crossing.Speed * FMath::Min(Dt, 0.1f);
		Crossing.X += FMath::Clamp(Target - Crossing.X, -Step, Step);
		const FVector2D At(Crossing.X, Crossing.RouteY);
		// Where Zenny arrives, the ground must already be there (not built in an emergency).
		if (!Crossing.bArrivalLogged && GLGridCells::CellAt(At) == FName(TEXT("cell.outer.diner_lots")))
		{
			Crossing.bArrivalLogged = true;
			Crossing.Arrival = FString::Printf(TEXT("crossed at frame %d; lots ground %s, lots complete %s, emergency chunks so far %d"),
				Crossing.Frames, Terrain->HasCell(TEXT("cell.outer.diner_lots")) ? TEXT("ready") : TEXT("MISSING"),
				Terrain->IsCellComplete(TEXT("cell.outer.diner_lots")) ? TEXT("yes") : TEXT("no"), Terrain->GetStats().EmergencyChunks - Crossing.EmergencyAtStart);
		}
		Zenny->SetActorLocation(FVector(At, Terrain->HeightAt(At) + 110.0), false, nullptr, ETeleportType::None);
		if (AController* C = Zenny->GetController())
		{
			C->SetControlRotation(FRotator(-8.0, Target >= Crossing.X ? 0.0 : 180.0, 0.0));
		}
		if (FMath::IsNearlyEqual(Crossing.X, Target, 1.0))
		{
			if (++Crossing.Leg >= Crossing.Route.Num())
			{
				// Linger at the end until the new cell is complete (or 20 s).
				static int32 Linger = 0;
				if (++Linger < 2400 && !Grid->IsComplete(TEXT("cell.outer.diner_lots")))
				{
					--Crossing.Leg;
					return true;
				}
				TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
				O->SetStringField(TEXT("mode"), Crossing.Mode);
				O->SetNumberField(TEXT("speedMetresPerSecond"), Crossing.Speed / 100.0);
				O->SetNumberField(TEXT("frames"), Crossing.FrameMs.Num());
				O->SetNumberField(TEXT("worstFrameMs"), Crossing.FrameMs.Num() ? FMath::Max(Crossing.FrameMs) : 0.0);
				O->SetNumberField(TEXT("frameMsMean"), FFrameStats::Mean(Crossing.FrameMs));
				O->SetNumberField(TEXT("frameMsP99"), [&] { TArray<double> V = Crossing.FrameMs; V.Sort(); return V.Num() ? V[FMath::Min(V.Num() - 1, FMath::FloorToInt(V.Num() * 0.99))] : 0.0; }());
				O->SetNumberField(TEXT("streamingGameThreadMsWorst"), Crossing.AdvanceMs.Num() ? FMath::Max(Crossing.AdvanceMs) : 0.0);
				O->SetNumberField(TEXT("streamingGameThreadMsMean"), FFrameStats::Mean(Crossing.AdvanceMs));
				O->SetNumberField(TEXT("gameThreadMsMean"), FFrameStats::Mean(Crossing.GameMs));
				O->SetNumberField(TEXT("gpuMsMean"), FFrameStats::Mean(Crossing.GpuMs));
				O->SetNumberField(TEXT("memStartMb"), Crossing.StartMb);
				O->SetNumberField(TEXT("memPeakMb"), Crossing.PeakMb);
				O->SetNumberField(TEXT("navActiveTilesPeak"), Crossing.PeakNavTiles);
				O->SetNumberField(TEXT("navPendingTasksPeak"), Crossing.PeakNavTasks);
				AddNavigation(World, *O, TEXT("end"));
				O->SetNumberField(TEXT("emergencyChunks"), Terrain->GetStats().EmergencyChunks - Crossing.EmergencyAtStart);
				O->SetNumberField(TEXT("staleResultsDropped"), Terrain->GetStats().StaleDropped);
				O->SetNumberField(TEXT("chunksReused"), Terrain->GetStats().ChunksReused);
				O->SetStringField(TEXT("arrival"), Crossing.Arrival);
				TArray<TSharedPtr<FJsonValue>> Loads;
				for (const FGLCellLoadRecord& R : Grid->GetLoadRecords())
				{
					TSharedRef<FJsonObject> L = MakeShared<FJsonObject>();
					L->SetStringField(TEXT("cell"), R.Cell.ToString());
					L->SetNumberField(TEXT("epoch"), R.Epoch);
					L->SetBoolField(TEXT("cancelledMidLoad"), R.bCancelled);
					L->SetNumberField(TEXT("groundReadySeconds"), R.GroundSeconds);
					L->SetNumberField(TEXT("runtimeReadySeconds"), R.RuntimeSeconds);
					L->SetNumberField(TEXT("cellCompleteSeconds"), R.CompleteSeconds);
					Loads.Add(MakeShared<FJsonValueObject>(L));
				}
				O->SetArrayField(TEXT("cellLoads"), Loads);
				O->SetArrayField(TEXT("hitches"), Crossing.Hitches);
				// Save/restart proof: the teleport run leaves a 2 m mound 5 m ahead in the lots (saved
				// on quit); the resume run reads the ground there once the lots are complete.
				const FVector2D Mark(90500.0, Crossing.RouteY);
				if (Crossing.Mode == TEXT("teleport"))
				{
					O->SetNumberField(TEXT("markBeforeCm"), Terrain->HeightAt(Mark));
					for (int32 I = 0; I < 2; ++I)
					{
						FGLTerrainEdit Raise;
						Raise.Op = EGLTerrainOp::Raise;
						Raise.Centre = Mark;
						Raise.RadiusCm = 300.0;
						Raise.AmountCm = 100.0;
						Terrain->ApplyEdit(Raise);
					}
					O->SetNumberField(TEXT("markAfterCm"), Terrain->HeightAt(Mark));
				}
				else if (Crossing.Mode == TEXT("resume"))
				{
					O->SetNumberField(TEXT("markCm"), Terrain->HeightAt(Mark));
				}
				O->SetNumberField(TEXT("garbageCollections"), Crossing.GcCount);
				FString Text;
				FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Text));
				FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectSavedDir() / TEXT("Perf") / FString::Printf(TEXT("crossing-%s.json"), *Crossing.Mode)));
				UE_LOG(LogGridlands, Log, TEXT("gl.Perf.CrossingResult %s"), *Text);
				GEngine->Exec(World, TEXT("gl.Demo.GridReport"));
				GEngine->DeferredCommands.Add(TEXT("quit"));
				return false;
			}
		}
		return true;
	}

	/**
	 * Memory across many round trips between the two cells in the real game (P7): the editor automation
	 * world cannot measure this (it never runs the physics and render scenes' deferred cleanup).
	 * Every 6 s Zenny jumps to the other cell, which streams in synchronously while the one left
	 * streams out; garbage is collected, then memory is sampled. Writes Saved/Perf/roundtrips.json.
	 */
	FAutoConsoleCommandWithWorldAndArgs RoundTripsCommand(
		TEXT("gl.Perf.RoundTrips"),
		TEXT("DEV ONLY: gl.Perf.RoundTrips [N=8] - jumps Zenny between the cells N times each way, sampling memory after each."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const int32 RoundTrips = FMath::Max(2, Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 8);
			TWeakObjectPtr<UWorld> Weak(World);
			TSharedRef<TArray<double>> Samples = MakeShared<TArray<double>>();
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Samples, RoundTrips](float)
			{
				UWorld* W = Weak.Get();
				APawn* Zenny = W ? UGameplayStatics::GetPlayerPawn(W, 0) : nullptr;
				if (!Zenny)
				{
					return !!W;
				}
				if (Samples->Num() >= 2 * RoundTrips)
				{
					// Growth after the first full round trip (both cells have been visited once).
					double Peak = 0.0;
					for (int32 I = 2; I < Samples->Num(); ++I)
					{
						Peak = FMath::Max(Peak, (*Samples)[I]);
					}
					TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
					O->SetNumberField(TEXT("roundTrips"), RoundTrips);
					TArray<TSharedPtr<FJsonValue>> Values;
					for (const double Mb : *Samples)
					{
						Values.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Mb)));
					}
					O->SetArrayField(TEXT("memMbAfterMove"), Values);
					O->SetNumberField(TEXT("memGrowthMb"), Peak - (*Samples)[1]);
					O->SetNumberField(TEXT("memPeakMb"), FMath::Max(Peak, FMath::Max((*Samples)[0], (*Samples)[1])));
					FString Text;
					FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Text));
					FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectSavedDir() / TEXT("Perf") / TEXT("roundtrips.json")));
					UE_LOG(LogGridlands, Log, TEXT("gl.Perf.RoundTripsResult %s"), *Text);
					GEngine->DeferredCommands.Add(TEXT("quit"));
					return false;
				}
				const FVector2D To = Samples->Num() % 2 == 0 ? FVector2D(120000.0, 0.0) : FVector2D(0.0, -1200.0);
				UGLGridSubsystem* Grid = W->GetSubsystem<UGLGridSubsystem>();
				UGLTerrainSubsystem* Terrain = W->GetSubsystem<UGLTerrainSubsystem>();
				Zenny->SetActorLocation(FVector(To, 300.0), false, nullptr, ETeleportType::TeleportPhysics);
				Grid->Advance(Zenny->GetActorLocation());
				Grid->FlushAll();
				Zenny->SetActorLocation(FVector(To, Terrain->HeightAt(To) + 110.0), false, nullptr, ETeleportType::TeleportPhysics);
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
				Samples->Add(FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0));
				UE_LOG(LogGridlands, Log, TEXT("gl.Perf.RoundTrips move %d: %.0f MB"), Samples->Num(), Samples->Last());
				return true;
			}), 6.0f);
		}));

	FAutoConsoleCommandWithWorldAndArgs CrossingCommand(
		TEXT("gl.Perf.Crossing"),
		TEXT("DEV ONLY: gl.Perf.Crossing straight|reversal|sprint|teleport|resume - walks Zenny across the 1 km Grid and records every frame."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			Crossing = FCrossing();
			Crossing.World = World;
			Crossing.Mode = Args.Num() > 0 ? Args[0] : TEXT("straight");
			Crossing.RouteY = -30000.0; // a line through open ground, clear of the town
			Crossing.X = 0.0;
			if (Crossing.Mode == TEXT("reversal"))
			{
				// Toward the lots (they start loading 256 m out), turn back while their chunks are still
				// building, go home past the unload margin, then cross for real.
				Crossing.Route = { 26500.0, 8000.0, 26500.0, 5000.0, 90000.0 };
				Crossing.Speed = 1200.0;
			}
			else if (Crossing.Mode == TEXT("teleport"))
			{
				// Walking can never cancel a load (it completes in ~2.5 s; the unload margin is 128 m
				// further). A jump can: step 10 cm into the load margin, then jump home at once, while
				// the lots' level and ground are still in flight. Then walk across and arrive.
				Crossing.Route = { 25610.0, -20000.0, 90000.0 };
				Crossing.TeleportLeg = 1;
				Crossing.Speed = 1200.0;
			}
			else if (Crossing.Mode == TEXT("resume"))
			{
				// Launched from a save made in the lots: stand still and measure the start.
				Crossing.Route = { 90000.0 };
				Crossing.X = 90000.0;
			}
			else if (Crossing.Mode == TEXT("sprint"))
			{
				Crossing.Route = { 90000.0 };
				Crossing.Speed = 1800.0;
			}
			else
			{
				Crossing.Route = { 90000.0 };
			}
			APawn* Zenny = UGameplayStatics::GetPlayerPawn(World, 0);
			if (Zenny && Crossing.Mode == TEXT("resume"))
			{
				Crossing.X = Zenny->GetActorLocation().X; // where the save put Zenny
				Crossing.RouteY = Zenny->GetActorLocation().Y;
				Crossing.Route = { Crossing.X };
				Zenny->DisableInput(nullptr);
			}
			else if (Zenny)
			{
				Zenny->SetActorLocation(FVector(0.0, Crossing.RouteY, 300.0));
				Zenny->DisableInput(nullptr);
			}
			FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddLambda([] { ++Crossing.GcCount; });
			Crossing.Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&CrossingTick));
		}));

	FAutoConsoleCommandWithWorldAndArgs TerrainCommand(
		TEXT("gl.Perf.Terrain"),
		TEXT("DEV ONLY: gl.Perf.Terrain Metres [SpacingMetres] [ChunkMetres] - terrain scaling harness; writes Saved/Perf/terrain-<m>m.json and quits."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Terrain));
}

#endif
