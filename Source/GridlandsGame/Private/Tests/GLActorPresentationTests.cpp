// P8: gameplay models before gameplay actors. Glitches, salvage nodes and creatures have authoritative
// models made with their cell's gameplay layer; the cell's saved state is resolved onto those models in the
// same frame; their actors are presentation made from them over the following frames, never in a state the
// model contradicts; unloading takes the models at once and retires the actors (inert, then destroyed
// within the budget). The same invariant ADR-0033 holds for structures.

#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Character.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "Inventory/GLInventoryComponent.h"
#include "Noise/GLNoiseSubsystem.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLScanComponent.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLGridSubsystem.h"
#include "World/GLPlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLActorPresentationTests
{
	const FName APOrigin(TEXT("cell.home.origin"));
	const FName APLamp(TEXT("placement.origin.glitch_flicker_lamp"));
	const FName APBlocker(TEXT("placement.origin.junk_pile_01"));
	const FName APFence(TEXT("placement.origin.fence_panel_01"));
	const FName APGremlin(TEXT("placement.origin.drain_gremlin_den"));
	const FVector APInOrigin(0, -1200, 100);
	const FVector APDeepInLots(120000, 0, 100);

	struct FActorScene
	{
		GLTestUtils::FTestWorld Test;
		ACharacter* Zenny = nullptr;
		AGLPehlichi* Pehlichi = nullptr;
		UGLGridSubsystem* Grid = nullptr;
		UGLPlacementSubsystem* Placements = nullptr;
		UGLGlitchSubsystem* Glitches = nullptr;
		TArray<FName> Events;

		explicit FActorScene(const TCHAR* Name) : Test(Name)
		{
			UWorld* World = Test.World;
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event) { Events.Add(Event.Tag.GetTagName()); }));
			Zenny = World->SpawnActor<ACharacter>(APInOrigin, FRotator::ZeroRotator);
			NewObject<UGLInventoryComponent>(Zenny)->RegisterComponent();
			NewObject<UGLHealthComponent>(Zenny)->RegisterComponent();
			Placements = World->GetSubsystem<UGLPlacementSubsystem>();
			Glitches = World->GetSubsystem<UGLGlitchSubsystem>();
			Glitches->SetCommander(Zenny);
			Pehlichi = World->SpawnActor<AGLPehlichi>(APInOrigin + FVector(0, 200, 0), FRotator::ZeroRotator);
			Grid = World->GetSubsystem<UGLGridSubsystem>();
			Grid->bShowBoundaries = false;
			Grid->Enable(false);
			GoTo(APInOrigin);
		}

		void GoTo(const FVector& Where)
		{
			Zenny->SetActorLocation(Where);
			Grid->Advance(Where);
			Grid->FlushAll();
		}

		/** Streams toward Where like play until the origin's authoritative layer is in (or out). */
		bool StepUntil(const FVector& Where, bool bLoaded)
		{
			Zenny->SetActorLocation(Where);
			auto Done = [&] { return bLoaded ? Grid->IsRuntimeReady(APOrigin) : !Grid->IsLoaded(APOrigin); };
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

		/** The facts of the origin's gameplay placements, as a save would keep them (from the models). */
		FGLSavedCell Facts() const
		{
			FGLSavedCell Cell = Test.World->GetSubsystem<UGLSaveSubsystem>()->CaptureCell(APOrigin);
			Cell.BuildPieces.Reset();
			Cell.TerrainIndices.Reset();
			Cell.TerrainDeltaCm.Reset();
			Cell.StructureParts.Reset();
			return Cell;
		}

		template <typename T> int32 Live() const
		{
			int32 N = 0;
			for (TActorIterator<T> It(Test.World); It; ++It)
			{
				N += IsValid(*It) && !It->IsHidden() ? 1 : 0;
			}
			return N;
		}

		/** How many placements of the origin have Kind (from the data). */
		static int32 PlacementsOfKind(const TCHAR* Kind)
		{
			int32 N = 0;
			GLContent::Get().ForEachEntry([&](const FGLContentEntry& E)
			{
				const FGLPlacementDef* P = E.Definition.GetPtr<FGLPlacementDef>();
				N += P && P->Kind == Kind && UGLPlacementSubsystem::IsPlacementOfCell(E.Id, APOrigin) ? 1 : 0;
			});
			return N;
		}

		/** Changes three facts: the lamp detected (Pehlichi's scan), the blocker salvaged, the gremlin defeated. */
		void Change()
		{
			AGLGlitch* Lamp = Glitches->FindByPlacement(APLamp);
			Zenny->SetActorLocation(Lamp->GetActorLocation() + FVector(0, -300, 100));
			Pehlichi->SetActorLocation(Lamp->GetActorLocation() + FVector(0, -200, 0));
			Pehlichi->GetScan()->Scan();
			AGLSalvageNode* Node = Placements->FindSalvageNode(APBlocker);
			for (int32 Hit = 0; Node && Hit < 50 && !Node->GetSalvageable()->IsSalvaged(); ++Hit)
			{
				Node->GetSalvageable()->Interact(Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
			}
			Placements->FindCreature(APGremlin)->GetHealth()->ApplyDamage(1000.0, Zenny);
		}
	};

	bool SameFacts(const FGLSavedCell& A, const FGLSavedCell& B)
	{
		if (A.Glitches.Num() != B.Glitches.Num() || A.SalvagedPlacements != B.SalvagedPlacements || A.DefeatedCreatures != B.DefeatedCreatures)
		{
			return false;
		}
		for (int32 I = 0; I < A.Glitches.Num(); ++I)
		{
			if (A.Glitches[I].Placement != B.Glitches[I].Placement || A.Glitches[I].State != B.Glitches[I].State || A.Glitches[I].ItemsDelivered != B.Glitches[I].ItemsDelivered)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLActorWriteThrough, "Gridlands.Game.ActorPresentation.PresentedActorsWriteThroughToTheirModels", GLTestUtils::Flags)
bool FGLActorWriteThrough::RunTest(const FString& Parameters)
{
	GLActorPresentationTests::FActorScene S(TEXT("GLActorWriteWorld"));
	TestTrue(TEXT("the models start clean"), !S.Placements->IsSalvaged(GLActorPresentationTests::APBlocker) && !S.Placements->IsCreatureDefeated(GLActorPresentationTests::APGremlin)
		&& S.Glitches->FindRecord(GLActorPresentationTests::APLamp) && S.Glitches->FindRecord(GLActorPresentationTests::APLamp)->State == EGLGlitchState::Latent);
	S.Change();
	TestEqual(TEXT("a scan of the presented lamp is its record's state"), S.Glitches->FindRecord(GLActorPresentationTests::APLamp)->State, EGLGlitchState::Detected);
	TestTrue(TEXT("salvaging the presented node makes its placement salvaged"), S.Placements->IsSalvaged(GLActorPresentationTests::APBlocker));
	TestTrue(TEXT("defeating the presented creature makes its placement defeated"), S.Placements->IsCreatureDefeated(GLActorPresentationTests::APGremlin));
	const FGLSavedCell Facts = S.Facts();
	TestTrue(TEXT("a save reads all three from the models"), Facts.SalvagedPlacements.Contains(GLActorPresentationTests::APBlocker)
		&& Facts.DefeatedCreatures.Contains(GLActorPresentationTests::APGremlin)
		&& Facts.Glitches.ContainsByPredicate([](const FGLSavedGlitch& G) { return G.Placement == GLActorPresentationTests::APLamp && G.State == EGLGlitchState::Detected; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLActorOrder, "Gridlands.Game.ActorPresentation.SavedStateResolvesBeforeAnyGameplayActor", GLTestUtils::Flags)
bool FGLActorOrder::RunTest(const FString& Parameters)
{
	using namespace GLActorPresentationTests;
	FActorScene S(TEXT("GLActorOrderWorld"));
	S.Change();
	const FGLSavedCell Facts = S.Facts();
	S.GoTo(APDeepInLots);
	TestFalse(TEXT("the origin streamed out"), S.Grid->IsLoaded(APOrigin));
	TestNull(TEXT("  with its models"), S.Placements->FindActorModel(APFence));
	TestNull(TEXT("  and its glitch records"), S.Glitches->FindRecord(APLamp));
	const int32 EventsAway = S.Events.Num();

	// Back, streaming like play, with presentation held: the authoritative layer only.
	S.Grid->PresentationBudgetMs = -1.f;
	S.Grid->PresentationNearM = 0.f;
	TestTrue(TEXT("the origin's authoritative layer comes in"), S.StepUntil(APInOrigin, true));
	TestTrue(TEXT("saved state is resolved at once: the models hold every kept fact"), SameFacts(S.Facts(), Facts));
	TestTrue(TEXT("  the blocker is salvaged"), S.Placements->IsSalvaged(APBlocker));
	TestTrue(TEXT("  the gremlin is defeated"), S.Placements->IsCreatureDefeated(APGremlin));
	TestEqual(TEXT("  the lamp is detected"), S.Glitches->FindRecord(APLamp)->State, EGLGlitchState::Detected);
	TestTrue(TEXT("while no gameplay actor exists yet"), !S.Glitches->FindByPlacement(APLamp) && !S.Placements->FindSalvageNode(APFence) && !S.Placements->FindCreature(APGremlin));
	TestEqual(TEXT("  no glitch actor"), S.Live<AGLGlitch>(), 0);
	TestEqual(TEXT("  no salvage node actor"), S.Live<AGLSalvageNode>(), 0);
	TestEqual(TEXT("  no creature actor"), S.Live<AGLCreature>(), 0);
	TestTrue(TEXT("  and they wait to be presented"), S.Placements->IsActorPending(APLamp) && S.Placements->IsActorPending(APFence));
	TestTrue(TEXT("a save mid-presentation keeps every fact"), SameFacts(S.Facts(), Facts));

	// Each is made from its model as it is now, by the pump's own path.
	int32 StateChanges = 0;
	TestTrue(TEXT("the lamp is presented"), S.Placements->PresentActor(APLamp));
	AGLGlitch* Lamp = S.Glitches->FindByPlacement(APLamp);
	TestTrue(TEXT("  already detected (never latent first)"), Lamp && Lamp->GetGlitch()->GetState() == EGLGlitchState::Detected);
	if (Lamp)
	{
		Lamp->GetGlitch()->OnStateChanged.AddLambda([&StateChanges](EGLGlitchState, EGLGlitchState) { ++StateChanges; });
	}
	TestTrue(TEXT("the salvaged blocker's turn comes"), S.Placements->PresentActor(APBlocker));
	TestNull(TEXT("  and it is never made"), S.Placements->FindSalvageNode(APBlocker));
	TestTrue(TEXT("the defeated gremlin's turn comes"), S.Placements->PresentActor(APGremlin));
	TestNull(TEXT("  and it is never made"), S.Placements->FindCreature(APGremlin));
	TestTrue(TEXT("an untouched node is made"), S.Placements->PresentActor(APFence) && S.Placements->FindSalvageNode(APFence));

	// The rest within the budget.
	S.Grid->PresentationBudgetMs = 1.5f;
	for (int32 F = 0; F < 2000 && !S.Placements->IsCellPresented(APOrigin); ++F)
	{
		S.Grid->Advance(APInOrigin);
	}
	TestTrue(TEXT("the cell is presented"), S.Placements->IsCellPresented(APOrigin));
	TestEqual(TEXT("one glitch actor per glitch placement"), S.Live<AGLGlitch>(), FActorScene::PlacementsOfKind(TEXT("glitch")));
	TestEqual(TEXT("one salvage node per unsalvaged salvage placement"), S.Live<AGLSalvageNode>(), FActorScene::PlacementsOfKind(TEXT("salvage_node")) - 1);
	TestEqual(TEXT("no creature: the only one is defeated"), S.Live<AGLCreature>(), 0);
	TestEqual(TEXT("presenting made no gameplay events"), S.Events.Num(), EventsAway);
	TestEqual(TEXT("and changed no glitch state"), StateChanges, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLActorCancel, "Gridlands.Game.ActorPresentation.UnloadBeforePresentationCancelsCleanly", GLTestUtils::Flags)
bool FGLActorCancel::RunTest(const FString& Parameters)
{
	using namespace GLActorPresentationTests;
	FActorScene S(TEXT("GLActorCancelWorld"));
	S.GoTo(APDeepInLots);
	S.Grid->PresentationBudgetMs = -1.f;
	S.Grid->PresentationNearM = 0.f;
	TestTrue(TEXT("the origin's authoritative layer comes in"), S.StepUntil(APInOrigin, true));
	TestTrue(TEXT("  its actors wait"), S.Placements->IsActorPending(APLamp));
	TestTrue(TEXT("unloaded before anything was presented"), S.StepUntil(APDeepInLots, false));
	TestFalse(TEXT("  the waiting actors are dropped"), S.Placements->IsActorPending(APLamp) || S.Placements->IsActorPending(APFence) || S.Placements->IsActorPending(APGremlin));
	TestTrue(TEXT("  the models are gone"), !S.Placements->FindActorModel(APFence) && !S.Placements->FindActorModel(APGremlin) && !S.Glitches->FindRecord(APLamp));
	TestEqual(TEXT("  and no actor was ever made"), S.Live<AGLGlitch>() + S.Live<AGLSalvageNode>() + S.Live<AGLCreature>(), 0);
	S.Grid->PresentationBudgetMs = 1.5f;
	S.Grid->PresentationNearM = 20.f;
	S.GoTo(APInOrigin);
	TestEqual(TEXT("back: exactly one glitch actor per placement"), S.Live<AGLGlitch>(), FActorScene::PlacementsOfKind(TEXT("glitch")));
	TestEqual(TEXT("  one salvage node per placement"), S.Live<AGLSalvageNode>(), FActorScene::PlacementsOfKind(TEXT("salvage_node")));
	TestEqual(TEXT("  one creature per spawn"), S.Live<AGLCreature>(), FActorScene::PlacementsOfKind(TEXT("spawn")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLActorRetire, "Gridlands.Game.ActorPresentation.RetiredActorsAreInertAtOnce", GLTestUtils::Flags)
bool FGLActorRetire::RunTest(const FString& Parameters)
{
	using namespace GLActorPresentationTests;
	FActorScene S(TEXT("GLActorRetireWorld"));
	TWeakObjectPtr<AGLGlitch> Lamp = S.Glitches->FindByPlacement(APLamp);
	TWeakObjectPtr<AGLSalvageNode> Fence = S.Placements->FindSalvageNode(APFence);
	TWeakObjectPtr<AGLCreature> Gremlin = S.Placements->FindCreature(APGremlin);
	if (!TestTrue(TEXT("all three are presented"), Lamp.IsValid() && Fence.IsValid() && Gremlin.IsValid()))
	{
		return false;
	}
	const FVector Den = Gremlin->GetActorLocation();
	S.Grid->PresentationBudgetMs = -1.f; // hold destruction: inspect the retired actors
	TestTrue(TEXT("the origin streams out"), S.StepUntil(APDeepInLots, false));
	TestTrue(TEXT("its actors still exist (retired, not yet destroyed)"), Lamp.IsValid() && Fence.IsValid() && Gremlin.IsValid());
	TestTrue(TEXT("  waiting to be destroyed"), S.Placements->RetiringActorCount() >= 3);
	TestFalse(TEXT("the glitch is out of every query at once"), S.Glitches->GetAll().Contains(Lamp) || S.Glitches->GlitchesNear(Lamp->GetActorLocation(), 500.0).Contains(Lamp.Get()));
	TestTrue(TEXT("  and hidden"), Lamp->IsHidden());
	TestFalse(TEXT("the node refuses salvage"), Fence->GetSalvageable()->Interact(S.Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage"))));
	TestTrue(TEXT("  hidden, no collision"), Fence->IsHidden() && !Fence->GetActorEnableCollision());
	TestTrue(TEXT("the creature is out of play"), Gremlin->IsDefeated() && Gremlin->IsHidden() && !Gremlin->GetActorEnableCollision() && !Gremlin->IsActorTickEnabled());
	FGLNoiseEvent Loud;
	Loud.Location = Den;
	Loud.RadiusCm = 5000.0;
	TestFalse(TEXT("  and hears nothing"), Gremlin->HearNoise(Loud));
	TestEqual(TEXT("no gameplay event from retiring"), S.Events.FilterByPredicate([](FName E) { return E.ToString().StartsWith(TEXT("Event.Creature")) || E.ToString().StartsWith(TEXT("Event.Salvage")) || E.ToString().StartsWith(TEXT("Event.Glitch")); }).Num(), 0);
	S.Grid->PresentationBudgetMs = 1.5f;
	for (int32 F = 0; F < 200 && S.Placements->RetiringActorCount() > 0; ++F)
	{
		S.Grid->Advance(APDeepInLots);
	}
	TestEqual(TEXT("then destroyed within the budget"), S.Placements->RetiringActorCount(), 0);
	TestFalse(TEXT("  all three"), Lamp.IsValid() || Fence.IsValid() || Gremlin.IsValid());
	return true;
}

#endif
