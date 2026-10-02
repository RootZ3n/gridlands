// P9 (ADR-0037, ADR-0029 as amended) against the dev proof room (-GLDungeonProof): creature gameplay facts live in
// the model (health, awareness, memory, position survive streaming and save/restart), DEFEATED and NEUTRALIZED are
// distinct durable outcomes, a model-first mechanism decides containment at its switch from where things are then,
// ambient noise masks hearing at the listener (lures included), and navigation scales with the authored region.

#include "Building/GLCollapseRules.h"
#include "Character/GLCharacter.h"
#include "Character/GLFootstepsComponent.h"
#include "Combat/GLCombatComponent.h"
#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "HAL/FileManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Mechanism/GLMechanism.h"
#include "Mechanism/GLMechanismSubsystem.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Noise/GLNoiseSubsystem.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLOperateComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLGridSubsystem.h"
#include "World/GLNavRegionSubsystem.h"
#include "World/GLPlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLEncounterTests
{
	const FName ENLots(TEXT("cell.outer.diner_lots"));
	const FString ENSlot(TEXT("gl-encounter-test"));
	const FVector ENFarAway(0, -1200, 100); // the origin: the lots stream out
	// Layout indices (GLDungeonProofLayout.inl): 0-19 walls, 20 grate, 21-24 patrol gremlins, 25 warden, 26 cage, 27 fan, 28 region.
	constexpr int32 ENFirstGremlin = 21;
	constexpr int32 ENWarden = 25;
	constexpr int32 ENCage = 26;
	constexpr int32 ENFan = 27;

	FName ENId(int32 Index) { return UGLPlacementSubsystem::DungeonProofId(Index); }

	struct FProofScene
	{
		GLTestUtils::FTestWorld Test;
		AGLCharacter* Zenny = nullptr;
		AGLPehlichi* Pehlichi = nullptr;
		UNavigationSystemV1* Nav = nullptr;
		UGLGridSubsystem* Grid = nullptr;
		UGLPlacementSubsystem* Placements = nullptr;
		UGLMechanismSubsystem* Mechanisms = nullptr;
		UGLTerrainSubsystem* Terrain = nullptr;
		UGLNavRegionSubsystem* Regions = nullptr;
		TArray<FName> Events;
		TArray<TPair<FName, double>> Numbers; // event tag, "neutralized" number

		FProofScene(const TCHAR* Name, FAutomationTestBase& Owner, bool bNavigation = false, bool bTownBlock = false, bool bStartFar = false) : Test(Name)
		{
			UWorld* World = Test.World;
			if (bNavigation)
			{
				Owner.AddExpectedMessagePlain(TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager"),
					ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
				FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode);
				Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			}
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event)
				{
					Events.Add(Event.Tag.GetTagName());
					Numbers.Add({ Event.Tag.GetTagName(), Event.Numbers.FindRef(TEXT("neutralized")) });
				}));
			Placements = World->GetSubsystem<UGLPlacementSubsystem>();
			Placements->AddDungeonProof(); // before any cell streams in
			if (bTownBlock)
			{
				Placements->AddTownBlock();
			}
			Mechanisms = World->GetSubsystem<UGLMechanismSubsystem>();
			Regions = World->GetSubsystem<UGLNavRegionSubsystem>();
			Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			Zenny = World->SpawnActor<AGLCharacter>(ENFarAway, FRotator::ZeroRotator);
			Pehlichi = World->SpawnActor<AGLPehlichi>(ENFarAway + FVector(0, 200, 0), FRotator::ZeroRotator);
			World->GetSubsystem<UGLGlitchSubsystem>()->SetCommander(Zenny);
			Grid = World->GetSubsystem<UGLGridSubsystem>();
			Grid->bShowBoundaries = false;
			Grid->Enable(false);
			GoTo(bStartFar ? ENFarAway : Room(-1500, 400)); // the entrance (a restart starts away, loads, then streams in)
		}

		/** A room-local point (cm) in world space, on the ground (lots-local origin UGLPlacementSubsystem::DungeonProofOrigin). */
		FVector Room(double X, double Y, double Up = 100.0) const
		{
			const FVector2D At(102400.0 + UGLPlacementSubsystem::DungeonProofOrigin.X + X, UGLPlacementSubsystem::DungeonProofOrigin.Y + Y);
			return FVector(At, (Terrain && Terrain->HasGroundAt(At) ? Terrain->HeightAt(At) : 0.0) + Up);
		}

		void GoTo(const FVector& Where)
		{
			Zenny->SetActorLocation(Where);
			Pehlichi->SetActorLocation(Where + FVector(0, 200, 0)); // he keeps up
			Grid->Advance(Where);
			Grid->FlushAll();
		}

		/** Streams toward Where like play until the lots' authoritative layer is in (bLoaded) or out. */
		bool StepUntil(const FVector& Where, bool bLoaded)
		{
			Zenny->SetActorLocation(Where);
			auto Done = [&] { return bLoaded ? Grid->IsRuntimeReady(ENLots) : !Grid->IsLoaded(ENLots); };
			for (int32 F = 0; F < 20000 && !Done(); ++F)
			{
				Grid->Advance(Where);
				if (!Done())
				{
					FPlatformProcess::Sleep(0.001f);
				}
			}
			return Done();
		}

		/** The world moves on by Seconds: clock, creatures, Pehlichi, combat, navigation (a bare test world ticks nothing). */
		void Step(double Seconds, double Dt = 0.05)
		{
			for (double T = 0.0; T < Seconds; T += Dt)
			{
				Test.World->TimeSeconds += Dt;
				for (TActorIterator<AGLCreature> It(Test.World); It; ++It)
				{
					if (IsValid(*It) && !It->IsHidden())
					{
						It->Think(Dt); // a bare world registers no tick functions; Think skips the defeated and the neutralized itself
					}
				}
				Pehlichi->GetPositioning()->Advance(Dt);
				Pehlichi->GetOperate()->Advance(Dt);
				Zenny->GetCombat()->Advance(Dt);
				if (Nav)
				{
					Nav->Tick(Dt);
				}
				Regions->Update(Zenny->GetActorLocation());
			}
		}

		AGLCreature* Creature(int32 Index) const { return Placements->FindCreature(ENId(Index)); }
		const FGLActorPlacement* Model(int32 Index) const { return Placements->FindActorModel(ENId(Index)); }
		int32 Count(const TCHAR* Tag) const { return Events.FilterByPredicate([Tag](FName E) { return E == FName(Tag); }).Num(); }
		bool Save() const { return Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(ENSlot); }
		bool Load(TArray<FString>& Problems) { return Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(ENSlot, &Problems); }

		/** Faces a creature toward a room point and puts it there (writing through, as its own step would). */
		void Place(AGLCreature* C, double X, double Y, double FaceYaw = 0.0)
		{
			C->SetActorLocationAndRotation(Room(X, Y, 70.0), FRotator(0.0, FaceYaw, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
			C->Think(0.0f);
		}

		int32 ActiveTiles() const
		{
			const ARecastNavMesh* R = Nav ? Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
			return R ? R->GetNumActiveTiles() : 0;
		}

		bool Settle()
		{
			for (int32 Tick = 0; Tick < 4000; ++Tick)
			{
				Step(0.05);
				if (Tick > 30 && !Nav->IsNavigationBuildInProgress() && !Nav->HasDirtyAreasQueued() && Nav->GetNumRemainingBuildTasks() == 0)
				{
					return true;
				}
				FPlatformProcess::Sleep(0.002f);
			}
			return false;
		}
	};

	/** Damage taken by a creature, and by whom (ADR-0017: never Pehlichi; P9: never on the environmental route). */
	struct FDamageLog
	{
		TArray<TWeakObjectPtr<AActor>> By;
		double Total = 0.0;
		void Watch(AGLCreature* C)
		{
			C->GetHealth()->OnDamaged.AddLambda([this](double Taken, AActor* Instigator) { Total += Taken; By.Add(Instigator); });
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLProofRoomStands, "Gridlands.Game.Encounter.ProofRoomStandsAndSpawnsModelsFirst", GLTestUtils::Flags)
bool FGLProofRoomStands::RunTest(const FString& Parameters)
{
	GLEncounterTests::FProofScene S(TEXT("GLProofStandsWorld"), *this);
	UGLStructureSubsystem* Structures = S.Test.World->GetSubsystem<UGLStructureSubsystem>();
	int32 Structs = 0;
	for (int32 I = 0; I < UGLPlacementSubsystem::DungeonProofCount(); ++I)
	{
		if (const FGLStructureRuntime* R = Structures->Find(GLEncounterTests::ENId(I)))
		{
			++Structs;
			TArray<FGLPlacedPiece> Pieces;
			for (const FGLStructurePartRuntime& P : R->Parts)
			{
				Pieces.Add(P.Piece);
			}
			TestEqual(*FString::Printf(TEXT("%s stands where it is authored"), *R->Placement.ToString()),
				GLCollapseRules::Unsupported(GLContent::Get(), Pieces, [&S](const FVector2D& At) { return S.Terrain->HeightAt(At); }).Num(), 0);
		}
	}
	TestEqual(TEXT("21 dev structures (20 walls, the grate)"), Structs, 21);
	for (int32 I = GLEncounterTests::ENFirstGremlin; I <= GLEncounterTests::ENWarden; ++I)
	{
		const FGLActorPlacement* M = S.Model(I);
		if (TestNotNull(*FString::Printf(TEXT("creature model %d"), I), M))
		{
			TestTrue(TEXT("  on the ground"), FMath::Abs(M->Location.Z - S.Terrain->HeightAt(FVector2D(M->Location))) < 60.0);
			TestTrue(TEXT("  calm, full health, no outcome"), M->Creature.Outcome == EGLCreatureOutcome::None && M->Creature.Health < 0.0);
		}
	}
	TestEqual(TEXT("the patrols have their loops"), S.Model(GLEncounterTests::ENFirstGremlin)->Creature.Patrol.Num(), 2);
	TestNotNull(TEXT("the cage's record"), S.Mechanisms->FindRecord(GLEncounterTests::ENId(GLEncounterTests::ENCage)));
	TestNotNull(TEXT("the fan's record"), S.Mechanisms->FindRecord(GLEncounterTests::ENId(GLEncounterTests::ENFan)));
	TestTrue(TEXT("the room is one authored navigation region"), S.Regions->GetRegions().Contains(GLEncounterTests::ENId(28)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLWoundedStaysWounded, "Gridlands.Game.Encounter.AWoundedCreatureStaysWoundedAcrossStreamingAndRestart", GLTestUtils::Flags)
bool FGLWoundedStaysWounded::RunTest(const FString& Parameters)
{
	GLEncounterTests::FProofScene S(TEXT("GLWoundedWorld"), *this);
	AGLCreature* Warden = S.Creature(GLEncounterTests::ENWarden);
	if (!TestNotNull(TEXT("the warden"), Warden))
	{
		return false;
	}
	Warden->GetHealth()->ApplyDamage(90.0, S.Zenny);
	TestEqual(TEXT("its model holds the wound"), S.Model(GLEncounterTests::ENWarden)->Creature.Health, 150.0);
	S.GoTo(GLEncounterTests::ENFarAway);
	S.GoTo(S.Room(-1500, 400));
	TestEqual(TEXT("streamed out and back: still wounded"), S.Creature(GLEncounterTests::ENWarden)->GetHealth()->GetCurrent(), 150.0);
	TestTrue(TEXT("saves"), S.Save());
	GLEncounterTests::FProofScene R(TEXT("GLWoundedRestartWorld"), *this, false, false, true);
	TArray<FString> Problems;
	TestTrue(TEXT("restart loads"), R.Load(Problems));
	TestEqual(TEXT("no load problems"), Problems.Num(), 0);
	R.GoTo(R.Room(-1500, 400));
	TestEqual(TEXT("after restart: still wounded (no free full heal)"), R.Creature(GLEncounterTests::ENWarden)->GetHealth()->GetCurrent(), 150.0);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(GLEncounterTests::ENSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLMemorySurvives, "Gridlands.Game.Encounter.ChaseAndSearchSurviveStreamingAndRestart", GLTestUtils::Flags)
bool FGLMemorySurvives::RunTest(const FString& Parameters)
{
	GLEncounterTests::FProofScene S(TEXT("GLMemoryWorld"), *this);
	AGLCreature* Warden = S.Creature(GLEncounterTests::ENWarden);
	if (!TestNotNull(TEXT("the warden"), Warden))
	{
		return false;
	}
	// It sees Zenny 6 m in front of it, away from home: a chase.
	S.Place(Warden, 600, -300, 0.0);
	S.Zenny->SetActorLocation(S.Room(1200, -300));
	S.Step(0.2);
	TestTrue(TEXT("it chases"), Warden->GetState() == EGLCreatureState::Chase || Warden->GetState() == EGLCreatureState::Attack);
	// Zenny ducks out of sight (back through the main door, behind the divider): it searches his last known spot.
	S.Zenny->SetActorLocation(S.Room(-1200, -1300));
	S.Step(0.2);
	TestEqual(TEXT("sight lost: it searches"), Warden->GetState(), EGLCreatureState::Search);
	const FVector Known = Warden->GetLastKnown();
	const double Left = Warden->GetSearchSecondsLeft();
	TestTrue(TEXT("  with memory left"), Left > 5.0);
	const FVector Where = S.Model(GLEncounterTests::ENWarden)->Location;
	TestTrue(TEXT("  and it is not at home"), FVector::Dist2D(Where, S.Model(GLEncounterTests::ENWarden)->Creature.Home) > 200.0);

	// Streaming out for 4 s of world time: the search keeps running, it is not erased.
	S.StepUntil(GLEncounterTests::ENFarAway, false);
	S.Test.World->TimeSeconds += 4.0;
	S.GoTo(S.Room(-1500, 400));
	AGLCreature* Back = S.Creature(GLEncounterTests::ENWarden);
	TestEqual(TEXT("back: still searching"), Back->GetState(), EGLCreatureState::Search);
	TestTrue(TEXT("  where Zenny was"), Back->GetLastKnown().Equals(Known, 1.0));
	TestTrue(TEXT("  with the time away spent (about 4 s less)"), FMath::IsNearlyEqual(Back->GetSearchSecondsLeft(), Left - 4.0, 0.6));
	TestTrue(TEXT("  from where it was, not home"), FVector::Dist2D(S.Model(GLEncounterTests::ENWarden)->Location, Where) < 60.0);

	// Save mid-search, restart: the same search resumes (reload is not a memory wipe).
	const double AtSave = Back->GetSearchSecondsLeft();
	TestTrue(TEXT("saves"), S.Save());
	GLEncounterTests::FProofScene R(TEXT("GLMemoryRestartWorld"), *this, false, false, true);
	TArray<FString> Problems;
	TestTrue(TEXT("restart loads"), R.Load(Problems));
	R.GoTo(R.Room(-1500, 400));
	AGLCreature* Resumed = R.Creature(GLEncounterTests::ENWarden);
	TestEqual(TEXT("after restart: still searching"), Resumed->GetState(), EGLCreatureState::Search);
	TestTrue(TEXT("  the same memory"), Resumed->GetLastKnown().Equals(Known, 1.0) && FMath::IsNearlyEqual(Resumed->GetSearchSecondsLeft(), AtSave, 0.6));
	TestTrue(TEXT("  not teleported home"), FVector::Dist2D(R.Model(GLEncounterTests::ENWarden)->Location, Where) < 60.0);
	// And the search ends as searches do: memory spent, it goes back to calm.
	R.Step(AtSave + 1.0);
	TestFalse(TEXT("memory spent: it gives up"), Resumed->GetState() == EGLCreatureState::Search);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(GLEncounterTests::ENSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLModelHears, "Gridlands.Game.Encounter.AnUnpresentedCreatureHearsByTheSameRule", GLTestUtils::Flags)
bool FGLModelHears::RunTest(const FString& Parameters)
{
	GLEncounterTests::FProofScene S(TEXT("GLModelHearsWorld"), *this);
	S.GoTo(GLEncounterTests::ENFarAway);
	// Back with presentation held: the models are in, the actors are not.
	S.Grid->PresentationBudgetMs = -1.f;
	S.Grid->PresentationNearM = 0.f;
	TestTrue(TEXT("the lots' authoritative layer is in"), S.StepUntil(S.Room(-1500, 400), true));
	TestNull(TEXT("the warden is not presented yet"), S.Creature(GLEncounterTests::ENWarden));
	UGLNoiseSubsystem* Noise = S.Test.World->GetSubsystem<UGLNoiseSubsystem>();
	const FVector Home = S.Model(GLEncounterTests::ENWarden)->Location;
	const int32 Heard = Noise->Emit(Noise->Make(TEXT("Noise.Salvage.Hit"), Home + FVector(500, 0, 0), S.Zenny));
	const FGLCreatureModel& W = S.Model(GLEncounterTests::ENWarden)->Creature;
	TestTrue(TEXT("its model heard a 10 m noise 5 m away"), Heard >= 1 && W.State == EGLCreatureState::Investigate && W.NoiseSeconds > 0.0);
	TestTrue(TEXT("  and remembers where"), W.Noise.Equals(Home + FVector(500, 0, 0), 1.0));
	const int32 FarHeard = Noise->Emit(Noise->Make(TEXT("Noise.Salvage.Hit"), Home + FVector(3000, 0, 0), S.Zenny));
	TestEqual(TEXT("a noise 30 m away reaches nobody's model"), FarHeard, 0);
	// Presented now: the actor starts from what the model heard.
	S.Grid->PresentationBudgetMs = 1.5f;
	S.Placements->PresentActor(GLEncounterTests::ENId(GLEncounterTests::ENWarden));
	AGLCreature* Warden = S.Creature(GLEncounterTests::ENWarden);
	TestTrue(TEXT("made investigating, with the memory its model kept"), Warden && Warden->GetState() == EGLCreatureState::Investigate && Warden->GetNoiseSecondsLeft() > 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLMaskingIsGeneral, "Gridlands.Game.Encounter.AmbientMaskingAppliesToStepsAndLuresAlike", GLTestUtils::Flags)
bool FGLMaskingIsGeneral::RunTest(const FString& Parameters)
{
	GLEncounterTests::FProofScene S(TEXT("GLMaskingWorld"), *this);
	UGLNoiseSubsystem* Noise = S.Test.World->GetSubsystem<UGLNoiseSubsystem>();
	const FGLMechanismRecord* Fan = S.Mechanisms->FindRecord(GLEncounterTests::ENId(GLEncounterTests::ENFan));
	// A guard 4 m from the grate, inside the fan's 10 m radius.
	AGLCreature* Guard = S.Creature(GLEncounterTests::ENFirstGremlin + 1);
	S.Place(Guard, -900, 600, 90.0);
	const FVector GuardAt = Guard->GetActorLocation();
	// The fan's duty cycle: on for 8 s of every 12 s (world clock, deterministic).
	S.Test.World->TimeSeconds = 1.0;
	TestTrue(TEXT("the fan is sounding at t = 1 s"), UGLMechanismSubsystem::IsAmbientOn(*Fan, 1.0));
	TestEqual(TEXT("the mask at the guard is the fan's (0.8)"), Noise->MaskAt(GuardAt), 0.8, 1e-6);
	// Zenny steps on the grate (sheet steel: 3 x 2.5 m = 7.5 m) 4 m from the guard.
	S.Zenny->SetActorLocation(S.Room(-900, 1000));
	FGLNoiseEvent Step = Noise->Make(TEXT("Noise.Move.Walk"), S.Room(-900, 1000), S.Zenny, TEXT("material.proof.sheet_steel"));
	TestEqual(TEXT("a footstep on the grate carries 7.5 m"), Step.RadiusCm, 750.0, 1e-3);
	TestFalse(TEXT("under the fan: not heard (7.5 m x 0.2 = 1.5 m < 4 m)"), Guard->HearNoise(Step));
	// Pehlichi's lure at the same spot: the same rule, no exemption.
	FGLNoiseEvent Lure = Noise->Make(TEXT("Noise.Pehlichi.Lure"), S.Room(-900, 1000), S.Pehlichi);
	Lure.bDistraction = true;
	Lure.InvestigateSeconds = 6.0;
	TestFalse(TEXT("the lure is masked too (min(40 m, hearing 16 m) x 0.2 = 3.2 m < 4 m)"), Guard->HearNoise(Lure));
	// Masking is judged at the LISTENER: a 10 m salvage noise under the fan (2.5 m from it) reaches a guard 9.5 m away
	// that stands outside the fan's radius (12 m from it): heard.
	AGLCreature* Outside = S.Creature(GLEncounterTests::ENFirstGremlin);
	S.Place(Outside, -900, -50, 90.0);
	TestEqual(TEXT("no mask at the outside guard"), Noise->MaskAt(Outside->GetActorLocation()), 0.0, 1e-6);
	TestTrue(TEXT("a masked source is still heard by an unmasked listener"), Outside->HearNoise(Noise->Make(TEXT("Noise.Salvage.Hit"), S.Room(-900, 900), S.Zenny)));
	// In the fan's quiet window (t = 9 s) both carry again.
	S.Test.World->TimeSeconds = 9.0;
	TestFalse(TEXT("the fan is quiet at t = 9 s"), UGLMechanismSubsystem::IsAmbientOn(*Fan, 9.0));
	TestTrue(TEXT("the footstep is heard"), Guard->HearNoise(Step));
	TestTrue(TEXT("the lure is heard"), Guard->HearNoise(Lure));
	// Switched off by its mechanism: no mask at any time.
	S.Test.World->TimeSeconds = 1.0;
	TestTrue(TEXT("Pehlichi's operation turns the fan off"), S.Mechanisms->Switch(Fan->Placement, TEXT("off"), S.Pehlichi));
	TestEqual(TEXT("no mask any more"), Noise->MaskAt(GuardAt), 0.0, 1e-6);
	TestTrue(TEXT("the lure works now"), Guard->HearNoise(Lure));
	// Ambient noise is never a stimulus: nothing heard the fan itself.
	TestEqual(TEXT("no noise event for the fan's sound"), Noise->CountOf(TEXT("Noise.Mechanism.Slam")), 0);
	// Zenny's real footsteps component uses the floor's material.
	TestEqual(TEXT("on terrain a step has no material"), S.Zenny->GetFootsteps()->FloorMaterial(), FName());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDirectRoute, "Gridlands.Game.Encounter.DirectRouteDefeatsTheWardenThroughHealth", GLTestUtils::Flags)
bool FGLDirectRoute::RunTest(const FString& Parameters)
{
	GLEncounterTests::FProofScene S(TEXT("GLDirectWorld"), *this);
	AGLCreature* Warden = S.Creature(GLEncounterTests::ENWarden);
	if (!TestNotNull(TEXT("the warden"), Warden))
	{
		return false;
	}
	const int32 ResidueBefore = S.Zenny->GetInventory()->CountOf(TEXT("item.material.static_residue"));
	// Zenny fights it: fists, 8 per hit, at its face.
	S.Place(Warden, 1000, -300, 180.0);
	S.Zenny->SetActorLocationAndRotation(S.Room(900, -300), FRotator::ZeroRotator);
	int32 Hits = 0;
	for (int32 Swing = 0; Swing < 100 && Warden->IsActiveHostile(); ++Swing)
	{
		Hits += S.Zenny->GetCombat()->Attack() ? 1 : 0;
		S.Zenny->GetCombat()->Advance(1.0f);
	}
	TestTrue(TEXT("the warden is defeated through the health system"), S.Placements->IsCreatureDefeated(GLEncounterTests::ENId(GLEncounterTests::ENWarden)));
	TestEqual(TEXT("  30 fist hits (240 / 8)"), Hits, 30);
	TestEqual(TEXT("Outcome = Defeated"), S.Model(GLEncounterTests::ENWarden)->Creature.Outcome, EGLCreatureOutcome::Defeated);
	TestEqual(TEXT("a normal defeat event"), S.Count(TEXT("Event.Creature.Defeated")), 1);
	TestEqual(TEXT("never a neutralized one"), S.Count(TEXT("Event.Creature.Neutralized")), 0);
	TestEqual(TEXT("the encounter resolves once"), S.Count(TEXT("Event.Encounter.Resolved")), 1);
	TestTrue(TEXT("  as a defeat"), S.Numbers.ContainsByPredicate([](const TPair<FName, double>& N) { return N.Key == FName(TEXT("Event.Encounter.Resolved")) && N.Value == 0.0; }));
	TestEqual(TEXT("its encounter reward"), S.Zenny->GetInventory()->CountOf(TEXT("item.material.static_residue")) - ResidueBefore, 5);
	TestTrue(TEXT("saves"), S.Save());
	GLEncounterTests::FProofScene R(TEXT("GLDirectRestartWorld"), *this, false, false, true);
	TArray<FString> Problems;
	TestTrue(TEXT("restart loads"), R.Load(Problems));
	const int32 EventsBefore = R.Events.Num();
	R.GoTo(R.Room(-1500, 400));
	TestTrue(TEXT("after restart: still defeated"), R.Placements->IsCreatureDefeated(GLEncounterTests::ENId(GLEncounterTests::ENWarden)));
	TestNull(TEXT("  with no actor (the P8 rule for Defeated)"), R.Creature(GLEncounterTests::ENWarden));
	TestEqual(TEXT("  and nothing replays"), R.Count(TEXT("Event.Encounter.Resolved")) + R.Count(TEXT("Event.Creature.Defeated")), 0);
	TestEqual(TEXT("  the reward was paid once (restored from the save, not paid again)"), R.Zenny->GetInventory()->CountOf(TEXT("item.material.static_residue")), 5);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(GLEncounterTests::ENSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLEnvironmentalRoute, "Gridlands.Game.Encounter.EnvironmentalRouteNeutralizesWithoutDamage", GLTestUtils::Flags)
bool FGLEnvironmentalRoute::RunTest(const FString& Parameters)
{
	GLEncounterTests::FProofScene S(TEXT("GLEnvironmentalWorld"), *this);
	AGLCreature* Warden = S.Creature(GLEncounterTests::ENWarden);
	if (!TestNotNull(TEXT("the warden"), Warden))
	{
		return false;
	}
	GLEncounterTests::FDamageLog Wounds;
	Wounds.Watch(Warden);
	const FName Cage = GLEncounterTests::ENId(GLEncounterTests::ENCage);
	const FBox Zone = UGLMechanismSubsystem::NeutralizeBox(*S.Mechanisms->FindRecord(Cage));
	// Zenny stands at the cage zone's edge; the warden comes for him and fights him inside the zone.
	S.Zenny->SetActorLocation(S.Room(1300, -750));
	S.Place(Warden, 1300, -900, 90.0);
	S.Step(0.3);
	TestTrue(TEXT("the warden is attacking Zenny"), Warden->GetState() == EGLCreatureState::Attack || Warden->GetState() == EGLCreatureState::Chase);
	// Zenny sends Pehlichi to the control behind the wall (only Pehlichi reaches it).
	const EGLCommandRejection Order = S.Pehlichi->GetCommands()->Issue(TEXT("Command.Pehlichi.Operate"), S.Zenny);
	TestEqual(TEXT("Pehlichi takes the order"), Order, EGLCommandRejection::None);
	const double ZennyBefore = S.Zenny->GetHealth()->GetCurrent();
	double Seconds = 0.0;
	for (; Seconds < 30.0 && !S.Placements->IsEncounterResolved(GLEncounterTests::ENId(GLEncounterTests::ENWarden)); Seconds += 0.1)
	{
		S.Step(0.1);
	}
	AddInfo(FString::Printf(TEXT("the cage dropped after %.1f s; Zenny took %.0f damage meanwhile"), Seconds, ZennyBefore - S.Zenny->GetHealth()->GetCurrent()));
	TestTrue(TEXT("Zenny was in real danger the whole time (the warden hit him)"), S.Zenny->GetHealth()->GetCurrent() < ZennyBefore);
	const FGLCreatureModel& M = S.Model(GLEncounterTests::ENWarden)->Creature;
	TestEqual(TEXT("Outcome = Neutralized"), M.Outcome, EGLCreatureOutcome::Neutralized);
	TestEqual(TEXT("  Neutralize.Contained"), M.NeutralizedHow, FName(TEXT("Neutralize.Contained")));
	TestEqual(TEXT("  by the cage"), M.NeutralizedBy, Cage);
	TestTrue(TEXT("  held inside the zone"), Zone.IsInsideOrOn(M.HeldAt));
	TestEqual(TEXT("ZERO damage to the warden (Zenny, Pehlichi, mechanism)"), Wounds.Total, 0.0);
	TestEqual(TEXT("  full health: no fake damage"), Warden->GetHealth()->GetCurrent(), Warden->GetHealth()->GetMax());
	TestEqual(TEXT("no death event"), S.Count(TEXT("Event.Creature.Defeated")), 0);
	TestFalse(TEXT("no kill: it is not defeated"), S.Placements->IsCreatureDefeated(GLEncounterTests::ENId(GLEncounterTests::ENWarden)));
	TestEqual(TEXT("its own event, once"), S.Count(TEXT("Event.Creature.Neutralized")), 1);
	TestEqual(TEXT("the encounter resolves once"), S.Count(TEXT("Event.Encounter.Resolved")), 1);
	TestTrue(TEXT("  as neutralized"), S.Numbers.ContainsByPredicate([](const TPair<FName, double>& N) { return N.Key == FName(TEXT("Event.Encounter.Resolved")) && N.Value == 1.0; }));
	TestEqual(TEXT("its encounter reward, the same as a defeat"), S.Zenny->GetInventory()->CountOf(TEXT("item.material.static_residue")), 5);
	TestTrue(TEXT("presented contained: alive, inert, still there"), Warden->IsNeutralized() && !Warden->IsActorTickEnabled() && !Warden->IsHidden());
	const double ZennyAfter = S.Zenny->GetHealth()->GetCurrent();
	S.Step(3.0);
	TestEqual(TEXT("it never strikes again"), S.Zenny->GetHealth()->GetCurrent(), ZennyAfter);
	TestFalse(TEXT("Zenny's fists find no target in it"), S.Zenny->GetCombat()->Attack() == Warden);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDecisionMoment, "Gridlands.Game.Encounter.ContainmentReflectsTheWorldAtTheDecision", GLTestUtils::Flags)
bool FGLDecisionMoment::RunTest(const FString& Parameters)
{
	GLEncounterTests::FProofScene S(TEXT("GLDecisionWorld"), *this);
	AGLCreature* Warden = S.Creature(GLEncounterTests::ENWarden);
	AGLCreature* Gremlin = S.Creature(GLEncounterTests::ENFirstGremlin + 3);
	if (!TestTrue(TEXT("the warden and a gremlin"), Warden && Gremlin))
	{
		return false;
	}
	const FName Cage = GLEncounterTests::ENId(GLEncounterTests::ENCage);
	// The warden 5 m outside the zone, a (non-susceptible) gremlin inside it, when the cage drops.
	S.Place(Warden, 1300, -300);
	S.Place(Gremlin, 1300, -800);
	TestTrue(TEXT("the cage drops"), S.Mechanisms->Switch(Cage, TEXT("dropped"), S.Pehlichi));
	TestFalse(TEXT("outside the volume at the decision: not neutralized"), S.Placements->IsCreatureNeutralized(GLEncounterTests::ENId(GLEncounterTests::ENWarden)));
	TestFalse(TEXT("inside but not susceptible: not neutralized"), S.Placements->IsCreatureNeutralized(GLEncounterTests::ENId(GLEncounterTests::ENFirstGremlin + 3)));
	TestEqual(TEXT("no neutralized event"), S.Count(TEXT("Event.Creature.Neutralized")), 0);
	// The warden walks into the dropped cage's zone afterwards: never retroactive.
	S.Place(Warden, 1300, -800);
	S.Step(2.0);
	TestFalse(TEXT("entering after the decision changes nothing"), S.Placements->IsCreatureNeutralized(GLEncounterTests::ENId(GLEncounterTests::ENWarden)));
	TestTrue(TEXT("  it is still an active hostile"), Warden->IsActiveHostile());
	// And a dropped cage cannot be operated again (its operation goes raised -> dropped only).
	const FGLMechanismRecord* Dropped = S.Mechanisms->FindRecord(Cage);
	TestNull(TEXT("the dropped cage is no longer operable"), S.Mechanisms->FindOperable(Dropped->Location, 50.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLContainedStays, "Gridlands.Game.Encounter.NeutralizedStaysContainedAndNothingReplays", GLTestUtils::Flags)
bool FGLContainedStays::RunTest(const FString& Parameters)
{
	GLEncounterTests::FProofScene S(TEXT("GLContainedWorld"), *this);
	AGLCreature* Warden = S.Creature(GLEncounterTests::ENWarden);
	if (!TestNotNull(TEXT("the warden"), Warden))
	{
		return false;
	}
	const FName Cage = GLEncounterTests::ENId(GLEncounterTests::ENCage);
	S.Place(Warden, 1300, -800);
	S.Mechanisms->Switch(Cage, TEXT("dropped"), S.Pehlichi);
	const FGLCreatureModel Held = S.Model(GLEncounterTests::ENWarden)->Creature;
	if (!TestEqual(TEXT("contained"), Held.Outcome, EGLCreatureOutcome::Neutralized))
	{
		return false;
	}
	UGLNoiseSubsystem* Noise = S.Test.World->GetSubsystem<UGLNoiseSubsystem>();
	auto CheckContained = [&](GLEncounterTests::FProofScene& X, const TCHAR* When, int32 EventsBefore, int32 NoiseBefore)
	{
		AGLCreature* W = X.Creature(GLEncounterTests::ENWarden);
		TestTrue(*FString::Printf(TEXT("%s: still neutralized"), When), X.Placements->IsCreatureNeutralized(GLEncounterTests::ENId(GLEncounterTests::ENWarden)));
		TestTrue(*FString::Printf(TEXT("%s: visibly contained (an actor, held, inert)"), When), W && W->IsNeutralized() && !W->IsHidden() && !W->IsActorTickEnabled()
			&& FVector::Dist(W->GetActorLocation() - FVector(0, 0, 70), Held.HeldAt) < 50.0);
		const FGLMechanismRecord* C = X.Mechanisms->FindRecord(Cage);
		TestTrue(*FString::Printf(TEXT("%s: the cage is down"), When), C && C->State == TEXT("dropped") && C->Actor.IsValid() && C->Actor->GetCageLift() == 0.0);
		TestEqual(*FString::Printf(TEXT("%s: nothing replays (operate, switch, neutralize, resolve)"), When), X.Events.Num() - EventsBefore, 0);
		TestEqual(*FString::Printf(TEXT("%s: no slam noise again"), When), X.Test.World->GetSubsystem<UGLNoiseSubsystem>()->CountOf(TEXT("Noise.Mechanism.Slam")) - NoiseBefore, 0);
		TestEqual(*FString::Printf(TEXT("%s: the reward was paid once"), When), X.Zenny->GetInventory()->CountOf(TEXT("item.material.static_residue")), 5);
		// Re-made from its model, it never resumes hostile behaviour.
		const double Hp = X.Zenny->GetHealth()->GetCurrent();
		X.Zenny->SetActorLocation(W ? W->GetActorLocation() + FVector(120, 0, 0) : X.Room(0, 0));
		X.Step(3.0);
		TestEqual(*FString::Printf(TEXT("%s: it never strikes"), When), X.Zenny->GetHealth()->GetCurrent(), Hp);
	};
	// Streaming out and back (presentation recreation).
	S.GoTo(GLEncounterTests::ENFarAway);
	int32 Events = S.Events.Num();
	int32 Slams = Noise->CountOf(TEXT("Noise.Mechanism.Slam"));
	S.GoTo(S.Room(-1500, 400));
	CheckContained(S, TEXT("streamed back"), Events, Slams);
	// Save, restart.
	TestTrue(TEXT("saves"), S.Save());
	GLEncounterTests::FProofScene R(TEXT("GLContainedRestartWorld"), *this, false, false, true);
	TArray<FString> Problems;
	TestTrue(TEXT("restart loads"), R.Load(Problems));
	TestEqual(TEXT("no load problems"), Problems.Num(), 0);
	Events = R.Events.Num();
	R.GoTo(R.Room(-1500, 400));
	CheckContained(R, TEXT("after restart"), Events, 0);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(GLEncounterTests::ENSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLRegionNavigation, "Gridlands.Game.Encounter.NavigationScalesWithTheRegionNotTheCreatures", GLTestUtils::Flags)
bool FGLRegionNavigation::RunTest(const FString& Parameters)
{
	GLEncounterTests::FProofScene S(TEXT("GLRegionNavWorld"), *this, true);
	const FVector Hall = S.Room(-1200, -400);
	S.GoTo(Hall);
	if (!TestNotNull(TEXT("navigation system"), S.Nav) || !TestTrue(TEXT("settles"), S.Settle()))
	{
		return false;
	}
	TestTrue(TEXT("Zenny inside: the region provides navigation"), S.Regions->IsProvidedAt(S.Room(1500, 1000, 0.0)));
	int32 OwnNav = 0;
	for (TActorIterator<AGLCreature> It(S.Test.World); It; ++It)
	{
		OwnNav += It->HasOwnNavigation() ? 1 : 0;
	}
	TestEqual(TEXT("no creature in the region carries its own navigation (patrolling or not)"), OwnNav, 0);
	const int32 OneRegion = S.ActiveTiles();
	// Sixteen more active creatures in the region: the same coverage.
	UGLPlacementSubsystem* P = S.Placements;
	TArray<AGLCreature*> Extra;
	for (int32 I = 0; I < 16; ++I)
	{
		AGLCreature* C = P->SpawnProofCreature(TEXT("creature.drain.static_gremlin"), S.Room(200 + (I % 4) * 400, -1200 + (I / 4) * 400, 0.0), 0.0, GLEncounterTests::ENLots);
		if (C)
		{
			C->HearNoise(S.Test.World->GetSubsystem<UGLNoiseSubsystem>()->Make(TEXT("Noise.Salvage.Hit"), C->GetActorLocation() + FVector(200, 0, 0), S.Zenny));
			C->SetActorTickEnabled(true);
			Extra.Add(C);
		}
	}
	S.Settle();
	int32 Active = 0, ExtraOwn = 0;
	for (AGLCreature* C : Extra)
	{
		Active += GLCreatureRules::NeedsNavigation(C->GetState()) ? 1 : 0;
		ExtraOwn += C->HasOwnNavigation() ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("tiles: region with the room's 5 creatures %d; with 16 more active ones %d (%d active, %d with their own navigation)"), OneRegion, S.ActiveTiles(), Active, ExtraOwn));
	TestTrue(TEXT("they are active (investigating)"), Active >= 12);
	TestEqual(TEXT("none carries its own navigation inside the region"), ExtraOwn, 0);
	TestTrue(TEXT("coverage does not grow with creature count (within 5%)"), S.ActiveTiles() <= OneRegion * 1.05);
	// Outside any region an active creature does carry its own.
	AGLCreature* Outside = P->SpawnProofCreature(TEXT("creature.drain.static_gremlin"), S.Room(-12000, -400, 0.0), 0.0, GLEncounterTests::ENLots);
	Outside->HearNoise(S.Test.World->GetSubsystem<UGLNoiseSubsystem>()->Make(TEXT("Noise.Salvage.Hit"), Outside->GetActorLocation() + FVector(200, 0, 0), S.Zenny));
	Outside->Think(0.05f);
	TestTrue(TEXT("an active creature outside any region carries its own navigation"), Outside->HasOwnNavigation());
	// Zenny leaves and nothing inside needs it: the region stops providing.
	for (AGLCreature* C : Extra)
	{
		C->Destroy();
	}
	Outside->Destroy();
	S.GoTo(GLEncounterTests::ENFarAway);
	S.Step(2.0);
	TestEqual(TEXT("far away: no region active"), S.Regions->ActiveRegions(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLTownIdleNavigation, "Gridlands.Game.Encounter.IdleCreaturesAddNoNavigation", GLTestUtils::Flags)
bool FGLTownIdleNavigation::RunTest(const FString& Parameters)
{
	// The P8 town block's creatures, idle at home.
	GLEncounterTests::FProofScene S(TEXT("GLTownIdleNavWorld"), *this, true, true);
	S.GoTo(FVector(63900, -30000, 100));
	if (!TestTrue(TEXT("settles in the town"), S.Settle()))
	{
		return false;
	}
	int32 Town = 0, Own = 0;
	for (TActorIterator<AGLCreature> It(S.Test.World); It; ++It)
	{
		if (It->GetPlacementId().ToString().Contains(TEXT("proof_town")))
		{
			++Town;
			Own += It->HasOwnNavigation() ? 1 : 0;
		}
	}
	TestEqual(TEXT("the town's 3 creatures are there"), Town, 3);
	TestEqual(TEXT("idle at home, none pays for navigation of its own"), Own, 0);
	AddInfo(FString::Printf(TEXT("tiles in the town with its idle creatures: %d"), S.ActiveTiles()));
	return true;
}

#endif
