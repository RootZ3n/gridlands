// P6: authored structures (ADR-0030) and world noise (ADR-0031) in a Grid world at canonical scale.
// Salvage the wrong support and a structure collapses by the deterministic plan; the impact hurts
// Zenny and creatures through the normal health system; the result persists through streaming,
// saves and restarts. Terraforming is heard, only within hearing; losing sight is not forgetting.

#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Character.h"
#include "HAL/FileManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Noise/GLNoiseSubsystem.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLGridSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLStructureGameTests
{
	const FName SCarport(TEXT("placement.origin.structure_carport_01"));
	const FName SCarportEdge(TEXT("placement.origin.structure_carport_edge"));
	const FName SPine(TEXT("placement.origin.structure_pine_01"));
	const FName SOrigin(TEXT("cell.home.origin"));
	const FName SLots(TEXT("cell.outer.diner_lots"));
	const FName SGremlinDef(TEXT("creature.drain.static_gremlin"));
	const FVector SHome(0, -1200, 100);
	const FVector SAtEdge(48600, 1500, 100); // near the carport by the origin/lots boundary (x = 512 m)
	const FVector SDeepInLots(120000, 0, 100);
	const FString SSlot = TEXT("automation-test-structures");

	struct FStructureScene
	{
		GLTestUtils::FTestWorld Test;
		ACharacter* Zenny = nullptr;
		UGLHealthComponent* Health = nullptr;
		UGLInventoryComponent* Inventory = nullptr;
		UGLGridSubsystem* Grid = nullptr;
		UGLStructureSubsystem* Structures = nullptr;
		UGLTerrainSubsystem* Terrain = nullptr;
		UGLNoiseSubsystem* Noise = nullptr;
		TArray<FName> Events;

		explicit FStructureScene(const TCHAR* Name, const FVector& Start = SHome) : Test(Name)
		{
			UWorld* World = Test.World;
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event) { Events.Add(Event.Tag.GetTagName()); }));
			Zenny = World->SpawnActor<ACharacter>(Start, FRotator::ZeroRotator);
			Inventory = NewObject<UGLInventoryComponent>(Zenny);
			Inventory->RegisterComponent();
			Health = NewObject<UGLHealthComponent>(Zenny);
			Health->RegisterComponent();
			Grid = World->GetSubsystem<UGLGridSubsystem>();
			Grid->bShowBoundaries = false;
			Grid->Enable(false);
			Structures = World->GetSubsystem<UGLStructureSubsystem>();
			Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			Noise = World->GetSubsystem<UGLNoiseSubsystem>();
			GoTo(Start);
		}

		void GoTo(const FVector& Where)
		{
			Zenny->SetActorLocation(Where);
			Grid->Advance(Where);
			Grid->FlushAll();
		}

		/** Zenny stands at XY on the ground (capsule centre 90 cm up). */
		void Stand(const FVector2D& At)
		{
			Zenny->SetActorLocation(FVector(At, Terrain->HeightAt(At) + 90.0));
		}

		/** Salvages a part through the ordinary salvage pipeline: hit until it comes away. */
		bool Salvage(FName Placement, FName Part)
		{
			AGLStructurePart* Actor = Structures->FindPart(Placement, Part);
			for (int32 Hit = 0; Actor && Hit < 50 && !Actor->GetSalvageable()->IsSalvaged(); ++Hit)
			{
				Actor->GetSalvageable()->Interact(Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
			}
			return Actor && Actor->GetSalvageable()->IsSalvaged();
		}

		void Run(double Seconds)
		{
			for (double T = 0.0; T < Seconds; T += 1.0 / 60.0)
			{
				Structures->Advance(1.0 / 60.0);
			}
		}

		EGLStructurePartState StateOf(FName Placement, FName Part) const
		{
			const FGLStructureRuntime* S = Structures->Find(Placement);
			const FGLStructurePartRuntime* P = S ? S->Parts.FindByPredicate([Part](const FGLStructurePartRuntime& X) { return X.Name == Part; }) : nullptr;
			return P ? P->State : EGLStructurePartState::Intact;
		}

		FTransform RestOf(FName Placement, FName Part) const
		{
			const FGLStructureRuntime* S = Structures->Find(Placement);
			const FGLStructurePartRuntime* P = S ? S->Parts.FindByPredicate([Part](const FGLStructurePartRuntime& X) { return X.Name == Part; }) : nullptr;
			return P ? P->Rest : FTransform::Identity;
		}

		int32 PartActors(FName Placement) const
		{
			int32 Count = 0;
			for (TActorIterator<AGLStructurePart> It(Test.World); It; ++It)
			{
				Count += It->StructurePlacement == Placement && !It->IsActorBeingDestroyed() ? 1 : 0;
			}
			return Count;
		}

		AGLCreature* SpawnCreature(const FVector2D& At, double Yaw = 0.0)
		{
			AGLCreature* Creature = Test.World->SpawnActor<AGLCreature>(FVector(At, Terrain->HeightAt(At) + 70.0), FRotator(0.0, Yaw, 0.0));
			if (Creature)
			{
				Creature->Setup(SGremlinDef, TEXT("placement.origin.test_gremlin"));
			}
			return Creature;
		}
	};
}

using GLStructureGameTests::SCarport;
using GLStructureGameTests::SCarportEdge;
using GLStructureGameTests::SPine;
using GLStructureGameTests::SOrigin;
using GLStructureGameTests::SLots;
using GLStructureGameTests::SGremlinDef;
using GLStructureGameTests::SHome;
using GLStructureGameTests::SAtEdge;
using GLStructureGameTests::SDeepInLots;
using GLStructureGameTests::SSlot;
using GLStructureGameTests::FStructureScene;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStructureWrongSupport, "Gridlands.Game.Structure.WrongSupportCollapsesAndHurtsZenny", GLTestUtils::Flags)
bool FGLStructureWrongSupport::RunTest(const FString& Parameters)
{
	FStructureScene S(TEXT("GLStructureWrongSupportWorld"));
	const FGLStructureRuntime* Carport = S.Structures->Find(SCarport);
	if (!TestNotNull(TEXT("the carport streamed in with its cell"), Carport) || !TestEqual(TEXT("four parts"), S.PartActors(SCarport), 4))
	{
		return false;
	}
	// Zenny stands under the west deck, beside the north post.
	S.Stand(FVector2D(-3050, -3950));
	const int32 NoiseBefore = S.Noise->GetEmittedCount();
	TestTrue(TEXT("the south post comes away"), S.Salvage(SCarport, TEXT("post_south")));
	S.Run(0.5);
	TestEqual(TEXT("redundancy: the north post still holds everything"), S.StateOf(SCarport, TEXT("deck_west")), EGLStructurePartState::Intact);
	TestEqual(TEXT("no collapse yet"), S.Structures->ActiveCollapses(), 0);
	TestTrue(TEXT("salvage is never silent (a noise per hit, one when the part comes away)"),
		S.Noise->CountOf(TEXT("Noise.Salvage.Hit")) > 0 && S.Noise->CountOf(TEXT("Noise.Structure.Break")) == 1 && S.Noise->GetEmittedCount() > NoiseBefore);

	// The wrong support: the last one.
	TestTrue(TEXT("the north post comes away"), S.Salvage(SCarport, TEXT("post_north")));
	TestEqual(TEXT("both decks lost their support at once"), S.Structures->ActiveCollapses(), 2);
	TestEqual(TEXT("the outcome is decided now: the west deck is already debris"), S.StateOf(SCarport, TEXT("deck_west")), EGLStructurePartState::Debris);
	TestEqual(TEXT("so is the east deck (it only hung off the west one)"), S.StateOf(SCarport, TEXT("deck_east")), EGLStructurePartState::Debris);
	TestTrue(TEXT("a collapse event for the dialogue director"), S.Events.Contains(FName(TEXT("Event.Structure.Collapsed"))));
	S.Run(0.5);
	TestEqual(TEXT("nothing is hurt before the impact"), S.Health->GetCurrent(), 100.0);
	S.Run(1.5);
	TestEqual(TEXT("the deck hit Zenny with the planned damage (10 + 20 x 2.5 m)"), S.Health->GetCurrent(), 40.0, 0.01);
	TestEqual(TEXT("each impact landed once"), S.Structures->GetImpacts().Num(), 2);
	TestEqual(TEXT("no collapse left running"), S.Structures->ActiveCollapses(), 0);
	TestEqual(TEXT("the landing is heard"), S.Noise->CountOf(TEXT("Noise.Structure.Collapse")), 2);
	// World state afterwards: debris rests on the ground where the plan said, solid and salvageable.
	const FTransform WestRest = S.RestOf(SCarport, TEXT("deck_west"));
	AGLStructurePart* West = S.Structures->FindPart(SCarport, TEXT("deck_west"));
	TestTrue(TEXT("the deck actor is where the plan put it"), West && West->GetActorLocation().Equals(WestRest.GetLocation(), 0.1));
	TestEqual(TEXT("on the ground"), WestRest.GetLocation().Z, S.Terrain->HeightAt(FVector2D(WestRest.GetLocation())), 30.0);
	TestTrue(TEXT("its debris can be salvaged"), West && !West->GetSalvageable()->IsSalvaged());
	TestEqual(TEXT("the posts are gone"), S.PartActors(SCarport), 2);
	TestTrue(TEXT("the ground under debris does not move (dig refused)"), S.Structures->IsUnderStructure(FVector2D(WestRest.GetLocation())));
	TArray<FGLSavedStructurePart> Facts;
	S.Structures->CaptureCell(SOrigin, Facts);
	TestEqual(TEXT("four facts: two posts removed, two decks as debris"), Facts.FilterByPredicate([](const FGLSavedStructurePart& F) { return F.Placement == SCarport; }).Num(), 4);

	// The same collapse can kill.
	FStructureScene K(TEXT("GLStructureKillWorld"));
	K.Stand(FVector2D(-3050, -3950));
	K.Health->Restore(50.0);
	K.Salvage(SCarport, TEXT("post_south"));
	K.Salvage(SCarport, TEXT("post_north"));
	K.Run(2.0);
	TestTrue(TEXT("a weakened Zenny under the deck is killed"), K.Health->IsDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStructureCreature, "Gridlands.Game.Structure.CollapseDamagesCreaturesThroughTheSameModel", GLTestUtils::Flags)
bool FGLStructureCreature::RunTest(const FString& Parameters)
{
	FStructureScene S(TEXT("GLStructureCreatureWorld"));
	// A creature under the east deck; Zenny well clear of both decks.
	AGLCreature* Gremlin = S.SpawnCreature(FVector2D(-2800, -4000));
	AGLCreature* Listener = S.SpawnCreature(FVector2D(-2800, -2800)); // 12 m away: outside the impact, inside hearing
	if (!TestNotNull(TEXT("creature"), Gremlin) || !TestNotNull(TEXT("listener"), Listener))
	{
		return false;
	}
	S.Stand(FVector2D(-3300, -4300));
	const int32 ResidueBefore = S.Inventory->CountOf(TEXT("item.material.static_residue"));
	S.Salvage(SCarport, TEXT("post_south"));
	S.Salvage(SCarport, TEXT("post_north"));
	S.Run(2.0);
	TestTrue(TEXT("the creature under the deck is defeated by the collapse"), Gremlin->IsDefeated());
	TestEqual(TEXT("Zenny, clear of it, is untouched"), S.Health->GetCurrent(), 100.0);
	const FGLImpactRecord* EastHit = S.Structures->GetImpacts().FindByPredicate([](const FGLImpactRecord& R) { return R.Part == FName(TEXT("deck_east")); });
	TestTrue(TEXT("the impact record names the creature (the one authoritative path)"), EastHit && EastHit->Hit.ContainsByPredicate([Gremlin](const TWeakObjectPtr<AActor>& A) { return A.Get() == Gremlin; }));
	TestEqual(TEXT("its drops went to whoever brought the structure down"), S.Inventory->CountOf(TEXT("item.material.static_residue")), ResidueBefore + 2);
	TestFalse(TEXT("the listener was not hit"), Listener->IsDefeated());
	Listener->Think(0.1f);
	TestEqual(TEXT("but it heard the collapse and goes to look"), Listener->GetState(), EGLCreatureState::Investigate);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStructurePersists, "Gridlands.Game.Structure.CollapsePersistsThroughStreamingSavesAndRestart", GLTestUtils::Flags)
bool FGLStructurePersists::RunTest(const FString& Parameters)
{
	FStructureScene S(TEXT("GLStructurePersistWorld"), SAtEdge);
	TestTrue(TEXT("both cells are loaded at the boundary"), S.Grid->IsLoaded(SOrigin) && S.Grid->IsLoaded(SLots));
	S.Stand(FVector2D(48600, 1500));
	S.Salvage(SCarportEdge, TEXT("post_south"));
	S.Salvage(SCarportEdge, TEXT("post_north"));
	S.Run(0.4); // mid-fall
	TestEqual(TEXT("mid-fall"), S.Structures->ActiveCollapses(), 2);
	const FTransform WestRest = S.RestOf(SCarportEdge, TEXT("deck_west"));
	const FTransform EastRest = S.RestOf(SCarportEdge, TEXT("deck_east"));
	// Cross away mid-fall: the origin streams out with the collapse unfinished.
	S.GoTo(SDeepInLots);
	TestFalse(TEXT("the origin streamed out"), S.Grid->IsLoaded(SOrigin));
	TestEqual(TEXT("its unfinished collapses went with it"), S.Structures->ActiveCollapses(), 0);
	TestEqual(TEXT("no carport actors remain"), S.PartActors(SCarportEdge), 0);
	const int32 ImpactsAway = S.Structures->GetImpacts().Num();
	// Back again, three times: the outcome, never a replay.
	for (int32 Round = 0; Round < 3; ++Round)
	{
		S.GoTo(SAtEdge);
		S.Run(3.0);
		TestEqual(TEXT("west deck: debris"), S.StateOf(SCarportEdge, TEXT("deck_west")), EGLStructurePartState::Debris);
		TestEqual(TEXT("east deck: debris"), S.StateOf(SCarportEdge, TEXT("deck_east")), EGLStructurePartState::Debris);
		TestEqual(TEXT("posts stay removed (no restored supports)"), S.StateOf(SCarportEdge, TEXT("post_north")), EGLStructurePartState::Removed);
		TestEqual(TEXT("two debris actors, no duplicates"), S.PartActors(SCarportEdge), 2);
		AGLStructurePart* West = S.Structures->FindPart(SCarportEdge, TEXT("deck_west"));
		TestTrue(TEXT("at the same rest"), West && West->GetActorLocation().Equals(WestRest.GetLocation(), 0.5));
		TestEqual(TEXT("no collapse replayed"), S.Structures->ActiveCollapses(), 0);
		TestEqual(TEXT("no impact repeated (no duplicate damage)"), S.Structures->GetImpacts().Num(), ImpactsAway);
		S.GoTo(SDeepInLots);
	}
	// Salvage one piece of debris, then save standing in the neighbouring cell.
	S.GoTo(SAtEdge);
	const int32 PlanksBefore = S.Inventory->CountOf(TEXT("item.material.timber_plank"));
	TestTrue(TEXT("debris salvages"), S.Salvage(SCarportEdge, TEXT("deck_east")));
	S.Run(0.1);
	TestTrue(TEXT("and pays"), S.Inventory->CountOf(TEXT("item.material.timber_plank")) > PlanksBefore);
	S.GoTo(SDeepInLots);
	TestTrue(TEXT("saved in the lots"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(SSlot));

	FStructureScene R(TEXT("GLStructureRestartWorld"));
	TArray<FString> Problems;
	TestTrue(TEXT("restart loads"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(SSlot, &Problems));
	TestEqual(TEXT("no load problems"), Problems.Num(), 0);
	R.GoTo(SAtEdge);
	R.Run(3.0);
	TestEqual(TEXT("after restart: west deck still debris"), R.StateOf(SCarportEdge, TEXT("deck_west")), EGLStructurePartState::Debris);
	TestEqual(TEXT("east deck stays salvaged"), R.StateOf(SCarportEdge, TEXT("deck_east")), EGLStructurePartState::DebrisSalvaged);
	TestEqual(TEXT("posts stay removed"), R.StateOf(SCarportEdge, TEXT("post_south")), EGLStructurePartState::Removed);
	TestEqual(TEXT("one actor: the west deck's debris"), R.PartActors(SCarportEdge), 1);
	AGLStructurePart* West = R.Structures->FindPart(SCarportEdge, TEXT("deck_west"));
	TestTrue(TEXT("where it came to rest"), West && West->GetActorLocation().Equals(WestRest.GetLocation(), 0.5));
	TestEqual(TEXT("nothing collapsed on load"), R.Structures->GetImpacts().Num(), 0);
	TestFalse(TEXT("no collapse event on load"), R.Events.Contains(FName(TEXT("Event.Structure.Collapsed"))));
	TestEqual(TEXT("the other structures are untouched"), R.StateOf(TEXT("placement.origin.structure_pine_edge"), TEXT("trunk")), EGLStructurePartState::Intact);
	AddInfo(FString::Printf(TEXT("east deck rested at %s"), *EastRest.GetLocation().ToCompactString()));
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(SSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLStructureTree, "Gridlands.Game.Structure.TreeFellsThroughTheStructuralPipelineAndPersists", GLTestUtils::Flags)
bool FGLStructureTree::RunTest(const FString& Parameters)
{
	FStructureScene S(TEXT("GLTreeWorld"));
	S.Stand(FVector2D(-1700, -4500)); // west of the pine
	const int32 PlanksBefore = S.Inventory->CountOf(TEXT("item.material.timber_plank"));
	TestTrue(TEXT("the stump is cut (the salvage pipeline)"), S.Salvage(SPine, TEXT("stump")));
	TestTrue(TEXT("chopping is heard as chopping"), S.Noise->CountOf(TEXT("Noise.Gather.Chop")) >= 3);
	TestEqual(TEXT("the trunk lost its support"), S.StateOf(SPine, TEXT("trunk")), EGLStructurePartState::Debris);
	S.Run(6.0);
	const FTransform Log = S.RestOf(SPine, TEXT("trunk"));
	TestTrue(TEXT("it fell away from Zenny (provisional policy): it lies pointing east"), Log.GetRotation().GetUpVector().Equals(FVector(1, 0, 0), 0.05));
	TestEqual(TEXT("Zenny, behind the cut, is untouched"), S.Health->GetCurrent(), 100.0);
	TestTrue(TEXT("the log salvages"), S.Salvage(SPine, TEXT("trunk")));
	S.Run(0.1);
	TestTrue(TEXT("timber gathered"), S.Inventory->CountOf(TEXT("item.material.timber_plank")) > PlanksBefore);
	TestEqual(TEXT("nothing left of the tree"), S.PartActors(SPine), 0);
	// Unload and reload, then restart.
	S.GoTo(SDeepInLots);
	S.GoTo(SHome);
	TestEqual(TEXT("after reloading the tree stays felled and gathered"), S.StateOf(SPine, TEXT("trunk")), EGLStructurePartState::DebrisSalvaged);
	TestEqual(TEXT("and no actors come back"), S.PartActors(SPine), 0);
	TestTrue(TEXT("saved"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(SSlot));
	FStructureScene R(TEXT("GLTreeRestartWorld"));
	TestTrue(TEXT("restart"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(SSlot));
	R.GoTo(SHome);
	TestEqual(TEXT("after restart: stump cut"), R.StateOf(SPine, TEXT("stump")), EGLStructurePartState::Removed);
	TestEqual(TEXT("log gathered"), R.StateOf(SPine, TEXT("trunk")), EGLStructurePartState::DebrisSalvaged);
	TestEqual(TEXT("no tree actors"), R.PartActors(SPine), 0);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(SSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNoiseHeard, "Gridlands.Game.Noise.TerraformingIsHeardOnlyWithinHearing", GLTestUtils::Flags)
bool FGLNoiseHeard::RunTest(const FString& Parameters)
{
	FStructureScene S(TEXT("GLNoiseWorld"));
	S.Inventory->AddItem(TEXT("item.tool.shovel"), 1);
	// The creature faces +X; Zenny is behind it, out of sight.
	const FVector2D Home(-8000, 3000);
	AGLCreature* Gremlin = S.SpawnCreature(Home, 0.0);
	S.Stand(Home + FVector2D(-600, 0));
	Gremlin->Think(0.1f);
	TestEqual(TEXT("it does not see Zenny behind it"), Gremlin->GetState(), EGLCreatureState::Idle);
	// Dig 8 m behind it: inside its hearing (16 m) and the dig's reach (12 m).
	TestTrue(TEXT("dug"), S.Terrain->Terraform(S.Zenny, TEXT("terraform.shovel.dig"), Home + FVector2D(-800, 0)).bApplied);
	const FGLNoiseEvent& Dig = S.Noise->GetRecent().Last();
	TestEqual(TEXT("a dig noise"), Dig.Action, FName(TEXT("Noise.Terrain.Dig")));
	TestEqual(TEXT("with its data radius (12 m)"), Dig.RadiusCm, 1200.0, 0.01);
	TestTrue(TEXT("the creature heard it"), Dig.Heard >= 1);
	Gremlin->Think(0.1f);
	TestEqual(TEXT("it goes to look"), Gremlin->GetState(), EGLCreatureState::Investigate);
	TestTrue(TEXT("an ordinary noise is not Pehlichi's distraction"), S.Events.Contains(FName(TEXT("Event.Creature.Heard"))) && !S.Events.Contains(FName(TEXT("Event.Creature.Distracted"))));
	for (int32 I = 0; I < 100; ++I)
	{
		Gremlin->Think(0.1f); // 10 s: the investigation runs out
	}
	TestEqual(TEXT("then it settles"), Gremlin->GetState(), EGLCreatureState::Idle);
	// Dig 20 m behind it: out of range.
	const int32 HeardBefore = S.Noise->GetRecent().Last().Heard;
	S.Stand(Home + FVector2D(-1800, 0));
	TestTrue(TEXT("dug far away"), S.Terrain->Terraform(S.Zenny, TEXT("terraform.shovel.dig"), Home + FVector2D(-2000, 0)).bApplied);
	TestEqual(TEXT("nobody heard it"), S.Noise->GetRecent().Last().Heard, 0);
	Gremlin->Think(0.1f);
	TestEqual(TEXT("it stays put"), Gremlin->GetState(), EGLCreatureState::Idle);
	AddInfo(FString::Printf(TEXT("heard counts: near %d, far %d"), HeardBefore, S.Noise->GetRecent().Last().Heard));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLNoiseMemory, "Gridlands.Game.Noise.CoverAfterDetectionLeavesALastKnownPosition", GLTestUtils::Flags)
bool FGLNoiseMemory::RunTest(const FString& Parameters)
{
	FStructureScene S(TEXT("GLMemoryWorld"));
	S.Inventory->AddItem(TEXT("item.tool.shovel"), 1);
	S.Inventory->AddItem(TEXT("item.material.soil"), 20);
	const FVector2D Home(-8000, 3000);
	AGLCreature* Gremlin = S.SpawnCreature(Home, 0.0);
	// Zenny in front of it, in sight: it gives chase.
	const FVector2D Spot = Home + FVector2D(800, 0);
	S.Stand(Spot);
	Gremlin->Think(0.1f);
	if (!TestEqual(TEXT("it sees Zenny and chases"), Gremlin->GetState(), EGLCreatureState::Chase))
	{
		return false;
	}
	const FVector Seen = S.Zenny->GetActorLocation();
	// Zenny raises cover between them (terraforming; it is heard too).
	for (int32 Stroke = 0; Stroke < 8; ++Stroke)
	{
		S.Terrain->Terraform(S.Zenny, TEXT("terraform.shovel.raise"), Home + FVector2D(400, 0));
	}
	AddInfo(FString::Printf(TEXT("the mound between them: %.0f cm above the creature's ground"), S.Terrain->HeightAt(Home + FVector2D(400, 0)) - S.Terrain->HeightAt(Home)));
	S.Stand(Spot); // Zenny stays put behind the new mound
	Gremlin->Think(0.1f);
	TestEqual(TEXT("sight broken: it searches instead of forgetting"), Gremlin->GetState(), EGLCreatureState::Search);
	TestTrue(TEXT("where Zenny was last seen"), Gremlin->GetLastKnown().Equals(Seen, 1.0));
	TestTrue(TEXT("a searching event"), S.Events.Contains(FName(TEXT("Event.Creature.Searching"))));
	for (int32 I = 0; I < 40; ++I)
	{
		Gremlin->Think(0.1f); // 4 s
	}
	TestEqual(TEXT("still searching after 4 s (memory is 8 s)"), Gremlin->GetState(), EGLCreatureState::Search);
	for (int32 I = 0; I < 60; ++I)
	{
		Gremlin->Think(0.1f); // 6 s more
	}
	TestNotEqual(TEXT("memory spent: it gives up"), Gremlin->GetState(), EGLCreatureState::Search);
	TestTrue(TEXT("and Zenny is lost"), S.Events.Contains(FName(TEXT("Event.Creature.Lost"))));
	return true;
}

#endif
