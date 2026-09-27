// P8 production-density fixture (GLTownBlock, -GLTownBlock): a credible town block on the crossing route. It
// stands where it is authored, presents completely, and under it the model/presentation split holds: saved
// state is resolved before any actor, a save mid-presentation and a restart keep every fact, and streaming
// out mid-presentation (cancellation) and back again (reversal) never duplicates, resurrects or leaks.

#include "Building/GLCollapseRules.h"
#include "Building/GLStructureRules.h"
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
#include "HAL/FileManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLScanComponent.h"
#include "Presentation/GLScatterPatch.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLGridSubsystem.h"
#include "World/GLPlacementSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLTownBlockTests
{
	const FName TBLots(TEXT("cell.outer.diner_lots"));
	const FVector TBHome(0, -1200, 100);
	const FVector TBInTown(63900, -30000, 100); // on the street, mid-block
	const FString TBSlot(TEXT("gl-town-test"));
	const double TBRouteY = -30000.0; // world y of the crossing route (gl.Perf.Crossing)

	bool IsTown(FName Placement) { return Placement.ToString().Contains(TEXT("proof_town")); }

	/** What the town block's actors are right now (live, not retired), per kind, with duplicates counted. */
	struct FTownActors
	{
		int32 Parts = 0, Nodes = 0, Creatures = 0, Glitches = 0, Duplicates = 0, Solid = 0;
	};

	struct FTownScene
	{
		GLTestUtils::FTestWorld Test;
		ACharacter* Zenny = nullptr;
		AGLPehlichi* Pehlichi = nullptr;
		UGLGridSubsystem* Grid = nullptr;
		UGLPlacementSubsystem* Placements = nullptr;
		UGLStructureSubsystem* Structures = nullptr;
		UGLGlitchSubsystem* Glitches = nullptr;
		UGLTerrainSubsystem* Terrain = nullptr;
		int32 Registered = 0;
		TArray<FName> Events;

		explicit FTownScene(const TCHAR* Name) : Test(Name)
		{
			UWorld* World = Test.World;
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event) { Events.Add(Event.Tag.GetTagName()); }));
			Zenny = World->SpawnActor<ACharacter>(TBHome, FRotator::ZeroRotator);
			NewObject<UGLInventoryComponent>(Zenny)->RegisterComponent();
			NewObject<UGLHealthComponent>(Zenny)->RegisterComponent();
			Placements = World->GetSubsystem<UGLPlacementSubsystem>();
			Registered = Placements->AddTownBlock(); // before any cell streams in
			Glitches = World->GetSubsystem<UGLGlitchSubsystem>();
			Glitches->SetCommander(Zenny);
			Pehlichi = World->SpawnActor<AGLPehlichi>(TBHome + FVector(0, 200, 0), FRotator::ZeroRotator);
			Grid = World->GetSubsystem<UGLGridSubsystem>();
			Grid->bShowBoundaries = false;
			Grid->Enable(false);
			Structures = World->GetSubsystem<UGLStructureSubsystem>();
			Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			GoTo(TBHome);
		}

		void GoTo(const FVector& Where)
		{
			Zenny->SetActorLocation(Where);
			Grid->Advance(Where);
			Grid->FlushAll();
		}

		/** Streams toward Where like play until the lots' authoritative layer is in (bLoaded) or out. */
		bool StepUntil(const FVector& Where, bool bLoaded)
		{
			Zenny->SetActorLocation(Where);
			auto Done = [&] { return bLoaded ? Grid->IsRuntimeReady(TBLots) : !Grid->IsLoaded(TBLots); };
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

		/** The town placements of one kind, in layout order (from the models: the lots must be loaded). */
		TArray<FName> OfKind(const TCHAR* Kind) const
		{
			TArray<FName> Out;
			for (int32 I = 0; I < Registered; ++I)
			{
				const FName Id = UGLPlacementSubsystem::TownBlockId(I);
				const FGLActorPlacement* Model = Placements->FindActorModel(Id);
				const bool bMatch = FCString::Strcmp(Kind, TEXT("structure")) == 0 ? Structures->Find(Id) != nullptr
					: FCString::Strcmp(Kind, TEXT("glitch")) == 0 ? Glitches->FindRecord(Id) != nullptr
					: Model && Model->Kind == FName(Kind);
				if (bMatch)
				{
					Out.Add(Id);
				}
			}
			return Out;
		}

		/** Parts of the town's structures the model says are present (intact or debris). */
		int32 PresentParts() const
		{
			int32 N = 0;
			for (const FName& Id : OfKind(TEXT("structure")))
			{
				for (const FGLStructurePartRuntime& P : Structures->Find(Id)->Parts)
				{
					N += P.State == EGLStructurePartState::Intact || P.State == EGLStructurePartState::Debris ? 1 : 0;
				}
			}
			return N;
		}

		FTownActors Actors() const
		{
			FTownActors A;
			TSet<FString> Seen;
			auto Note = [&](const FString& Key) { bool bDup = false; Seen.Add(Key, &bDup); A.Duplicates += bDup ? 1 : 0; };
			for (TActorIterator<AGLStructurePart> It(Test.World); It; ++It)
			{
				if (IsValid(*It) && !It->IsActorBeingDestroyed() && IsTown(It->StructurePlacement))
				{
					if (It->bRetired)
					{
						A.Solid += !It->IsHidden() || It->GetActorEnableCollision() ? 1 : 0;
						continue;
					}
					++A.Parts;
					Note(It->StructurePlacement.ToString() + TEXT("/") + It->PartName.ToString());
				}
			}
			for (TActorIterator<AGLSalvageNode> It(Test.World); It; ++It)
			{
				if (IsValid(*It) && IsTown(It->PlacementId) && !It->GetSalvageable()->IsSalvaged())
				{
					++A.Nodes;
					Note(It->PlacementId.ToString());
				}
			}
			for (TActorIterator<AGLCreature> It(Test.World); It; ++It)
			{
				if (IsValid(*It) && IsTown(It->GetPlacementId()) && !It->IsDefeated())
				{
					++A.Creatures;
					Note(It->GetPlacementId().ToString());
				}
			}
			for (TActorIterator<AGLGlitch> It(Test.World); It; ++It)
			{
				if (IsValid(*It) && !It->IsHidden() && IsTown(It->GetGlitch()->GetPlacementId()))
				{
					++A.Glitches;
					Note(It->GetGlitch()->GetPlacementId().ToString());
				}
			}
			return A;
		}

		/** The town's kept facts, as a save would write them (from the models). */
		FGLSavedCell Facts() const
		{
			FGLSavedCell Cell = Test.World->GetSubsystem<UGLSaveSubsystem>()->CaptureCell(TBLots);
			Cell.StructureParts.RemoveAll([](const FGLSavedStructurePart& P) { return !IsTown(P.Placement); });
			Cell.Glitches.RemoveAll([](const FGLSavedGlitch& G) { return !IsTown(G.Placement); });
			Cell.SalvagedPlacements.RemoveAll([](FName P) { return !IsTown(P); });
			Cell.DefeatedCreatures.RemoveAll([](FName P) { return !IsTown(P); });
			return Cell;
		}

		void Salvage(AActor* Target, UGLSalvageableComponent* Salvageable)
		{
			if (Target && Salvageable)
			{
				Zenny->SetActorLocation(Target->GetActorLocation() + FVector(-150, 0, 90));
				for (int32 Hit = 0; Hit < 50 && !Salvageable->IsSalvaged(); ++Hit)
				{
					Salvageable->Interact(Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
				}
			}
		}

		/** Changes the town through the real pipelines: a storefront's awning posts, a street tree, a junk pile, a gremlin, a glitch scanned. */
		void Change()
		{
			const TArray<FName> Built = OfKind(TEXT("structure"));
			for (const TCHAR* Post : { TEXT("post_w"), TEXT("post_m"), TEXT("post_e") })
			{
				AGLStructurePart* Part = Structures->FindPart(Built[0], Post);
				Salvage(Part, Part ? Part->GetSalvageable() : nullptr);
			}
			const FName* Pine = Built.FindByPredicate([this](FName Id) { return Structures->Find(Id)->Def == FName(TEXT("structure.nature.pine_tree")); });
			if (AGLStructurePart* Stump = Pine ? Structures->FindPart(*Pine, TEXT("stump")) : nullptr)
			{
				Salvage(Stump, Stump->GetSalvageable());
			}
			AGLSalvageNode* Node = Placements->FindSalvageNode(OfKind(TEXT("salvage_node"))[0]);
			Salvage(Node, Node ? Node->GetSalvageable() : nullptr);
			if (AGLCreature* Gremlin = Placements->FindCreature(OfKind(TEXT("spawn"))[0]))
			{
				Gremlin->GetHealth()->ApplyDamage(1000.0, Zenny);
			}
			if (AGLGlitch* Glitch = Glitches->FindByPlacement(OfKind(TEXT("glitch"))[0]))
			{
				Zenny->SetActorLocation(Glitch->GetActorLocation() + FVector(0, -300, 100));
				Pehlichi->SetActorLocation(Glitch->GetActorLocation() + FVector(0, -200, 0));
				Pehlichi->GetScan()->Scan();
			}
			for (double T = 0.0; T < 4.0; T += 1.0 / 60.0)
			{
				Structures->Advance(1.0 / 60.0);
			}
		}

		/** Presents what is waiting, frame by frame like play, checking the invariant every frame. Returns the frames it took. */
		int32 PresentAll(FAutomationTestBase& T, const FVector& Where, int32& OutProblems, TFunctionRef<void()> EachFrame)
		{
			int32 Frames = 0;
			while (!Placements->IsCellPresented(TBLots) && Frames < 40000)
			{
				Grid->Advance(Where);
				++Frames;
				const FTownActors A = Actors();
				if (A.Duplicates > 0 || A.Solid > 0)
				{
					++OutProblems;
					T.AddError(FString::Printf(TEXT("frame %d: %d duplicate actor(s), %d retired actor(s) still visible or solid"), Frames, A.Duplicates, A.Solid));
				}
				EachFrame();
			}
			return Frames;
		}
	};

	bool SameFacts(const FGLSavedCell& A, const FGLSavedCell& B)
	{
		if (A.StructureParts.Num() != B.StructureParts.Num() || A.Glitches.Num() != B.Glitches.Num()
			|| A.SalvagedPlacements != B.SalvagedPlacements || A.DefeatedCreatures != B.DefeatedCreatures)
		{
			return false;
		}
		for (int32 I = 0; I < A.StructureParts.Num(); ++I)
		{
			const FGLSavedStructurePart& X = A.StructureParts[I];
			const FGLSavedStructurePart& Y = B.StructureParts[I];
			if (X.Placement != Y.Placement || X.Part != Y.Part || X.State != Y.State || !X.Location.Equals(Y.Location, 0.5))
			{
				return false;
			}
		}
		for (int32 I = 0; I < A.Glitches.Num(); ++I)
		{
			if (A.Glitches[I].Placement != B.Glitches[I].Placement || A.Glitches[I].State != B.Glitches[I].State)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLTownStands, "Gridlands.Game.TownBlock.StandsOnTheRouteAndPresentsCompletely", GLTestUtils::Flags)
bool FGLTownStands::RunTest(const FString& Parameters)
{
	GLTownBlockTests::FTownScene S(TEXT("GLTownStandsWorld"));
	S.GoTo(GLTownBlockTests::TBInTown);
	const TArray<FName> Built = S.OfKind(TEXT("structure"));
	const TArray<FName> Nodes = S.OfKind(TEXT("salvage_node"));
	const TArray<FName> Spawns = S.OfKind(TEXT("spawn"));
	const TArray<FName> Glitched = S.OfKind(TEXT("glitch"));
	TestEqual(TEXT("72 structures"), Built.Num(), 72);
	TestEqual(TEXT("12 salvage nodes"), Nodes.Num(), 12);
	TestEqual(TEXT("3 creatures"), Spawns.Num(), 3);
	TestEqual(TEXT("6 glitches"), Glitched.Num(), 6);
	int32 Parts = 0;
	TArray<TPair<FName, FBox2D>> Footprints;
	for (const FName& Id : Built)
	{
		const FGLStructureRuntime* Runtime = S.Structures->Find(Id);
		TArray<FGLPlacedPiece> Pieces;
		FBox2D Box(ForceInit);
		for (const FGLStructurePartRuntime& Part : Runtime->Parts)
		{
			Pieces.Add(Part.Piece);
			if (const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(Part.Piece.Def))
			{
				const FBox B = GLStructureRules::Bounds(*Def, Part.Piece);
				Box += FVector2D(B.Min);
				Box += FVector2D(B.Max);
			}
		}
		Parts += Pieces.Num();
		TestEqual(*FString::Printf(TEXT("%s: every part supported where it stands"), *Id.ToString()),
			GLCollapseRules::Unsupported(GLContent::Get(), Pieces, [&S](const FVector2D& At) { return S.Terrain->HeightAt(At); }).Num(), 0);
		const FBox2D Shrunk(Box.Min + FVector2D(10), Box.Max - FVector2D(10));
		for (const TPair<FName, FBox2D>& Other : Footprints)
		{
			TestFalse(*FString::Printf(TEXT("%s overlaps %s"), *Id.ToString(), *Other.Key.ToString()), Other.Value.Intersect(Shrunk));
		}
		Footprints.Add({ Id, Box });
		TestTrue(*FString::Printf(TEXT("%s: on the crossing street (within 45 m of the route)"), *Id.ToString()), FMath::Abs(Box.GetCenter().Y - GLTownBlockTests::TBRouteY) < 4500.0);
	}
	for (const TArray<FName>* Kind : { &Nodes, &Spawns, &Glitched })
	{
		for (const FName& Id : *Kind)
		{
			const FGLActorPlacement* Model = S.Placements->FindActorModel(Id);
			const FGLGlitchRecord* Record = S.Glitches->FindRecord(Id);
			const FVector At = Model ? Model->Location : Record ? Record->Location : FVector::ZeroVector;
			TestTrue(*FString::Printf(TEXT("%s stands on the ground (authored z within 0.5 m of it)"), *Id.ToString()),
				FMath::Abs(At.Z - S.Terrain->HeightAt(FVector2D(At))) < 50.0);
		}
	}
	const FGLGlitchRecord* Lamp = nullptr;
	for (const FName& Id : Glitched)
	{
		const FGLGlitchRecord* R = S.Glitches->FindRecord(Id);
		Lamp = R && R->Glitch == FName(TEXT("glitch.home.flicker_lamp")) ? R : Lamp;
	}
	TestTrue(TEXT("the town's lamp is blocked by the town's own junk pile"), Lamp && Lamp->Bindings.FindRef(TEXT("blocker")) == Nodes[0]);

	S.Grid->Advance(GLTownBlockTests::TBInTown);
	const GLTownBlockTests::FTownActors A = S.Actors();
	TestTrue(TEXT("the lots are fully presented"), S.Placements->IsCellPresented(GLTownBlockTests::TBLots) && S.Grid->IsComplete(GLTownBlockTests::TBLots));
	TestEqual(TEXT("one actor per part"), A.Parts, Parts);
	TestEqual(TEXT("one actor per salvage node"), A.Nodes, Nodes.Num());
	TestEqual(TEXT("one actor per creature"), A.Creatures, Spawns.Num());
	TestEqual(TEXT("one actor per glitch"), A.Glitches, Glitched.Num());
	TestEqual(TEXT("no duplicates"), A.Duplicates, 0);
	int32 Patches = 0, Instances = 0;
	for (TActorIterator<AGLScatterPatch> It(S.Test.World); It; ++It)
	{
		if (IsValid(*It) && GLTownBlockTests::IsTown(It->GetPlacementId()))
		{
			++Patches;
			Instances += It->GetInstanceCount();
		}
	}
	TestEqual(TEXT("12 vegetation patches"), Patches, 12);
	TestTrue(TEXT("with thousands of instances (grass does not grow under buildings)"), Instances > 2000);
	AddInfo(FString::Printf(TEXT("town block: %d structures, %d parts, %d salvage nodes, %d creatures, %d glitches, %d vegetation patches (%d instances)"),
		Built.Num(), Parts, Nodes.Num(), Spawns.Num(), Glitched.Num(), Patches, Instances));
	TestTrue(TEXT("production density: more parts than the P7 stress strip (308)"), Parts > 308);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLTownSaveRestart, "Gridlands.Game.TownBlock.SaveMidPresentationAndRestartKeepEveryFact", GLTestUtils::Flags)
bool FGLTownSaveRestart::RunTest(const FString& Parameters)
{
	GLTownBlockTests::FTownScene S(TEXT("GLTownSaveWorld"));
	S.GoTo(GLTownBlockTests::TBInTown);
	S.Change();
	const FGLSavedCell Facts = S.Facts();
	TestTrue(TEXT("the town has changed facts to keep"), Facts.StructureParts.Num() >= 4 && Facts.SalvagedPlacements.Num() == 1 && Facts.DefeatedCreatures.Num() == 1
		&& Facts.Glitches.ContainsByPredicate([](const FGLSavedGlitch& G) { return G.State != EGLGlitchState::Latent; }));
	const FName Salvaged = Facts.SalvagedPlacements.IsEmpty() ? NAME_None : Facts.SalvagedPlacements[0];
	const FName Defeated = Facts.DefeatedCreatures.IsEmpty() ? NAME_None : Facts.DefeatedCreatures[0];
	S.GoTo(GLTownBlockTests::TBHome);
	TestFalse(TEXT("the lots streamed out"), S.Grid->IsLoaded(GLTownBlockTests::TBLots));
	const int32 EventsAway = S.Events.Num();

	S.Grid->PresentationBudgetMs = 0.05f;
	S.Grid->PresentationNearM = 0.f;
	TestTrue(TEXT("the lots' authoritative layer comes in"), S.StepUntil(GLTownBlockTests::TBInTown, true));
	TestTrue(TEXT("saved state is resolved at once: the models hold every kept fact"), GLTownBlockTests::SameFacts(S.Facts(), Facts));
	TestTrue(TEXT("while presentation still waits"), S.Placements->PendingPresentation() > 100);
	// Gameplay actors are presentation under the same budget as parts: the frame the authoritative layer
	// landed in made only what is near (nothing, at PresentationNearM 0), and the next one little.
	auto WaitingActors = [&S]
	{
		int32 N = 0;
		for (const TCHAR* Kind : { TEXT("salvage_node"), TEXT("spawn"), TEXT("glitch") })
		{
			for (const FName& Id : S.OfKind(Kind))
			{
				N += S.Placements->IsActorPending(Id) ? 1 : 0;
			}
		}
		return N;
	};
	const int32 ActorsTotal = S.OfKind(TEXT("salvage_node")).Num() + S.OfKind(TEXT("spawn")).Num() + S.OfKind(TEXT("glitch")).Num();
	TestEqual(TEXT("the authoritative frame made no far gameplay actor"), WaitingActors(), ActorsTotal);
	S.Grid->Advance(GLTownBlockTests::TBInTown);
	TestTrue(TEXT("and the next frame made gameplay actors within the budget, not all at once"), WaitingActors() > ActorsTotal / 2);
	int32 Problems = 0;
	bool bSavedMid = false;
	const int32 Frames = S.PresentAll(*this, GLTownBlockTests::TBInTown, Problems, [&]
	{
		if (S.Placements->FindSalvageNode(Salvaged) || S.Placements->FindCreature(Defeated))
		{
			++Problems;
			AddError(TEXT("the salvaged node or the defeated gremlin was made"));
		}
		if (!bSavedMid && S.Placements->PendingPresentation() < 200)
		{
			bSavedMid = true;
			TestTrue(TEXT("a save mid-presentation keeps every fact"), GLTownBlockTests::SameFacts(S.Facts(), Facts));
			TestTrue(TEXT("and writes"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(GLTownBlockTests::TBSlot));
		}
	});
	AddInfo(FString::Printf(TEXT("town presented over %d frames at 0.05 ms per frame"), Frames));
	TestTrue(TEXT("presentation took many frames"), Frames > 20);
	TestTrue(TEXT("saved in the middle"), bSavedMid);
	TestEqual(TEXT("no frame showed a duplicate or a live retired actor"), Problems, 0);
	GLTownBlockTests::FTownActors A = S.Actors();
	TestEqual(TEXT("one actor per present part"), A.Parts, S.PresentParts());
	TestEqual(TEXT("one node per unsalvaged node"), A.Nodes, S.OfKind(TEXT("salvage_node")).Num() - 1);
	TestEqual(TEXT("one creature per undefeated spawn"), A.Creatures, S.OfKind(TEXT("spawn")).Num() - 1);
	TestEqual(TEXT("one actor per glitch"), A.Glitches, S.OfKind(TEXT("glitch")).Num());
	TestEqual(TEXT("presenting made no gameplay events"), S.Events.Num(), EventsAway);

	GLTownBlockTests::FTownScene R(TEXT("GLTownRestartWorld"));
	TArray<FString> Load;
	TestTrue(TEXT("restart loads"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(GLTownBlockTests::TBSlot, &Load));
	TestEqual(TEXT("no load problems"), Load.Num(), 0);
	R.GoTo(GLTownBlockTests::TBInTown);
	TestTrue(TEXT("after restart: the same facts"), GLTownBlockTests::SameFacts(R.Facts(), Facts));
	A = R.Actors();
	TestEqual(TEXT("after restart: one actor per present part"), A.Parts, R.PresentParts());
	TestTrue(TEXT("  the salvaged node and the defeated gremlin have no actor"), !R.Placements->FindSalvageNode(Salvaged) && !R.Placements->FindCreature(Defeated));
	TestEqual(TEXT("  one node per unsalvaged node"), A.Nodes, R.OfKind(TEXT("salvage_node")).Num() - 1);
	TestEqual(TEXT("  no duplicates"), A.Duplicates, 0);
	TestFalse(TEXT("no collapse event on load"), R.Events.Contains(FName(TEXT("Event.Structure.Collapsed"))));
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(GLTownBlockTests::TBSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLTownReversal, "Gridlands.Game.TownBlock.CancellationAndReversalNeverDuplicateOrLeak", GLTestUtils::Flags)
bool FGLTownReversal::RunTest(const FString& Parameters)
{
	GLTownBlockTests::FTownScene S(TEXT("GLTownReversalWorld"));
	S.GoTo(GLTownBlockTests::TBInTown);
	S.Change();
	const FGLSavedCell Facts = S.Facts();
	S.GoTo(GLTownBlockTests::TBHome);
	S.Grid->PresentationBudgetMs = 0.05f;
	S.Grid->PresentationNearM = 0.f;
	int32 Problems = 0;
	for (int32 Round = 0; Round < 3; ++Round)
	{
		// In, part-way through presenting, then turn back.
		TestTrue(*FString::Printf(TEXT("round %d: the lots come in"), Round), S.StepUntil(GLTownBlockTests::TBInTown, true));
		const int32 Waiting = S.Placements->PendingPresentation();
		for (int32 F = 0; F < 40000 && S.Placements->PendingPresentation() > Waiting * (Round + 1) / 4; ++F)
		{
			S.Grid->Advance(GLTownBlockTests::TBInTown);
		}
		const GLTownBlockTests::FTownActors Mid = S.Actors();
		TestTrue(*FString::Printf(TEXT("round %d: part-way presented (%d parts made)"), Round, Mid.Parts), Mid.Parts > 0 && !S.Placements->IsCellPresented(GLTownBlockTests::TBLots));
		Problems += Mid.Duplicates;
		TestTrue(*FString::Printf(TEXT("round %d: reversed out before presentation finished"), Round), S.StepUntil(GLTownBlockTests::TBHome, false));
		const GLTownBlockTests::FTownActors Out = S.Actors();
		TestEqual(*FString::Printf(TEXT("round %d: out: no live town actor"), Round), Out.Parts + Out.Nodes + Out.Creatures + Out.Glitches, 0);
		TestEqual(*FString::Printf(TEXT("round %d: out: retired actors inert at once"), Round), Out.Solid, 0);
		TestEqual(*FString::Printf(TEXT("round %d: out: nothing waits"), Round), S.Placements->PendingPresentation(), 0);
		for (int32 F = 0; F < 40000 && S.Placements->RetiringActorCount() > 0; ++F)
		{
			S.Grid->Advance(GLTownBlockTests::TBHome);
		}
		TestEqual(*FString::Printf(TEXT("round %d: retired actors destroyed within the budget"), Round), S.Placements->RetiringActorCount(), 0);
	}
	S.Grid->PresentationBudgetMs = 1.5f;
	S.Grid->PresentationNearM = 20.f;
	TestTrue(TEXT("back in for good"), S.StepUntil(GLTownBlockTests::TBInTown, true));
	S.PresentAll(*this, GLTownBlockTests::TBInTown, Problems, [] {});
	TestEqual(TEXT("no duplicates or live retired actors at any point"), Problems, 0);
	TestTrue(TEXT("the same facts after three reversals"), GLTownBlockTests::SameFacts(S.Facts(), Facts));
	const GLTownBlockTests::FTownActors A = S.Actors();
	TestEqual(TEXT("one actor per present part"), A.Parts, S.PresentParts());
	TestEqual(TEXT("one node per unsalvaged node"), A.Nodes, S.OfKind(TEXT("salvage_node")).Num() - 1);
	TestEqual(TEXT("one creature per undefeated spawn"), A.Creatures, S.OfKind(TEXT("spawn")).Num() - 1);
	TestEqual(TEXT("one actor per glitch"), A.Glitches, S.OfKind(TEXT("glitch")).Num());
	return true;
}

#endif
