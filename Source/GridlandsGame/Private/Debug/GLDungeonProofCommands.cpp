// DEV ONLY (P9, ADR-0037): the dual-route proof room played in the real game (-GLDungeonProof), with real ticking,
// real navigation and real streaming. Zenny is driven through the character's own movement input (so footsteps are
// real movement noise), Pehlichi is commanded like the player commands him, and the creatures run their own
// behaviour. Each run writes Saved/P9/dungeon-<mode>.json and logs "gl.Dungeon.Proof:" lines.
//   gl.Dungeon.Proof direct        fight the patrols and the warden; the warden is DEFEATED through health
//   gl.Dungeon.Proof environmental slip past the patrols (fan timing on the grate), lead the warden into the cage
//                                  while it attacks, Pehlichi operates the cage: the warden is NEUTRALIZED
//   gl.Dungeon.Proof report        after a restart: outcomes, presentation, nothing replayed
//   gl.Dungeon.Proof navscale      navigation tiles with the room's creatures, then with 1 and 16 extra active ones

#include "Character/GLCharacter.h"
#include "Combat/GLCombatComponent.h"
#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "GameplayTagsManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GridlandsGame.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Mechanism/GLMechanism.h"
#include "Mechanism/GLMechanismSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Noise/GLNoiseSubsystem.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "World/GLGridCells.h"
#include "World/GLGridSubsystem.h"
#include "World/GLNavRegionSubsystem.h"
#include "World/GLPlacementSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace
{
	const FName DPLots(TEXT("cell.outer.diner_lots"));
	constexpr int32 DPFirstGremlin = 21, DPWarden = 25, DPCage = 26, DPFan = 27;

	FName DPId(int32 I) { return UGLPlacementSubsystem::DungeonProofId(I); }

	struct FDungeonRun
	{
		FString Mode;
		TWeakObjectPtr<UWorld> World;
		int32 Phase = 0;
		double PhaseAt = 0.0;
		double Started = 0.0;
		int32 Waypoint = 0;
		int32 Target = DPFirstGremlin;
		int32 TilesPeak = 0;
		int32 TasksPeak = 0;
		double NextSample = 0.0;
		double ZennyDamage = 0.0;
		double WardenDamage = 0.0;
		TMap<FName, int32> Events;
		FDelegateHandle Listener;
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		TArray<AGLCreature*> Extra;
		FTSTicker::FDelegateHandle Ticker;
		bool bInside = false; // navscale inside: Zenny stands in the room (his own circle covers it too)
	};
	TSharedPtr<FDungeonRun> Run;

	FVector RoomPoint(UWorld* World, double X, double Y, double Up = 100.0)
	{
		const FVector2D At(102400.0 + UGLPlacementSubsystem::DungeonProofOrigin.X + X, UGLPlacementSubsystem::DungeonProofOrigin.Y + Y);
		const UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
		return FVector(At, (Terrain && Terrain->HasGroundAt(At) ? Terrain->HeightAt(At) : 0.0) + Up);
	}

	AGLCharacter* ZennyOf(UWorld* World) { return Cast<AGLCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)); }

	AGLPehlichi* PehlichiOf(UWorld* World)
	{
		for (TActorIterator<AGLPehlichi> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Walks Zenny toward a room point with his own movement input. True once there. */
	bool WalkTo(UWorld* World, double X, double Y)
	{
		AGLCharacter* Zenny = ZennyOf(World);
		const FVector Goal = RoomPoint(World, X, Y);
		const FVector To = Goal - Zenny->GetActorLocation();
		if (To.Size2D() < 80.0)
		{
			return true;
		}
		Zenny->AddMovementInput(FVector(To.X, To.Y, 0.0).GetSafeNormal(), 1.0f, true);
		return false;
	}

	int32 ActiveTiles(UWorld* World, int32* Tasks = nullptr)
	{
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const ARecastNavMesh* R = Nav ? Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
		if (Tasks && Nav)
		{
			*Tasks = Nav->GetNumRemainingBuildTasks();
		}
		return R ? R->GetNumActiveTiles() : 0;
	}

	void Log(const FString& Line)
	{
		UE_LOG(LogGridlands, Log, TEXT("gl.Dungeon.Proof: %s %s"), *Run->Mode, *Line);
	}

	void Finish(UWorld* World, bool bQuit)
	{
		const UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>();
		const FGLActorPlacement* Warden = Placements->FindActorModel(DPId(DPWarden));
		const FGLMechanismRecord* Cage = World->GetSubsystem<UGLMechanismSubsystem>()->FindRecord(DPId(DPCage));
		const AGLCreature* WardenActor = Placements->FindCreature(DPId(DPWarden));
		TSharedRef<FJsonObject> O = Run->Out;
		O->SetStringField(TEXT("mode"), Run->Mode);
		O->SetNumberField(TEXT("seconds"), World->GetTimeSeconds() - Run->Started);
		O->SetStringField(TEXT("wardenOutcome"), !Warden ? TEXT("?") : Warden->Creature.Outcome == EGLCreatureOutcome::Defeated ? TEXT("Defeated")
			: Warden->Creature.Outcome == EGLCreatureOutcome::Neutralized ? TEXT("Neutralized") : TEXT("None"));
		O->SetStringField(TEXT("wardenNeutralizedHow"), Warden ? Warden->Creature.NeutralizedHow.ToString() : TEXT(""));
		O->SetBoolField(TEXT("wardenActorPresent"), WardenActor != nullptr && !WardenActor->IsHidden());
		O->SetBoolField(TEXT("wardenActorInert"), WardenActor && !WardenActor->IsActorTickEnabled());
		O->SetNumberField(TEXT("wardenHealth"), WardenActor ? WardenActor->GetHealth()->GetCurrent() : -1.0);
		O->SetStringField(TEXT("cageState"), Cage ? Cage->State : TEXT("?"));
		O->SetNumberField(TEXT("cageLift"), Cage && Cage->Actor.IsValid() ? Cage->Actor->GetCageLift() : -1.0);
		O->SetNumberField(TEXT("zennyDamageTaken"), Run->ZennyDamage);
		O->SetNumberField(TEXT("wardenDamageTaken"), Run->WardenDamage);
		O->SetNumberField(TEXT("navActiveTilesPeak"), Run->TilesPeak);
		O->SetNumberField(TEXT("navPendingTasksPeak"), Run->TasksPeak);
		O->SetNumberField(TEXT("navStaleTiles"), World->GetSubsystem<UGLNavRegionSubsystem>()->CountStaleTiles(RoomPoint(World, 0, 0), 30));
		O->SetNumberField(TEXT("navSweptTiles"), World->GetSubsystem<UGLNavRegionSubsystem>()->GetSweptTiles());
		const AGLCharacter* Zenny = ZennyOf(World);
		O->SetNumberField(TEXT("residue"), Zenny && Zenny->GetInventory() ? Zenny->GetInventory()->CountOf(TEXT("item.material.static_residue")) : -1);
		TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
		for (const TPair<FName, int32>& Pair : Run->Events)
		{
			E->SetNumberField(Pair.Key.ToString(), Pair.Value);
		}
		O->SetObjectField(TEXT("events"), E);
		FString Text;
		FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Text));
		FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectSavedDir() / TEXT("P9") / (TEXT("dungeon-") + Run->Mode + TEXT(".json"))));
		Log(Text);
		if (UGLEventSubsystem* Bus = World->GetSubsystem<UGLEventSubsystem>())
		{
			Bus->Unsubscribe(Run->Listener);
		}
		if (bQuit)
		{
			GEngine->DeferredCommands.Add(TEXT("quit")); // quitting autosaves
		}
	}

	void NextPhase(UWorld* World, const FString& Note = FString())
	{
		++Run->Phase;
		Run->PhaseAt = World->GetTimeSeconds();
		if (!Note.IsEmpty())
		{
			const AGLCharacter* Zenny = ZennyOf(World);
			Log(FString::Printf(TEXT("phase %d at %.1f s (Zenny %.0f hp, detections so far %d): %s"), Run->Phase, World->GetTimeSeconds() - Run->Started,
				Zenny ? Zenny->GetHealth()->GetCurrent() : -1.0, Run->Events.FindRef(TEXT("Event.Creature.Spotted")), *Note));
		}
	}

	/** One frame of the environmental route. */
	bool TickEnvironmental(UWorld* World)
	{
		UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>();
		UGLMechanismSubsystem* Mechanisms = World->GetSubsystem<UGLMechanismSubsystem>();
		const double InPhase = World->GetTimeSeconds() - Run->PhaseAt;
		AGLCreature* Warden = Placements->FindCreature(DPId(DPWarden));
		// A script cannot dodge like a player: if Zenny is about to die (a respawn would take him out of the room),
		// his health is restored and counted (Dev.ZennyHealed). The damage he took stays in the evidence.
		if (AGLCharacter* Zenny = ZennyOf(World); Zenny && Zenny->GetHealth()->GetCurrent() < 25.0)
		{
			Zenny->GetHealth()->Restore(Zenny->GetHealth()->GetMax());
			++Run->Events.FindOrAdd(TEXT("Dev.ZennyHealed"));
		}
		switch (Run->Phase)
		{
		case 1: // through the entrance, wait for the hall patrol at y = 3 m to be at its far end
		{
			FVector Guard;
			const bool bClear = Placements->CreatureLocation(DPId(DPFirstGremlin + 1), Guard) && Guard.X > RoomPoint(World, -900, 0).X;
			if (WalkTo(World, -1750, 500) && (bClear || InPhase > 20.0))
			{
				NextPhase(World, bClear ? TEXT("hall patrol at its far end: into the corridor") : TEXT("patrol timing timed out: going anyway"));
			}
			break;
		}
		case 2: // into the corridor's west end
			if (WalkTo(World, -1650, 1150))
			{
				NextPhase(World, TEXT("at the grate: waiting for the fan"));
			}
			break;
		case 3: // wait for the fan to sound with most of its on-window left, then cross the grate
		{
			const FGLMechanismRecord* Fan = Mechanisms->FindRecord(DPId(DPFan));
			const FGLMechanismDef* Def = Fan ? UGLMechanismSubsystem::DefOf(*Fan) : nullptr;
			const double T = World->GetTimeSeconds();
			const bool bFresh = Def && UGLMechanismSubsystem::IsAmbientOn(*Fan, T) && FMath::Fmod(T + Def->Ambient.Phase, Def->Ambient.Period) < 2.0;
			if (bFresh || InPhase > 30.0)
			{
				NextPhase(World, FString::Printf(TEXT("fan sounding (mask %.1f at the nearest guard): crossing the grate"),
					World->GetSubsystem<UGLNoiseSubsystem>()->MaskAt(RoomPoint(World, -900, 600))));
			}
			break;
		}
		case 4: // across the grate to the corridor's east end
			if (WalkTo(World, -250, 1150))
			{
				NextPhase(World, TEXT("through the corridor"));
			}
			break;
		case 5: // wait for the arena patrol to walk away east, then slip south to the cage zone
		{
			FVector Guard;
			const bool bAway = Placements->CreatureLocation(DPId(DPFirstGremlin + 3), Guard) && Guard.X > RoomPoint(World, 700, 0).X;
			if (bAway || InPhase > 20.0)
			{
				NextPhase(World, TEXT("arena patrol away: to the cage"));
			}
			break;
		}
		case 6: // the zone's south side: the warden, coming from its home to the north, attacks from inside the zone
			if (WalkTo(World, 1300, -950))
			{
				NextPhase(World, TEXT("in the cage zone: drawing the warden"));
			}
			break;
		case 7: // the warden comes; once it attacks, Pehlichi goes to the control
			if (Warden && (Warden->GetState() == EGLCreatureState::Attack || Warden->GetState() == EGLCreatureState::Chase) && InPhase > 1.0)
			{
				AGLPehlichi* Peh = PehlichiOf(World);
				const EGLCommandRejection R = Peh ? Peh->GetCommands()->Issue(TEXT("Command.Pehlichi.Operate"), ZennyOf(World)) : EGLCommandRejection::UnknownCommand;
				NextPhase(World, FString::Printf(TEXT("warden %s; Operate %s"), GLCreatureRules::StateName(Warden->GetState()), R == EGLCommandRejection::None ? TEXT("accepted") : TEXT("REJECTED")));
			}
			else if (InPhase > 30.0)
			{
				NextPhase(World, TEXT("the warden never came"));
				Run->Phase = 99;
			}
			break;
		case 8: // hold the warden in the zone while it attacks: Zenny stays, takes the hits
			if (const FGLMechanismRecord* Cage = World->GetSubsystem<UGLMechanismSubsystem>()->FindRecord(DPId(DPCage)); Cage && Cage->State == TEXT("dropped") && !Placements->IsEncounterResolved(DPId(DPWarden)))
			{
				NextPhase(World, TEXT("the cage dropped with the warden OUTSIDE the zone at the decision: not neutralized (the rule held)"));
			}
			else if (Placements->IsEncounterResolved(DPId(DPWarden)))
			{
				NextPhase(World, FString::Printf(TEXT("warden %s after %.1f s of Pehlichi's work"), Placements->IsCreatureNeutralized(DPId(DPWarden)) ? TEXT("NEUTRALIZED") : TEXT("resolved"), InPhase));
			}
			else if (InPhase > 45.0)
			{
				NextPhase(World, TEXT("timed out waiting for the cage"));
			}
			break;
		case 9:
			if (InPhase > 3.0)
			{
				Finish(World, true);
				return false;
			}
			break;
		default:
			Finish(World, true);
			return false;
		}
		return true;
	}

	/** One frame of the direct route: fight each patrol, then the warden (health restored between fights, logged). */
	bool TickDirect(UWorld* World)
	{
		UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>();
		AGLCharacter* Zenny = ZennyOf(World);
		if (Run->Phase == 1)
		{
			if (Run->Target > DPWarden)
			{
				NextPhase(World, TEXT("every patrol and the warden fought"));
				return true;
			}
			AGLCreature* Foe = Placements->FindCreature(DPId(Run->Target));
			if (!Foe || !Foe->IsActiveHostile())
			{
				Log(FString::Printf(TEXT("%s: %s"), *DPId(Run->Target).ToString(), Placements->IsCreatureDefeated(DPId(Run->Target)) ? TEXT("defeated") : TEXT("gone")));
				Zenny->GetHealth()->Restore(Zenny->GetHealth()->GetMax()); // DEV: survival is not what the direct proof measures
				++Run->Target;
				return true;
			}
			// Through the room's openings, not its walls: in by the west gap, into the arena by the main door.
			const double DoorX = RoomPoint(World, -400, 0).X;
			const bool bInside = Zenny->GetActorLocation().X > RoomPoint(World, -1900, 0).X;
			const bool bFoeInArena = Foe->GetActorLocation().X > DoorX, bZennyInArena = Zenny->GetActorLocation().X > DoorX;
			if (!bInside && !WalkTo(World, -1700, 400))
			{
				return true;
			}
			if (bFoeInArena != bZennyInArena && !WalkTo(World, bFoeInArena ? 100 : -900, -400))
			{
				return true;
			}
			if (Zenny->GetHealth()->GetCurrent() < 30.0)
			{
				Zenny->GetHealth()->Restore(Zenny->GetHealth()->GetMax()); // DEV: survival is not what the direct proof measures (logged)
				++Run->Events.FindOrAdd(TEXT("Dev.ZennyHealed"));
			}
			const FVector To = Foe->GetActorLocation() - Zenny->GetActorLocation();
			if (To.Size2D() > 120.0)
			{
				Zenny->AddMovementInput(FVector(To.X, To.Y, 0.0).GetSafeNormal(), 1.0f, true);
			}
			Zenny->SetActorRotation(FRotator(0.0, To.Rotation().Yaw, 0.0));
			Zenny->GetCombat()->Attack();
			return true;
		}
		if (Run->Phase == 2 && World->GetTimeSeconds() - Run->PhaseAt > 3.0)
		{
			Finish(World, true);
			return false;
		}
		return true;
	}

	bool TickReport(UWorld* World)
	{
		if (World->GetTimeSeconds() - Run->PhaseAt > 4.0)
		{
			Finish(World, true);
			return false;
		}
		return true;
	}

	bool TickNavScale(UWorld* World)
	{
		// Zenny stands 60 m west of the room, outside its relevance margin: the region is active only through the
		// demand of active creatures inside it (patrols, and the extra ones kept investigating).
		const double InPhase = World->GetTimeSeconds() - Run->PhaseAt;
		UGLPlacementSubsystem* Placements = World->GetSubsystem<UGLPlacementSubsystem>();
		UGLNavRegionSubsystem* Regions = World->GetSubsystem<UGLNavRegionSubsystem>();
		for (AGLCreature* C : Run->Extra)
		{
			if (C && C->GetState() == EGLCreatureState::Idle)
			{
				C->HearNoise(World->GetSubsystem<UGLNoiseSubsystem>()->Make(TEXT("Noise.Salvage.Hit"), C->GetActorLocation() + FVector(300, 0, 0), ZennyOf(World)));
			}
		}
		auto Record = [World, Regions, Placements](const TCHAR* Key)
		{
			int32 Own = 0, Active = 0, Covered = 0, RoomOwn = 0;
			for (AGLCreature* C : Run->Extra)
			{
				Own += C && C->HasOwnNavigation() ? 1 : 0;
				Active += C && GLCreatureRules::NeedsNavigation(C->GetState()) ? 1 : 0;
				Covered += C && Regions->IsProvidedAt(C->GetActorLocation()) ? 1 : 0;
			}
			for (int32 I = DPFirstGremlin; I <= DPWarden; ++I)
			{
				const AGLCreature* C = Placements->FindCreature(DPId(I));
				RoomOwn += C && C->HasOwnNavigation() ? 1 : 0;
			}
			const int32 Tiles = ActiveTiles(World);
			Run->Out->SetNumberField(Key, Tiles);
			Log(FString::Printf(TEXT("%s: %d active tiles; extra creatures %d (%d active, %d covered by a region, %d with their own navigation); room creatures with their own navigation %d; active regions %d%s"),
				Key, Tiles, Run->Extra.Num(), Active, Covered, Own, RoomOwn, Regions->ActiveRegions(), Regions->bDevWithholdRegions ? TEXT(" (regions WITHHELD)") : TEXT("")));
		};
		if (InPhase < 8.0)
		{
			return true;
		}
		auto Spawn = [World, Placements](int32 I)
		{
			Run->Extra.Add(Placements->SpawnProofCreature(TEXT("creature.drain.static_gremlin"), RoomPoint(World, 200 + (I % 4) * 400, -1200 + (I / 4) * 400, 0.0), 0.0, DPLots));
		};
		switch (Run->Phase)
		{
		case 1:
			Record(TEXT("tilesRegionRoomCreatures"));
			Spawn(0);
			NextPhase(World);
			break;
		case 2:
			Record(TEXT("tilesRegionPlus1Active"));
			for (int32 I = 1; I < 16; ++I)
			{
				Spawn(I);
			}
			NextPhase(World);
			break;
		case 3:
			Record(TEXT("tilesRegionPlus16Active"));
			Regions->bDevWithholdRegions = true; // the contrast: the same creatures, each with its own invoker
			NextPhase(World);
			break;
		case 4:
			Record(TEXT("tilesNoRegionPlus16Active"));
			Regions->bDevWithholdRegions = false;
			Finish(World, true);
			return false;
		}
		return true;
	}

	void Start(const TArray<FString>& Args, UWorld* World)
	{
		Run = MakeShared<FDungeonRun>();
		Run->Mode = Args.Num() > 0 ? Args[0] : TEXT("environmental");
		Run->bInside = Args.Num() > 1 && Args[1] == TEXT("inside");
		Run->World = World;
		Run->Listener = World->GetSubsystem<UGLEventSubsystem>()->Subscribe(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event")),
			FGLGameplayEventDelegate::CreateLambda([](const FGLGameplayEvent& Event) { if (Run) { ++Run->Events.FindOrAdd(Event.Tag.GetTagName()); } }));
		Run->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			UWorld* W = Run ? Run->World.Get() : nullptr;
			AGLCharacter* Zenny = W ? ZennyOf(W) : nullptr;
			if (!Zenny)
			{
				return !!W;
			}
			UGLGridSubsystem* Grid = W->GetSubsystem<UGLGridSubsystem>();
			UGLPlacementSubsystem* Placements = W->GetSubsystem<UGLPlacementSubsystem>();
			if (Run->Phase == 0)
			{
				// Teleport to the room's entrance once, then wait for the lots to be in and presented.
				if (Run->PhaseAt == 0.0)
				{
					// No ground there yet: hold Zenny still (no falling, so no death and respawn elsewhere) until the lots are in.
					Zenny->GetCharacterMovement()->DisableMovement();
					// navscale measures navigation, not combat: the patrols must not kill Zenny out of the room.
					Zenny->GetHealth()->SetIgnoresDamage(Run->Mode == TEXT("navscale"));
					Zenny->SetActorLocation(RoomPoint(W, Run->Mode == TEXT("navscale") ? (Run->bInside ? -1200 : -8000) : -2400, Run->Mode == TEXT("navscale") ? (Run->bInside ? -400 : 0) : 400, 120.0), false, nullptr, ETeleportType::TeleportPhysics);
					Run->PhaseAt = W->GetTimeSeconds();
					return true;
				}
				if (!Grid->IsComplete(DPLots) || !Placements->IsCellPresented(DPLots) || W->GetTimeSeconds() - Run->PhaseAt < 3.0)
				{
					return true;
				}
				if (Run->Waypoint == 0)
				{
					// The first teleport landed before the lots' ground existed: stand on the real ground now, then settle.
					Zenny->SetActorLocation(RoomPoint(W, Run->Mode == TEXT("navscale") ? (Run->bInside ? -1200 : -8000) : -2400, Run->Mode == TEXT("navscale") ? (Run->bInside ? -400 : 0) : 400, 120.0), false, nullptr, ETeleportType::TeleportPhysics);
					Zenny->GetHealth()->Restore(Zenny->GetHealth()->GetMax());
					Zenny->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
					Run->Waypoint = 1;
					Run->PhaseAt = W->GetTimeSeconds();
					return true;
				}
				Run->Started = W->GetTimeSeconds();
				if (AGLPehlichi* Peh = PehlichiOf(W))
				{
					Peh->SetActorLocation(Zenny->GetActorLocation() + FVector(0, 200, 0));
				}
				Zenny->GetHealth()->OnDamaged.AddLambda([](double Taken, AActor*) { if (Run) { Run->ZennyDamage += Taken; } });
				if (AGLCreature* Warden = Placements->FindCreature(DPId(DPWarden)))
				{
					Warden->GetHealth()->OnDamaged.AddLambda([](double Taken, AActor*) { if (Run) { Run->WardenDamage += Taken; } });
				}
				if (!Grid->IsLoaded(DPLots) || (GLGridCells::CellAt(FVector2D(Zenny->GetActorLocation())) != DPLots && Run->Mode != TEXT("navscale")))
				{
					Log(TEXT("Zenny is not in the lots: the run is invalid"));
				}
				NextPhase(W, TEXT("room in and presented"));
			}
			if (W->GetTimeSeconds() >= Run->NextSample)
			{
				Run->NextSample = W->GetTimeSeconds() + 0.5;
				int32 Tasks = 0;
				Run->TilesPeak = FMath::Max(Run->TilesPeak, ActiveTiles(W, &Tasks));
				Run->TasksPeak = FMath::Max(Run->TasksPeak, Tasks);
			}
			const bool bContinue = Run->Mode == TEXT("direct") ? TickDirect(W) : Run->Mode == TEXT("report") ? TickReport(W)
				: Run->Mode == TEXT("navscale") ? TickNavScale(W) : TickEnvironmental(W);
			if (!bContinue)
			{
				Run.Reset();
			}
			return bContinue;
		}));
	}

	FAutoConsoleCommandWithWorldAndArgs DungeonProofCommand(TEXT("gl.Dungeon.Proof"),
		TEXT("DEV ONLY (P9, needs -GLDungeonProof): direct | environmental | report | navscale. Writes Saved/P9/dungeon-<mode>.json and quits (autosave)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));
}

#endif
