#include "Combat/GLCombatComponent.h"
#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Dialogue/GLDialogueDirector.h"
#include "Engine/StaticMeshActor.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "HAL/FileManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
#include "Pehlichi/GLRepairComponent.h"
#include "Pehlichi/GLScanComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Storm/GLStormArtifact.h"
#include "Storm/GLStormSubsystem.h"
#include "EngineUtils.h"
#include "Tests/GLTestUtils.h"
#include "World/GLAmbientSubsystem.h"
#include "World/GLPlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLSliceTests
{
	const FName GremlinDef(TEXT("creature.drain.static_gremlin"));
	const FString SliceSlot = TEXT("automation-test-slice");

	/** An arena with a pawn Zenny (inventory, health, combat), Pehlichi, and event capture. */
	struct FArena
	{
		GLTestUtils::FTestWorld Test;
		ACharacter* Zenny = nullptr;
		UGLInventoryComponent* Inventory = nullptr;
		UGLHealthComponent* Health = nullptr;
		UGLCombatComponent* Combat = nullptr;
		AGLPehlichi* Pehlichi = nullptr;
		TArray<FName> Events;
		TArray<FName> Exchanges;

		explicit FArena(const TCHAR* Name, bool bSpawnCell = false) : Test(Name)
		{
			UWorld* World = Test.World;
			World->GetSubsystem<UGLDialogueDirector>()->bShowOnScreen = false;
			World->GetSubsystem<UGLDialogueDirector>()->OnLine.AddLambda([this](const FGLDialogueLine& Line) { Exchanges.AddUnique(Line.ExchangeId); });
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event) { Events.Add(Event.Tag.GetTagName()); }));
			if (bSpawnCell)
			{
				World->GetSubsystem<UGLPlacementSubsystem>()->SpawnCell(TEXT("cell.home.origin"));
			}
			World->GetWorldSettings()->NotifyBeginPlay(); // components registered from here on begin play
			Zenny = World->SpawnActor<ACharacter>(FVector(0, 0, 100), FRotator::ZeroRotator);
			Inventory = NewObject<UGLInventoryComponent>(Zenny);
			Inventory->RegisterComponent();
			Health = NewObject<UGLHealthComponent>(Zenny);
			Health->RegisterComponent();
			Combat = NewObject<UGLCombatComponent>(Zenny);
			Combat->RegisterComponent();
			World->GetSubsystem<UGLGlitchSubsystem>()->SetCommander(Zenny);
			Pehlichi = World->SpawnActor<AGLPehlichi>(FVector(-300, 300, 100), FRotator::ZeroRotator);
		}

		~FArena()
		{
			Test.World->EndPlay(EEndPlayReason::Quit);
		}

		AGLCreature* Gremlin(const FVector& At, double Yaw)
		{
			AGLCreature* Creature = Test.World->SpawnActor<AGLCreature>(At, FRotator(0.0, Yaw, 0.0));
			Creature->Setup(GremlinDef, TEXT("placement.test.gremlin"));
			Creature->SetActorTickEnabled(false); // the test steps it
			return Creature;
		}

		int32 Count(const TCHAR* Tag) const { return Events.FilterByPredicate([Tag](FName E) { return E == FName(Tag); }).Num(); }

		/** Storm artifacts actually in the world (not just the ones the storm still tracks). */
		int32 ArtifactsInWorld() const
		{
			int32 Alive = 0;
			for (TActorIterator<AGLStormArtifact> It(Test.World); It; ++It)
			{
				Alive += IsValid(*It) ? 1 : 0;
			}
			return Alive;
		}
	};
}

using namespace GLSliceTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSliceFight, "Gridlands.Game.Combat.ZennyCanFightTheGremlin", GLTestUtils::Flags)
bool FGLSliceFight::RunTest(const FString& Parameters)
{
	FArena Arena(TEXT("GLSliceFight"));
	Arena.Inventory->AddItem(TEXT("item.tool.pry_bar"), 1);
	AGLCreature* Gremlin = Arena.Gremlin(FVector(150, 0, 100), 180.0);
	TestNull(TEXT("fists and bar: nothing behind Zenny is hit"), [&] { Arena.Zenny->SetActorRotation(FRotator(0, 180, 0)); return Arena.Combat->Attack(); }());
	Arena.Combat->Advance(2.0);
	Arena.Zenny->SetActorRotation(FRotator::ZeroRotator);
	int32 Swings = 0;
	while (!Gremlin->IsDefeated() && Swings < 10)
	{
		TestNotNull(TEXT("a swing lands"), Arena.Combat->Attack());
		Arena.Combat->Advance(1.0);
		++Swings;
	}
	TestEqual(TEXT("the pry bar (20) beats 60 health in three swings"), Swings, 3);
	TestTrue(TEXT("defeated"), Gremlin->IsDefeated());
	TestEqual(TEXT("its drop is Zenny's (combat source)"), Arena.Inventory->CountOf(TEXT("item.material.static_residue")), 2);
	TestEqual(TEXT("Event.Creature.Defeated once"), Arena.Count(TEXT("Event.Creature.Defeated")), 1);
	TestFalse(TEXT("it no longer blocks anything"), Gremlin->GetActorEnableCollision());
	TestTrue(TEXT("NICE mourns her gremlin"), Arena.Exchanges.Contains(FName(TEXT("exchange.creature.defeated"))));

	// And it fights back: next to it, Zenny takes 12 a strike, dies, and wakes at his respawn point.
	AGLCreature* Second = Arena.Gremlin(FVector(120, 0, 100), 180.0);
	Arena.Combat->SetRespawnPoint(FVector(-2000, 0, 100));
	for (int32 Step = 0; Step < 200 && !Arena.Health->IsDead(); ++Step)
	{
		Second->Think(0.1f);
	}
	TestTrue(TEXT("the gremlin can kill Zenny"), Arena.Health->IsDead());
	TestTrue(TEXT("Event.Player.Hurt along the way"), Arena.Count(TEXT("Event.Player.Hurt")) >= 8);
	TestEqual(TEXT("Event.Player.Died"), Arena.Count(TEXT("Event.Player.Died")), 1);
	Second->Think(0.1f);
	TestNotEqual(TEXT("it stops attacking a de-rezzed Zenny"), Second->GetState(), EGLCreatureState::Attack);
	Arena.Combat->Advance(3.5f);
	TestFalse(TEXT("respawned"), Arena.Health->IsDead());
	TestEqual(TEXT("with full health"), Arena.Health->GetCurrent(), 100.0);
	TestTrue(TEXT("at the respawn point"), Arena.Zenny->GetActorLocation().Equals(FVector(-2000, 0, 100), 1.0));
	TestEqual(TEXT("keeping his things"), Arena.Inventory->CountOf(TEXT("item.tool.pry_bar")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSliceDistract, "Gridlands.Game.Combat.PehlichiDistractionIsNonLethal", GLTestUtils::Flags)
bool FGLSliceDistract::RunTest(const FString& Parameters)
{
	FArena Arena(TEXT("GLSliceDistract"));
	AGLCreature* Gremlin = Arena.Gremlin(FVector(600, 0, 100), 180.0); // facing Zenny, 6 m away
	Gremlin->Think(0.1f);
	TestEqual(TEXT("it has seen Zenny"), Gremlin->GetState(), EGLCreatureState::Chase);
	TestTrue(TEXT("Pehlichi warns about it"), Arena.Exchanges.Contains(FName(TEXT("exchange.creature.spotted"))));
	Arena.Pehlichi->SetActorLocation(FVector(600, 1200, 100)); // parked across the room, within hearing
	TestEqual(TEXT("distract accepted"), Arena.Pehlichi->GetCommands()->Issue(TEXT("Command.Pehlichi.Distract"), Arena.Zenny), EGLCommandRejection::None);
	TestEqual(TEXT("and cannot be spammed"), Arena.Pehlichi->GetCommands()->Issue(TEXT("Command.Pehlichi.Distract"), Arena.Zenny), EGLCommandRejection::Busy);
	Gremlin->Think(0.1f);
	TestEqual(TEXT("the gremlin goes after the noise"), Gremlin->GetState(), EGLCreatureState::Investigate);
	const double HealthBefore = Gremlin->GetHealth()->GetCurrent();
	Arena.Zenny->SetActorLocation(FVector(550, 0, 100)); // Zenny walks right past it
	for (int32 Step = 0; Step < 60; ++Step)
	{
		Gremlin->Think(0.1f);
	}
	TestEqual(TEXT("while lured, it never touched Zenny"), Arena.Health->GetCurrent(), 100.0);
	TestEqual(TEXT("Pehlichi dealt zero damage (ADR-0017)"), Gremlin->GetHealth()->GetCurrent(), HealthBefore);
	TestEqual(TEXT("Event.Creature.Distracted"), Arena.Count(TEXT("Event.Creature.Distracted")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSliceAvoid, "Gridlands.Game.Combat.GremlinCanBeAvoidedAndTheDrainFixedWithoutAFight", GLTestUtils::Flags)
bool FGLSliceAvoid::RunTest(const FString& Parameters)
{
	FArena Arena(TEXT("GLSliceAvoid"), true);
	AGLCreature* Gremlin = Arena.Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindCreature(TEXT("placement.origin.drain_gremlin_den"));
	if (!TestNotNull(TEXT("the drain gremlin comes from its placement"), Gremlin))
	{
		return false;
	}
	Gremlin->SetActorTickEnabled(false);
	const FVector Den = Gremlin->GetActorLocation();
	// Behind it (it faces the entrance, west): safe even up close.
	Arena.Zenny->SetActorLocation(Den + FVector(500, 300, 0));
	Gremlin->Think(0.1f);
	TestEqual(TEXT("behind it: unseen"), Gremlin->GetState(), EGLCreatureState::Idle);
	// In front but behind a wall (the side channel's partition): unseen.
	AStaticMeshActor* DrainWall = Arena.Test.World->SpawnActor<AStaticMeshActor>(Den + FVector(-300, 0, 0), FRotator::ZeroRotator);
	DrainWall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	DrainWall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	DrainWall->GetStaticMeshComponent()->SetWorldScale3D(FVector(0.4, 6.0, 4.0));
	DrainWall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Arena.Zenny->SetActorLocation(Den + FVector(-700, 0, 0));
	Gremlin->Think(0.1f);
	TestEqual(TEXT("a wall between them: unseen"), Gremlin->GetState(), EGLCreatureState::Idle);

	// The drain's glitch is repaired with the gremlin alive and untouched: no combat needed.
	AGLGlitch* Echo = Arena.Test.World->GetSubsystem<UGLGlitchSubsystem>()->FindByPlacement(TEXT("placement.origin.drain_glitch_echo_loop"));
	if (!TestNotNull(TEXT("the drain glitch"), Echo))
	{
		return false;
	}
	Arena.Zenny->SetActorLocation(Echo->GetActorLocation() + FVector(0, -300, 100));
	Arena.Pehlichi->SetActorLocation(Echo->GetActorLocation() + FVector(0, -150, 0));
	Arena.Pehlichi->GetScan()->Scan();
	UGLGlitchSubsystem* Glitches = Arena.Test.World->GetSubsystem<UGLGlitchSubsystem>();
	Glitches->EvaluateRequirements();
	TestEqual(TEXT("repair accepted"), Arena.Pehlichi->GetCommands()->Issue(TEXT("Command.Pehlichi.Repair"), Arena.Zenny), EGLCommandRejection::None);
	for (int32 Step = 0; Step < 130; ++Step)
	{
		Glitches->EvaluateRequirements();
		Arena.Pehlichi->GetPositioning()->Advance(0.1f);
		Arena.Pehlichi->GetRepair()->Advance(0.1f);
	}
	TestEqual(TEXT("the echo loop is repaired"), Echo->GetGlitch()->GetState(), EGLGlitchState::Repaired);
	TestFalse(TEXT("the gremlin was never fought"), Gremlin->IsDefeated());
	TestEqual(TEXT("at full health"), Gremlin->GetHealth()->GetCurrent(), Gremlin->GetHealth()->GetMax());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSliceStorm, "Gridlands.Game.Storm.RainingCatsAndDogsIsBoundedAndCleansUp", GLTestUtils::Flags)
bool FGLSliceStorm::RunTest(const FString& Parameters)
{
	auto Repaired = [](UWorld* World)
	{
		FGLGameplayEvent Event;
		Event.Tag = GLTestUtils::Tag(TEXT("Event.Glitch.Repaired"));
		UGLEventSubsystem::Emit(World, Event);
	};
	{
		FArena Arena(TEXT("GLSliceStorm"));
		UGLStormSubsystem* Storms = Arena.Test.World->GetSubsystem<UGLStormSubsystem>();
		Repaired(Arena.Test.World);
		TestFalse(TEXT("one repair: no storm yet"), Storms->IsRaining());
		Repaired(Arena.Test.World);
		TestTrue(TEXT("NICE retaliates after the second repair"), Storms->IsRaining());
		// Her line is story-critical: it plays now or right after the repair banter (deferred, never dropped).
		UGLDialogueDirector* Director = Arena.Test.World->GetSubsystem<UGLDialogueDirector>();
		for (double Time = 100.0; Time < 400.0 && Director->NumDeferred() > 0; Time += 30.0)
		{
			Director->SetTimeOverride(Time);
			Director->PlayDeferred();
		}
		TestTrue(TEXT("and says so"), Arena.Exchanges.Contains(FName(TEXT("exchange.storm.cats_and_dogs"))));
		int32 MostAtOnce = 0;
		double Elapsed = 0.0;
		bool bSawFalling = false;
		int32 AliveNearTheEnd = 0;
		while (Storms->IsRaining() && Elapsed < 60.0)
		{
			if (Elapsed > 39.0)
			{
				AliveNearTheEnd = FMath::Max(AliveNearTheEnd, Arena.ArtifactsInWorld());
			}
			Storms->Step(0.1f);
			Elapsed += 0.1;
			MostAtOnce = FMath::Max(MostAtOnce, Arena.ArtifactsInWorld());
			bSawFalling |= Arena.ArtifactsInWorld() > 0;
		}
		TestTrue(TEXT("it rained artifacts"), bSawFalling && MostAtOnce > 5);
		TestTrue(FString::Printf(TEXT("it rains all storm long (%d still falling just before the end)"), AliveNearTheEnd), AliveNearTheEnd > 3);
		TestTrue(FString::Printf(TEXT("bounded: it stopped after its duration (%.1f s)"), Elapsed), FMath::IsNearlyEqual(Elapsed, 40.0, 0.2));
		TestEqual(TEXT("clean-up: no storm artifact remains anywhere in the world"), Arena.ArtifactsInWorld(), 0);
		TestEqual(TEXT("and the storm tracks none"), Storms->NumArtifacts(), 0);
		TestEqual(TEXT("Event.Storm.Ended"), Arena.Count(TEXT("Event.Storm.Ended")), 1);
		TestEqual(TEXT("harmless: Zenny untouched"), Arena.Health->GetCurrent(), 100.0);
		Repaired(Arena.Test.World);
		TestFalse(TEXT("it does not repeat"), Storms->IsRaining());
		Arena.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(SliceSlot);
	}
	FArena Reloaded(TEXT("GLSliceStormReloaded"));
	Reloaded.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(SliceSlot);
	UGLStormSubsystem* Storms = Reloaded.Test.World->GetSubsystem<UGLStormSubsystem>();
	TestFalse(TEXT("not raining after load"), Storms->IsRaining());
	Repaired(Reloaded.Test.World);
	Repaired(Reloaded.Test.World);
	TestFalse(TEXT("and it remembers the storm happened"), Storms->IsRaining());
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(SliceSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSliceWorld, "Gridlands.Game.World.DiscoveriesSilenceAndPersistence", GLTestUtils::Flags)
bool FGLSliceWorld::RunTest(const FString& Parameters)
{
	{
		FArena Arena(TEXT("GLSliceWorld"), true);
		UGLAmbientSubsystem* Ambient = Arena.Test.World->GetSubsystem<UGLAmbientSubsystem>();
		Ambient->bEnabled = false;
		Arena.Zenny->SetActorLocation(FVector(9000, -2000, 100)); // at the diner
		Ambient->Step(0.5f, Arena.Zenny);
		TestTrue(TEXT("finding the diner teaches its memory"), Arena.Test.World->GetSubsystem<UGLKnowledgeSubsystem>()->Knows(TEXT("knowledge.memory.fifties_diner")));
		TestEqual(TEXT("Event.Discovery.Found once"), Arena.Count(TEXT("Event.Discovery.Found")), 1);
		TestTrue(TEXT("NICE calls it a rendering accident"), Arena.Exchanges.Contains(FName(TEXT("exchange.discovery.fifties_diner"))));
		Ambient->Step(0.5f, Arena.Zenny);
		TestEqual(TEXT("not announced twice"), Arena.Count(TEXT("Event.Discovery.Found")), 1);
		for (int32 Second = 0; Second < 50; ++Second)
		{
			Ambient->Step(1.f, Arena.Zenny); // standing still, saying nothing (as always)
		}
		TestEqual(TEXT("his silence is noticed once"), Arena.Count(TEXT("Event.Player.Silent")), 1);
		// A dead gremlin and Zenny's health persist.
		AGLCreature* Gremlin = Arena.Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindCreature(TEXT("placement.origin.drain_gremlin_den"));
		Gremlin->GetHealth()->ApplyDamage(1000.0, Arena.Zenny);
		Arena.Health->ApplyDamage(30.0, nullptr);
		Arena.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(SliceSlot);
	}
	FArena Reloaded(TEXT("GLSliceWorldReloaded"), true);
	TArray<FString> Problems;
	Reloaded.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(SliceSlot, &Problems);
	TestEqual(TEXT("no load problems"), Problems.Num(), 0);
	TestTrue(TEXT("the gremlin stays defeated"), Reloaded.Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindCreature(TEXT("placement.origin.drain_gremlin_den"))->IsDefeated());
	TestEqual(TEXT("Zenny's health is kept"), Reloaded.Health->GetCurrent(), 70.0);
	UGLAmbientSubsystem* Ambient = Reloaded.Test.World->GetSubsystem<UGLAmbientSubsystem>();
	Ambient->bEnabled = false;
	Reloaded.Zenny->SetActorLocation(FVector(9000, -2000, 100));
	Ambient->Step(0.5f, Reloaded.Zenny);
	TestEqual(TEXT("the diner is not re-discovered"), Reloaded.Count(TEXT("Event.Discovery.Found")), 0);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(SliceSlot));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
