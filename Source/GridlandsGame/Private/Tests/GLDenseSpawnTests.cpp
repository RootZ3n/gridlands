// P7 multi-frame presentation (ADR-0033) against the dense authored stress fixture (GLDenseProof):
// authoritative saved state is resolved before any part of a cell's presentation is made, and a
// partially presented cell never duplicates, resurrects, loses, replays or leaks anything.

#include "Building/GLCollapseRules.h"
#include "Combat/GLHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Character.h"
#include "HAL/FileManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Presentation/GLScatterPatch.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "UObject/UObjectIterator.h"
#include "World/GLGridSubsystem.h"
#include "World/GLPlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLDenseTests
{
	const FName DLots(TEXT("cell.outer.diner_lots"));
	const FVector DHome(0, -1200, 100);
	const FVector DInLots(120000, -20000, 100); // lots, south of the dense town (it is at local y >= 40 m)
	const FString DSlot(TEXT("gl-dense-test"));

	FName Dense(int32 Index) { return FName(*FString::Printf(TEXT("placement.diner_lots.proof_dense_%03d"), Index)); }
	// Fixture layout (GLDenseProofSites.inl): 0-9 storefronts, 10-19 carports, 20-43 pines, 44-63 phone tables, 64-83 boulders.
	const int32 DStructures = 84;

	struct FDenseScene
	{
		GLTestUtils::FTestWorld Test;
		ACharacter* Zenny = nullptr;
		UGLGridSubsystem* Grid = nullptr;
		UGLPlacementSubsystem* Placements = nullptr;
		UGLStructureSubsystem* Structures = nullptr;
		UGLTerrainSubsystem* Terrain = nullptr;
		TArray<FName> Events;

		explicit FDenseScene(const TCHAR* Name) : Test(Name)
		{
			UWorld* World = Test.World;
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event) { Events.Add(Event.Tag.GetTagName()); }));
			Zenny = World->SpawnActor<ACharacter>(DHome, FRotator::ZeroRotator);
			NewObject<UGLInventoryComponent>(Zenny)->RegisterComponent();
			NewObject<UGLHealthComponent>(Zenny)->RegisterComponent();
			Placements = World->GetSubsystem<UGLPlacementSubsystem>();
			Placements->AddDenseProof(); // before any cell streams in
			Grid = World->GetSubsystem<UGLGridSubsystem>();
			Grid->bShowBoundaries = false;
			Grid->Enable(false);
			Structures = World->GetSubsystem<UGLStructureSubsystem>();
			Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			GoTo(DHome);
		}

		void GoTo(const FVector& Where)
		{
			Zenny->SetActorLocation(Where);
			Grid->Advance(Where);
			Grid->FlushAll();
		}

		/** Streams toward Where like play until the lots' authoritative layer is in (ground is built on workers). */
		bool StepUntilAuthoritative(const FVector& Where)
		{
			Zenny->SetActorLocation(Where);
			for (int32 F = 0; F < 20000 && !Grid->IsRuntimeReady(DLots); ++F)
			{
				Grid->Advance(Where);
				if (!Grid->IsRuntimeReady(DLots))
				{
					FPlatformProcess::Sleep(0.001f);
				}
			}
			return Grid->IsRuntimeReady(DLots);
		}

		bool Salvage(FName Placement, FName Part)
		{
			AGLStructurePart* Actor = Structures->FindPart(Placement, Part);
			if (Actor)
			{
				Zenny->SetActorLocation(Actor->GetActorLocation() + FVector(-150, 0, 90));
			}
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

		const FGLStructurePartRuntime* PartOf(FName Placement, FName Part) const
		{
			const FGLStructureRuntime* S = Structures->Find(Placement);
			return S ? S->Parts.FindByPredicate([Part](const FGLStructurePartRuntime& P) { return P.Name == Part; }) : nullptr;
		}

		TArray<FGLSavedStructurePart> Facts() const
		{
			TArray<FGLSavedStructurePart> Out;
			Structures->CaptureCell(DLots, Out);
			return Out;
		}

		static bool IsSolid(const AActor* Actor)
		{
			TArray<UStaticMeshComponent*> Meshes;
			Actor->GetComponents(Meshes);
			for (const UStaticMeshComponent* M : Meshes)
			{
				if (M->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * The presentation invariant, checked against the world as it is right now: every live part
		 * actor of the fixture belongs to a part the authoritative model says is present, debris
		 * stands at its authoritative rest, and no part has two actors. Returns the problems found.
		 */
		int32 CheckPresentedParts(FAutomationTestBase& T, const TCHAR* When, int32* OutActors = nullptr) const
		{
			int32 Problems = 0, Actors = 0;
			TSet<FString> Seen;
			for (TActorIterator<AGLStructurePart> It(Test.World); It; ++It)
			{
				if (!IsValid(*It) || It->IsActorBeingDestroyed() || !It->StructurePlacement.ToString().Contains(TEXT("proof_dense")))
				{
					continue;
				}
				if (It->bRetired)
				{
					// Its cell unloaded: it must already be inert (hidden, no collision) while it waits to be destroyed.
					if (!It->IsHidden() || IsSolid(*It))
					{
						++Problems;
						T.AddError(FString::Printf(TEXT("%s: a retired actor of %s is still visible or solid"), When, *It->StructurePlacement.ToString()));
					}
					continue;
				}
				++Actors;
				const FString Key = It->StructurePlacement.ToString() + TEXT("/") + It->PartName.ToString();
				const FGLStructurePartRuntime* Part = PartOf(It->StructurePlacement, It->PartName);
				const bool bDuplicate = Seen.Contains(Key);
				Seen.Add(Key);
				const bool bGone = !Part || (Part->State != EGLStructurePartState::Intact && Part->State != EGLStructurePartState::Debris);
				const bool bDebrisElsewhere = Part && Part->State == EGLStructurePartState::Debris && Structures->ActiveCollapses() == 0
					&& !It->GetActorLocation().Equals(Part->Rest.GetLocation(), 1.0);
				if (bDuplicate || bGone || bDebrisElsewhere)
				{
					++Problems;
					T.AddError(FString::Printf(TEXT("%s: %s is %s"), When, *Key, bDuplicate ? TEXT("a duplicate") : bGone ? TEXT("resurrected (the model says it is gone)") : TEXT("debris away from its authoritative rest")));
				}
			}
			if (OutActors)
			{
				*OutActors = Actors;
			}
			return Problems;
		}

		/** Parts the authoritative model says are present (intact or debris), over the whole fixture. */
		int32 PresentParts() const
		{
			int32 N = 0;
			for (int32 I = 0; I < DStructures; ++I)
			{
				if (const FGLStructureRuntime* S = Structures->Find(Dense(I)))
				{
					for (const FGLStructurePartRuntime& P : S->Parts)
					{
						N += P.State == EGLStructurePartState::Intact || P.State == EGLStructurePartState::Debris ? 1 : 0;
					}
				}
			}
			return N;
		}

		int32 CountEvents(const TCHAR* Tag) const { return Events.FilterByPredicate([Tag](FName E) { return E == FName(Tag); }).Num(); }
	};

	bool SameFacts(const TArray<FGLSavedStructurePart>& A, const TArray<FGLSavedStructurePart>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 I = 0; I < A.Num(); ++I)
		{
			if (A[I].Placement != B[I].Placement || A[I].Part != B[I].Part || A[I].State != B[I].State || !A[I].Location.Equals(B[I].Location, 0.5))
			{
				return false;
			}
		}
		return true;
	}

	/** Changes a handful of the fixture's structures through the real salvage pipeline, and lets everything land. */
	void Damage(FDenseScene& S)
	{
		S.Salvage(Dense(10), TEXT("post_south"));
		S.Salvage(Dense(10), TEXT("post_north")); // both decks fall
		S.Salvage(Dense(20), TEXT("stump"));      // the trunk topples
		S.Salvage(Dense(64), TEXT("rock"));       // a boulder is gone
		S.Salvage(Dense(0), TEXT("post_w"));      // the storefront's three awning posts: the awning comes down
		S.Salvage(Dense(0), TEXT("post_m"));
		S.Salvage(Dense(0), TEXT("post_e"));
		S.Run(4.0);
		S.Salvage(Dense(10), TEXT("deck_west"));  // salvaged debris
		S.Run(0.5);
	}

	TMap<FName, int32> LiveByClass()
	{
		TMap<FName, int32> Count;
		for (TObjectIterator<UObject> It; It; ++It)
		{
			if (IsValid(*It))
			{
				++Count.FindOrAdd(It->GetClass()->GetFName());
			}
		}
		return Count;
	}
}

using GLDenseTests::FDenseScene;
using GLDenseTests::Dense;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDenseStands, "Gridlands.Game.Streaming.DenseFixtureStandsAndPresentsCompletely", GLTestUtils::Flags)
bool FGLDenseStands::RunTest(const FString& Parameters)
{
	FDenseScene S(TEXT("GLDenseStandsWorld"));
	S.GoTo(GLDenseTests::DInLots);
	int32 Parts = 0;
	for (int32 I = 0; I < GLDenseTests::DStructures; ++I)
	{
		const FGLStructureRuntime* Runtime = S.Structures->Find(Dense(I));
		if (!TestNotNull(*FString::Printf(TEXT("%s spawned"), *Dense(I).ToString()), Runtime))
		{
			continue;
		}
		TArray<FGLPlacedPiece> Pieces;
		for (const FGLStructurePartRuntime& Part : Runtime->Parts)
		{
			Pieces.Add(Part.Piece);
		}
		Parts += Pieces.Num();
		const TArray<int32> Loose = GLCollapseRules::Unsupported(GLContent::Get(), Pieces, [&S](const FVector2D& At) { return S.Terrain->HeightAt(At); });
		TestEqual(*FString::Printf(TEXT("%s: every part supported where it stands"), *Dense(I).ToString()), Loose.Num(), 0);
	}
	S.Grid->Advance(GLDenseTests::DInLots); // completion is recorded by the next streaming frame
	int32 Actors = 0;
	TestEqual(TEXT("presented cleanly"), S.CheckPresentedParts(*this, TEXT("flushed"), &Actors), 0);
	TestEqual(TEXT("every part of every fixture structure has its actor"), Actors, Parts);
	TestTrue(TEXT("the lots are fully presented"), S.Placements->IsCellPresented(GLDenseTests::DLots) && S.Grid->IsComplete(GLDenseTests::DLots));
	int32 Patches = 0;
	for (TActorIterator<AGLScatterPatch> It(S.Test.World); It; ++It)
	{
		Patches += IsValid(*It) ? 1 : 0;
	}
	TestTrue(TEXT("the fixture's vegetation is presented too (8 patches besides the slice's 4)"), Patches >= 12);
	AddInfo(FString::Printf(TEXT("dense fixture: %d structures, %d parts, %d vegetation patches in the lots"), GLDenseTests::DStructures, Parts, Patches));
	TestTrue(TEXT("the fixture is dense: ~12x the lots' 26 authored parts"), Parts >= 300);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDenseOrder, "Gridlands.Game.Streaming.PresentationNeverRunsAheadOfSavedState", GLTestUtils::Flags)
bool FGLDenseOrder::RunTest(const FString& Parameters)
{
	FDenseScene S(TEXT("GLDenseOrderWorld"));
	S.GoTo(GLDenseTests::DInLots);
	GLDenseTests::Damage(S);
	const TArray<FGLSavedStructurePart> Facts = S.Facts();
	TestTrue(TEXT("the fixture has changed facts to keep"), Facts.Num() >= 5);
	const int32 ImpactsBefore = S.Structures->GetImpacts().Num();
	S.GoTo(GLDenseTests::DHome);
	TestFalse(TEXT("the lots streamed out"), S.Grid->IsLoaded(GLDenseTests::DLots));
	const int32 EventsAway = S.Events.Num();

	// Come back streaming like play, with a presentation budget small enough to take many frames.
	S.Grid->PresentationBudgetMs = 0.05f;
	S.Grid->PresentationNearM = 0.f;
	TestTrue(TEXT("the lots' authoritative layer comes in"), S.StepUntilAuthoritative(GLDenseTests::DInLots));
	TestTrue(TEXT("saved state is resolved at once: the model already holds every kept fact"), GLDenseTests::SameFacts(S.Facts(), Facts));
	TestTrue(TEXT("while presentation is still waiting"), S.Placements->PendingPresentation() > 100);
	TestEqual(TEXT("the removed boulder has no actor, before or after"), S.Structures->FindPart(Dense(64), TEXT("rock")), static_cast<AGLStructurePart*>(nullptr));

	// Every frame of the incremental presentation: nothing present that the model says is gone, no
	// debris away from its rest, no duplicates. Save in the middle of it.
	int32 Frames = 0, Problems = 0;
	bool bSavedMid = false;
	while (!S.Placements->IsCellPresented(GLDenseTests::DLots) && Frames < 20000)
	{
		S.Grid->Advance(GLDenseTests::DInLots);
		++Frames;
		Problems += S.CheckPresentedParts(*this, *FString::Printf(TEXT("frame %d"), Frames));
		if (!bSavedMid && S.Placements->PendingPresentation() < 150)
		{
			bSavedMid = true;
			TestTrue(TEXT("a save mid-presentation keeps every fact (nothing waiting is lost)"), GLDenseTests::SameFacts(S.Facts(), Facts));
			TestTrue(TEXT("and writes"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(GLDenseTests::DSlot));
		}
	}
	AddInfo(FString::Printf(TEXT("presented over %d frames at 0.05 ms per frame"), Frames));
	TestTrue(TEXT("presentation took many frames (the proof is incremental)"), Frames > 20);
	TestEqual(TEXT("no frame showed a zombie, a duplicate or displaced debris"), Problems, 0);
	TestTrue(TEXT("saved in the middle"), bSavedMid);
	int32 Actors = 0;
	S.CheckPresentedParts(*this, TEXT("presented"), &Actors);
	TestEqual(TEXT("exactly one actor per present part"), Actors, S.PresentParts());
	TestEqual(TEXT("streaming made no gameplay events"), S.Events.Num(), EventsAway);
	TestEqual(TEXT("no impact replayed"), S.Structures->GetImpacts().Num(), ImpactsBefore);
	TestEqual(TEXT("no collapse running"), S.Structures->ActiveCollapses(), 0);

	// Restart from the mid-presentation save: the same facts.
	FDenseScene R(TEXT("GLDenseRestartWorld"));
	TArray<FString> Load;
	TestTrue(TEXT("restart loads"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(GLDenseTests::DSlot, &Load));
	TestEqual(TEXT("no load problems"), Load.Num(), 0);
	R.GoTo(GLDenseTests::DInLots);
	TestTrue(TEXT("after restart: the same facts"), GLDenseTests::SameFacts(R.Facts(), Facts));
	R.CheckPresentedParts(*this, TEXT("after restart"), &Actors);
	TestEqual(TEXT("after restart: one actor per present part"), Actors, R.PresentParts());
	TestFalse(TEXT("no collapse event on load"), R.Events.Contains(FName(TEXT("Event.Structure.Collapsed"))));
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(GLDenseTests::DSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDenseCancel, "Gridlands.Game.Streaming.UnloadDuringPresentationCancelsCleanly", GLTestUtils::Flags)
bool FGLDenseCancel::RunTest(const FString& Parameters)
{
	FDenseScene S(TEXT("GLDenseCancelWorld"));
	S.GoTo(GLDenseTests::DInLots);
	GLDenseTests::Damage(S);
	const TArray<FGLSavedStructurePart> Facts = S.Facts();
	S.GoTo(GLDenseTests::DHome);
	const int32 EventsAway = S.Events.Num();
	S.Grid->PresentationBudgetMs = 0.05f;
	S.Grid->PresentationNearM = 0.f;
	TMap<FName, int32> AfterFirst;
	for (int32 Cycle = 0; Cycle < 3; ++Cycle)
	{
		TestTrue(*FString::Printf(TEXT("cycle %d: authoritative layer in"), Cycle), S.StepUntilAuthoritative(GLDenseTests::DInLots));
		for (int32 F = 0; F < 30; ++F)
		{
			S.Grid->Advance(GLDenseTests::DInLots);
		}
		int32 Actors = 0;
		S.CheckPresentedParts(*this, TEXT("partially presented"), &Actors);
		TestTrue(*FString::Printf(TEXT("cycle %d: partially presented (%d actors made, %d waiting)"), Cycle, Actors, S.Placements->PendingPresentation()),
			Actors > 0 && S.Placements->PendingPresentation() > 0);
		// Leave mid-presentation, one streaming frame (no flush): the cell unloads with work still queued
		// and its made actors are retired, not destroyed, in that frame.
		S.Zenny->SetActorLocation(GLDenseTests::DHome);
		S.Grid->Advance(GLDenseTests::DHome);
		TestFalse(TEXT("  unloaded"), S.Grid->IsLoaded(GLDenseTests::DLots));
		const int32 Retired = S.Structures->RetiringActors();
		// A retired actor is out of the game: salvaging it (even by calling the pipeline directly) does nothing.
		for (TActorIterator<AGLStructurePart> It(S.Test.World); It; ++It)
		{
			if (It->bRetired && !It->IsActorBeingDestroyed())
			{
				TestTrue(TEXT("  a retired actor is marked out of the salvage pipeline"), It->GetSalvageable()->IsSalvaged());
				TestFalse(TEXT("  and refuses salvage"), It->GetSalvageable()->Interact(S.Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage"))));
				break;
			}
		}
		TestTrue(TEXT("  its made actors are retired, waiting"), Retired > 0);
		AddInfo(FString::Printf(TEXT("cycle %d: %d actors made before the unload, %d retired in the unload frame"), Cycle, Actors, Retired));
		TestTrue(TEXT("  its waiting presentation was cancelled (the origin's own may be arriving)"), S.Placements->IsCellPresented(GLDenseTests::DLots));
		TestEqual(TEXT("  no live fixture actor survives the unload (retired ones are inert)"), S.CheckPresentedParts(*this, TEXT("unloaded"), &Actors), 0);
		TestEqual(TEXT("  live fixture actors"), Actors, 0);
		int32 Drain = 0;
		for (; Drain < 20000 && (S.Structures->RetiringActors() > 0 || Drain < 60); ++Drain)
		{
			S.Grid->Advance(GLDenseTests::DHome); // stale work would land here; retired actors are destroyed here
		}
		S.CheckPresentedParts(*this, TEXT("after unload"), &Actors);
		TestEqual(TEXT("  no stale presentation landed after the unload"), Actors, 0);
		TestEqual(TEXT("  every retired actor was destroyed, within the budget"), S.Structures->RetiringActors(), 0);
		AddInfo(FString::Printf(TEXT("cycle %d: retired actors destroyed over %d frames at 0.05 ms"), Cycle, Drain));
		// Count in a settled state: an automation world never finishes async collision cooks (each pending
		// cook holds a queued body setup), so finish streaming, including collision, before counting.
		S.Grid->FlushAll();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		if (Cycle == 0)
		{
			AfterFirst = GLDenseTests::LiveByClass();
		}
	}
	FString Grew;
	for (const TPair<FName, int32>& Class : GLDenseTests::LiveByClass())
	{
		if (Class.Value > AfterFirst.FindRef(Class.Key))
		{
			Grew += FString::Printf(TEXT(" %s %d->%d"), *Class.Key.ToString(), AfterFirst.FindRef(Class.Key), Class.Value);
		}
	}
	TestTrue(FString::Printf(TEXT("cancelled presentations leak nothing (grew:%s)"), Grew.IsEmpty() ? TEXT(" nothing") : *Grew), Grew.IsEmpty());
	// Finally, all the way in: the kept facts, one actor each.
	S.Grid->PresentationBudgetMs = 1.5f;
	S.GoTo(GLDenseTests::DInLots);
	TestTrue(TEXT("the facts survived three cancelled presentations"), GLDenseTests::SameFacts(S.Facts(), Facts));
	int32 Actors = 0;
	TestEqual(TEXT("clean"), S.CheckPresentedParts(*this, TEXT("final"), &Actors), 0);
	TestEqual(TEXT("one actor per present part"), Actors, S.PresentParts());
	TestEqual(TEXT("nothing replayed: streaming made no gameplay events"), S.Events.Num(), EventsAway);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLDenseFalling, "Gridlands.Game.Streaming.ACollapseDecidedBeforePresentationShowsTheDecision", GLTestUtils::Flags)
bool FGLDenseFalling::RunTest(const FString& Parameters)
{
	// Presentation paused: the carport's model is in, its actors are not. Present only the posts, take
	// them: both decks are decided while they are still waiting. Whatever is presented afterwards
	// shows the decision (falling, not solid; then debris at rest), never the intact deck.
	FDenseScene S(TEXT("GLDenseFallingWorld"));
	S.Grid->PresentationBudgetMs = -1.f;
	S.Grid->PresentationNearM = 0.f;
	TestTrue(TEXT("authoritative layer in"), S.StepUntilAuthoritative(GLDenseTests::DInLots));
	const FName Carport = Dense(11);
	TestNull(TEXT("paused: the deck is not presented"), S.Structures->FindPart(Carport, TEXT("deck_west")));
	TestTrue(TEXT("present the south post"), S.Structures->PresentPart(Carport, TEXT("post_south")));
	TestTrue(TEXT("present the north post"), S.Structures->PresentPart(Carport, TEXT("post_north")));
	const int32 CollapsedBefore = S.CountEvents(TEXT("Event.Structure.Collapsed"));
	TestTrue(TEXT("south post salvaged"), S.Salvage(Carport, TEXT("post_south")));
	TestTrue(TEXT("north post salvaged"), S.Salvage(Carport, TEXT("post_north")));
	TestEqual(TEXT("both waiting decks were decided"), S.Structures->ActiveCollapses(), 2);
	TestEqual(TEXT("one collapse event"), S.CountEvents(TEXT("Event.Structure.Collapsed")), CollapsedBefore + 1);
	S.Run(0.5); // past the 0.3 s creak (startDelaySeconds): falling
	TestTrue(TEXT("the west deck is presented mid-fall"), S.Structures->PresentPart(Carport, TEXT("deck_west")));
	AGLStructurePart* West = S.Structures->FindPart(Carport, TEXT("deck_west"));
	const FGLStructurePartRuntime* WestPart = S.PartOf(Carport, TEXT("deck_west"));
	if (!TestNotNull(TEXT("made"), West) || !TestNotNull(TEXT("model"), WestPart))
	{
		return false;
	}
	TestFalse(TEXT("falling: not solid (nothing stands on a falling deck)"), FDenseScene::IsSolid(West));
	TestFalse(TEXT("not at its intact place"), West->GetActorLocation().Equals(WestPart->Piece.Location, 5.0));
	TestNull(TEXT("the east deck is still waiting"), S.Structures->FindPart(Carport, TEXT("deck_east")));
	S.Run(3.0);
	TestEqual(TEXT("landed"), S.Structures->ActiveCollapses(), 0);
	TestTrue(TEXT("the west deck rests where the plan said"), West->GetActorLocation().Equals(WestPart->Rest.GetLocation(), 1.0));
	TestTrue(TEXT("solid debris"), FDenseScene::IsSolid(West));
	S.Grid->PresentationBudgetMs = 1.5f;
	S.Grid->FlushAll();
	int32 Actors = 0;
	TestEqual(TEXT("clean after the rest is presented"), S.CheckPresentedParts(*this, TEXT("flushed"), &Actors), 0);
	TestEqual(TEXT("the carport: its two decks as debris, no duplicates, no posts"), [&]() {
		int32 N = 0;
		for (TActorIterator<AGLStructurePart> It(S.Test.World); It; ++It) { N += IsValid(*It) && !It->IsActorBeingDestroyed() && It->StructurePlacement == Carport ? 1 : 0; }
		return N; }(), 2);
	AGLStructurePart* East = S.Structures->FindPart(Carport, TEXT("deck_east"));
	const FGLStructurePartRuntime* EastPart = S.PartOf(Carport, TEXT("deck_east"));
	TestTrue(TEXT("the east deck, never presented intact, is debris at its rest"), East && EastPart && East->GetActorLocation().Equals(EastPart->Rest.GetLocation(), 1.0));
	TestEqual(TEXT("each impact landed once"), S.Structures->GetImpacts().Num(), 2);
	return true;
}

#endif
