// DEV ONLY: P7 visual review. Stages the style slice in the diner lots (Zenny, Pehlichi, posed proof
// creatures), frames named views and takes real-time screenshots for the operator's visual judgement;
// exercises terrain edits, structural collapse and tree felling inside the styled scene.
// Not in shipping builds.

#include "Camera/CameraActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "DynamicRHI.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Presentation/GLScatterPatch.h"
#include "RenderTimer.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Camera/CameraComponent.h"
#include "Character/GLCharacter.h"
#include "Combat/GLCreature.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "GridlandsGame.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Presentation/GLStyleSubsystem.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "TimerManager.h"
#include "World/GLGridSubsystem.h"
#include "World/GLPlacementSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace GLStyleDemo
{
	/** The slice centre in world cm (diner lots, local (-142, 218) m). */
	const FVector2D SCentre(102400.0 - 14200.0, 21800.0);
	const FName SLots(TEXT("cell.outer.diner_lots"));
	const FName SStorefront(TEXT("placement.diner_lots.slice_storefront"));
	const FName SPine(TEXT("placement.diner_lots.slice_pine_01"));
	TWeakObjectPtr<ACameraActor> Camera;
	int32 Shots = 0;

	AGLCharacter* Zenny(UWorld* World) { return Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)); }
	double Ground(UWorld* World, const FVector2D& At) { return World->GetSubsystem<UGLTerrainSubsystem>()->HeightAt(At); }

	void Put(UWorld* World, AActor* Actor, const FVector2D& At, double Up, double Yaw)
	{
		if (Actor)
		{
			Actor->SetActorLocationAndRotation(FVector(At, Ground(World, At) + Up), FRotator(0.0, Yaw, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	/** Frames the shot from a camera at Eye (slice-relative XY, height above ground) looking at Target. */
	void View(UWorld* World, const FVector2D& Eye, double EyeUp, const FVector2D& Target, double TargetUp, double Fov = 60.0)
	{
		const FVector From(SCentre + Eye, Ground(World, SCentre + Eye) + EyeUp);
		const FVector To(SCentre + Target, Ground(World, SCentre + Target) + TargetUp);
		if (!Camera.IsValid())
		{
			Camera = World->SpawnActor<ACameraActor>();
		}
		Camera->SetActorLocationAndRotation(From, (To - From).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(Fov);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTarget(Camera.Get());
		}
	}

	void Shot(const TCHAR* Name)
	{
		++Shots;
		UE_LOG(LogGridlands, Log, TEXT("gl.Style.Shot %d %s"), Shots, Name);
		GEngine->DeferredCommands.Add(TEXT("HighResShot 1920x1080"));
	}

	void Stage(UWorld* World)
	{
		AGLCharacter* Z = Zenny(World);
		if (!Z)
		{
			return;
		}
		UGLGridSubsystem* Grid = World->GetSubsystem<UGLGridSubsystem>();
		const FVector2D Front = SCentre + FVector2D(60, -760);
		Z->SetActorLocation(FVector(Front, 2000.0));
		Grid->Advance(FVector(Front, 0.0));
		Grid->FlushAll();
		Put(World, Z, Front, 100.0, 90.0);
		if (AGLPehlichi* Peh = Z->GetPehlichi())
		{
			Peh->GetPositioning()->Follow(nullptr);
			Put(World, Peh, Front + FVector2D(-110, 40), 170.0, 60.0);
		}
		// Posed proof creatures (dev-only, not saved): the same creature, ordinary and sparsely corrupted.
		UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>();
		for (const auto& [Offset, Visual] : { TPair<FVector2D, const TCHAR*>(FVector2D(-110, -980), TEXT("visual.creature.raccoon")), TPair<FVector2D, const TCHAR*>(FVector2D(110, -980), TEXT("visual.creature.raccoon_glitched")) })
		{
			const FVector2D At = SCentre + Offset;
			Placements->SpawnProofCreature(TEXT("creature.drain.static_gremlin"), FVector(At, Ground(World, At)), Offset.X < 0 ? -70.0 : -150.0, SLots, Visual, true);
		}
		if (APlayerController* PC = World->GetFirstPlayerController(); PC && PC->MyHUD)
		{
			PC->MyHUD->bShowHUD = false; // review frames show the world, not subtitles and HUD static
		}
		UE_LOG(LogGridlands, Log, TEXT("gl.Style: slice staged at %s (lots loaded %d)"), *SCentre.ToString(), Grid->IsLoaded(SLots) ? 1 : 0);
	}

	void Salvage(UWorld* World, FName Placement, FName Part)
	{
		AGLStructurePart* Actor = World->GetSubsystem<UGLStructureSubsystem>()->FindPart(Placement, Part);
		for (int32 Hit = 0; Actor && Hit < 60 && !Actor->GetSalvageable()->IsSalvaged(); ++Hit)
		{
			Actor->GetSalvageable()->Interact(Zenny(World), UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Interact.Salvage")));
		}
	}

	void Dig(UWorld* World)
	{
		AGLCharacter* Z = Zenny(World);
		Z->GetInventory()->AddItem(TEXT("item.tool.shovel"), 1);
		Z->GetInventory()->AddItem(TEXT("item.material.soil"), 40);
		UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
		int32 Dug = 0, Raised = 0;
		for (int32 I = 0; I < 5; ++I) // a pit in the lawn, in front of the diner
		{
			for (const FVector2D& D : { FVector2D(0, 0), FVector2D(120, 0), FVector2D(60, 100), FVector2D(-60, 90) })
			{
				Dug += Terrain->Terraform(Z, TEXT("terraform.shovel.dig"), SCentre + FVector2D(-600, -1250) + D).bApplied ? 1 : 0;
			}
		}
		for (int32 I = 0; I < 2; ++I) // and a low mound of what came out
		{
			for (const FVector2D& D : { FVector2D(0, 0), FVector2D(130, 0), FVector2D(-130, 0), FVector2D(0, 130), FVector2D(0, -130), FVector2D(90, 90), FVector2D(-90, -90), FVector2D(90, -90), FVector2D(-90, 90) })
			{
				Raised += Terrain->Terraform(Z, TEXT("terraform.shovel.raise"), SCentre + FVector2D(250, -1350) + D).bApplied ? 1 : 0;
			}
		}
		UE_LOG(LogGridlands, Log, TEXT("gl.Style: terrain edited in the slice: %d dig strokes, %d raise strokes"), Dug, Raised);
	}

	struct FStep { double At; TFunction<void()> Run; };

	void Tour(UWorld* World)
	{
		TArray<FStep> Steps = {
			{ 1.0, [World]() { Stage(World); World->GetSubsystem<UGLStyleSubsystem>()->ApplyPreset(TEXT("day")); } },
			{ 5.0, [World]() { View(World, FVector2D(-1250, -2300), 520.0, FVector2D(0, -200), 160.0); } },
			{ 6.5, []() { Shot(TEXT("01-day-overview")); } },
			{ 8.0, [World]() { Put(World, Zenny(World), SCentre + FVector2D(-650, -1250), 100.0, 60.0); if (AGLPehlichi* P = Zenny(World)->GetPehlichi()) { Put(World, P, SCentre + FVector2D(-760, -1200), 170.0, 60.0); } View(World, FVector2D(0, -960), 125.0, FVector2D(0, -520), 78.0, 44.0); } },
			{ 9.5, []() { Shot(TEXT("03-prop-ordinary-vs-corrupted")); } },
			{ 11.0, [World]() { View(World, FVector2D(0, -1300), 85.0, FVector2D(0, -980), 30.0, 44.0); } },
			{ 12.5, []() { Shot(TEXT("04-creature-ordinary-vs-corrupted")); } },
			{ 14.0, [World]() { Put(World, Zenny(World), SCentre + FVector2D(60, -760), 100.0, 90.0); if (AGLPehlichi* P = Zenny(World)->GetPehlichi()) { Put(World, P, SCentre + FVector2D(-50, -720), 170.0, 60.0); } View(World, FVector2D(300, -1500), 190.0, FVector2D(0, -760), 120.0, 50.0); } },
			{ 15.5, []() { Shot(TEXT("05-zenny-pehlichi-readability")); } },
			{ 15.8, [World]() { View(World, FVector2D(260, -300), 150.0, FVector2D(40, -760), 120.0, 45.0); } },
			{ 16.6, []() { Shot(TEXT("05b-zenny-pehlichi-front")); } },
			{ 17.0, [World]() { World->GetSubsystem<UGLStyleSubsystem>()->ApplyPreset(TEXT("dusk")); View(World, FVector2D(-1250, -2300), 520.0, FVector2D(0, -200), 160.0); } },
			{ 19.5, []() { Shot(TEXT("02a-dusk-overview")); } },
			{ 21.0, [World]() { World->GetSubsystem<UGLStyleSubsystem>()->ApplyPreset(TEXT("night")); } },
			{ 24.0, []() { Shot(TEXT("02b-night-overview")); } },
			{ 25.5, [World]() { World->GetSubsystem<UGLStyleSubsystem>()->SetPostEnabled(false); } },
			{ 27.0, []() { Shot(TEXT("02c-night-without-stylize-post")); } },
			{ 28.5, [World]() { World->GetSubsystem<UGLStyleSubsystem>()->SetPostEnabled(true); World->GetSubsystem<UGLStyleSubsystem>()->ApplyPreset(TEXT("day")); } },
			{ 30.0, [World]() { World->GetSubsystem<UGLStyleSubsystem>()->SetPostEnabled(false); View(World, FVector2D(-1250, -2300), 520.0, FVector2D(0, -200), 160.0); } },
			{ 32.0, []() { Shot(TEXT("01b-day-without-stylize-post")); } },
			{ 33.0, [World]() { World->GetSubsystem<UGLStyleSubsystem>()->SetPostEnabled(true); } },
			{ 34.0, [World]() { Dig(World); View(World, FVector2D(-900, -2250), 420.0, FVector2D(-150, -1300), 0.0, 55.0); } },
			{ 36.5, []() { Shot(TEXT("06-terrain-after-manipulation")); } },
			{ 38.0, [World]()
			{
				// The awning's three posts: the awning (hung on posts and roof slabs) comes down. And a pine is felled.
				Put(World, Zenny(World), SCentre + FVector2D(-420, -520), 100.0, 90.0);
				for (const TCHAR* Post : { TEXT("post_w"), TEXT("post_m"), TEXT("post_e") })
				{
					Salvage(World, SStorefront, Post);
				}
				Put(World, Zenny(World), SCentre + FVector2D(-1100, 300), 100.0, 0.0);
				Salvage(World, SPine, TEXT("stump"));
				Put(World, Zenny(World), SCentre + FVector2D(-100, -1400), 100.0, 90.0);
				View(World, FVector2D(-1300, -1900), 470.0, FVector2D(-300, -100), 60.0, 62.0);
			} },
			{ 38.9, []() { Shot(TEXT("07a-collapse-in-progress")); } },
			{ 43.0, [World]()
			{
				const FGLStructureRuntime* S = World->GetSubsystem<UGLStructureSubsystem>()->Find(SStorefront);
				int32 Debris = 0;
				for (const FGLStructurePartRuntime& P : S ? S->Parts : TArray<FGLStructurePartRuntime>())
				{
					Debris += P.State == EGLStructurePartState::Debris ? 1 : 0;
				}
				UE_LOG(LogGridlands, Log, TEXT("gl.Style: after collapse: %d storefront parts are debris; impacts %d"), Debris, World->GetSubsystem<UGLStructureSubsystem>()->GetImpacts().Num());
			} },
			{ 43.5, []() { Shot(TEXT("07b-collapse-debris-and-felled-pine")); } },
			{ 46.0, []() { GEngine->DeferredCommands.Add(TEXT("quit")); } },
		};
		for (FStep& Step : Steps)
		{
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(MoveTemp(Step.Run)), Step.At, false);
		}
	}

	/**
	 * gl.Perf.Style: where the rendering budget goes in the styled slice (P7-K). Holds the overview view
	 * and measures frame / game / render / GPU time with each feature switched off in turn. Writes
	 * Saved/Perf/style.json and quits.
	 */
	struct FStyleConfig { const TCHAR* Name; TFunction<void(UWorld*, bool)> Toggle; };
	struct FStylePerf
	{
		TArray<FStyleConfig> Configs;
		int32 Index = -1;
		int32 Frames = 0;
		TArray<double> Frame, Game, Render, Gpu;
		TSharedPtr<FJsonObject> Out;
		FTSTicker::FDelegateHandle Ticker;
		TWeakObjectPtr<UWorld> World;
	};
	FStylePerf Perf;

	double Median(TArray<double> V) { if (!V.Num()) return 0.0; V.Sort(); return V[V.Num() / 2]; }
	double P95(TArray<double> V) { if (!V.Num()) return 0.0; V.Sort(); return V[FMath::Min(V.Num() - 1, FMath::FloorToInt(V.Num() * 0.95))]; }

	void SetScatterVisible(UWorld* World, bool bVisible)
	{
		for (TActorIterator<AGLScatterPatch> It(World); It; ++It) { It->SetActorHiddenInGame(!bVisible); }
	}

	void SetCorruptionVisible(UWorld* World, bool bVisible)
	{
		UMaterialInterface* Corrupt = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Gridlands/Art/Materials/M_GLCorruption.M_GLCorruption"));
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetMaterial(0) == Corrupt) { It->SetVisibility(bVisible); }
		}
	}

	bool PerfTick(float)
	{
		UWorld* World = Perf.World.Get();
		if (!World)
		{
			return false;
		}
		constexpr int32 Warmup = 90, Measure = 400;
		++Perf.Frames;
		if (Perf.Index >= 0 && Perf.Frames > Warmup)
		{
			Perf.Frame.Add(FApp::GetDeltaTime() * 1000.0);
			Perf.Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
			Perf.Render.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
			Perf.Gpu.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
		}
		if (Perf.Index < 0 || Perf.Frames >= Warmup + Measure)
		{
			if (Perf.Index >= 0)
			{
				TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
				Row->SetNumberField(TEXT("frameMsMedian"), Median(Perf.Frame));
				Row->SetNumberField(TEXT("frameMsP95"), P95(Perf.Frame));
				Row->SetNumberField(TEXT("gpuMsMedian"), Median(Perf.Gpu));
				Row->SetNumberField(TEXT("gameThreadMsMedian"), Median(Perf.Game));
				Row->SetNumberField(TEXT("renderThreadMsMedian"), Median(Perf.Render));
				Perf.Out->SetObjectField(Perf.Configs[Perf.Index].Name, Row);
				UE_LOG(LogGridlands, Log, TEXT("gl.Perf.Style %s: frame %.2f ms, GPU %.2f ms, game %.2f, render %.2f"), Perf.Configs[Perf.Index].Name, Median(Perf.Frame), Median(Perf.Gpu), Median(Perf.Game), Median(Perf.Render));
				Perf.Configs[Perf.Index].Toggle(World, false); // restore
			}
			++Perf.Index;
			Perf.Frames = 0;
			Perf.Frame.Reset(); Perf.Game.Reset(); Perf.Render.Reset(); Perf.Gpu.Reset();
			if (Perf.Index >= Perf.Configs.Num())
			{
				FString Text;
				FJsonSerializer::Serialize(Perf.Out.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
				FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectSavedDir() / TEXT("Perf") / TEXT("style.json")));
				UE_LOG(LogGridlands, Log, TEXT("gl.Perf.StyleResult %s"), *Text);
				GEngine->DeferredCommands.Add(TEXT("quit"));
				return false;
			}
			Perf.Configs[Perf.Index].Toggle(World, true); // this configuration's change
		}
		return true;
	}

	void PerfStyle(UWorld* World)
	{
		Perf = FStylePerf();
		Perf.World = World;
		Perf.Out = MakeShared<FJsonObject>();
		auto Style = [](UWorld* W) { return W->GetSubsystem<UGLStyleSubsystem>(); };
		Perf.Configs = {
			{ TEXT("full"), [](UWorld*, bool) {} },
			{ TEXT("outlineOff"), [Style](UWorld* W, bool b) { Style(W)->SetParam(TEXT("OutlineOn"), b ? 0.f : 1.f); } },
			{ TEXT("celOff"), [Style](UWorld* W, bool b) { Style(W)->SetParam(TEXT("CelOn"), b ? 0.f : 1.f); } },
			{ TEXT("postOff"), [Style](UWorld* W, bool b) { Style(W)->SetPostEnabled(!b); } },
			{ TEXT("vegetationOff"), [](UWorld* W, bool b) { SetScatterVisible(W, !b); } },
			{ TEXT("corruptionOff"), [](UWorld* W, bool b) { SetCorruptionVisible(W, !b); } },
			{ TEXT("shadowsOff"), [](UWorld*, bool b) { GEngine->Exec(nullptr, b ? TEXT("r.ShadowQuality 0") : TEXT("r.ShadowQuality 5")); } },
		};
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([World]()
		{
			Stage(World);
			World->GetSubsystem<UGLStyleSubsystem>()->ApplyPreset(TEXT("day"));
			View(World, FVector2D(-1250, -2300), 520.0, FVector2D(0, -200), 160.0);
			int32 Grass = 0, Patches = 0;
			for (TActorIterator<AGLScatterPatch> It(World); It; ++It) { Grass += It->GetInstanceCount(); ++Patches; }
			Perf.Out->SetNumberField(TEXT("scatterInstances"), Grass);
			Perf.Out->SetNumberField(TEXT("scatterPatches"), Patches);
			Perf.Out->SetStringField(TEXT("gpu"), GRHIAdapterName);
			Perf.Out->SetNumberField(TEXT("resX"), GSystemResolution.ResX);
			Perf.Out->SetNumberField(TEXT("resY"), GSystemResolution.ResY);
			Perf.Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&PerfTick));
		}), 3.0f, false);
	}

	FAutoConsoleCommandWithWorld PerfStyleCommand(TEXT("gl.Perf.Style"),
		TEXT("DEV ONLY (P7): measures the styled slice with each rendering feature switched off in turn; writes Saved/Perf/style.json."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&PerfStyle));

	FAutoConsoleCommandWithWorld TourCommand(TEXT("gl.Style.Tour"),
		TEXT("DEV ONLY (P7): the visual review tour: stages the diner-lots slice, frames views, edits terrain, collapses the awning, fells a pine; screenshots and quits."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Tour));
	FAutoConsoleCommandWithWorld StageCommand(TEXT("gl.Style.Stage"),
		TEXT("DEV ONLY (P7): puts Zenny, Pehlichi and two posed proof creatures in the style slice."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&Stage));
}

#endif
