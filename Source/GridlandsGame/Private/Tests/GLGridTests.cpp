#include "Building/GLBuildingSubsystem.h"
#include "Combat/GLCombatComponent.h"
#include "Combat/GLCreature.h"
#include "Combat/GLHealthComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Dialogue/GLDialogueDirector.h"
#include "EngineUtils.h"
#include "Events/GLEventSubsystem.h"
#include "GameFramework/Character.h"
#include "Glitch/GLGlitch.h"
#include "Glitch/GLGlitchComponent.h"
#include "Glitch/GLGlitchSubsystem.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "UObject/GarbageCollection.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLPehlichi.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
#include "Pehlichi/GLRepairComponent.h"
#include "Pehlichi/GLScanComponent.h"
#include "Salvage/GLSalvageNode.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Terrain/GLTerrainChunk.h"
#include "Tests/GLTestUtils.h"
#include "World/GLGridSubsystem.h"
#include "World/GLPlacementSubsystem.h"
#include "World/GLStabilitySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLGridTests
{
	const FName GOrigin(TEXT("cell.home.origin"));
	const FName GLots(TEXT("cell.outer.diner_lots"));
	const FName GLamp(TEXT("placement.origin.glitch_flicker_lamp"));
	const FName GBlocker(TEXT("placement.origin.junk_pile_01"));
	const FName GGremlin(TEXT("placement.origin.drain_gremlin_den"));
	const FName GJukebox(TEXT("placement.diner_lots.glitch_jukebox"));
	const FVector GInOrigin(0, -1200, 100);
	const FVector GAtBoundary(51100, 0, 100);   // the origin side of x = 512 m, both cells loaded
	const FVector GDeepInLots(120000, 0, 100);  // far enough that the origin unloads (> 384 m past its edge)
	const FString GGridSlot = TEXT("automation-test-grid");

	/** A world with the Grid streamer (no level instances: the runtime layer only) and a pawn Zenny. */
	struct FGridScene
	{
		GLTestUtils::FTestWorld Test;
		ACharacter* Zenny = nullptr;
		UGLInventoryComponent* Inventory = nullptr;
		AGLPehlichi* Pehlichi = nullptr;
		UGLGridSubsystem* Grid = nullptr;
		UGLGlitchSubsystem* Glitches = nullptr;
		UGLBuildingSubsystem* Building = nullptr;
		UGLTerrainSubsystem* Terrain = nullptr;
		TArray<FName> Events;
		int32 Lines = 0;

		explicit FGridScene(const TCHAR* Name) : Test(Name)
		{
			UWorld* World = Test.World;
			World->GetSubsystem<UGLDialogueDirector>()->bShowOnScreen = false;
			World->GetSubsystem<UGLDialogueDirector>()->OnLine.AddLambda([this](const FGLDialogueLine&) { ++Lines; });
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event) { Events.Add(Event.Tag.GetTagName()); }));
			Zenny = World->SpawnActor<ACharacter>(GInOrigin, FRotator::ZeroRotator);
			Inventory = NewObject<UGLInventoryComponent>(Zenny);
			Inventory->RegisterComponent();
			NewObject<UGLHealthComponent>(Zenny)->RegisterComponent();
			Glitches = World->GetSubsystem<UGLGlitchSubsystem>();
			Glitches->SetCommander(Zenny);
			Pehlichi = World->SpawnActor<AGLPehlichi>(GInOrigin + FVector(0, 200, 0), FRotator::ZeroRotator);
			Grid = World->GetSubsystem<UGLGridSubsystem>();
			Grid->bShowBoundaries = false;
			Grid->Enable(false); // the runtime layer only; the real game proves the level instances
			Building = World->GetSubsystem<UGLBuildingSubsystem>();
			Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			World->GetSubsystem<UGLKnowledgeSubsystem>()->Learn(TEXT("knowledge.style.modern_timber_frame"));
			GoTo(GInOrigin);
		}

		void GoTo(const FVector& Where)
		{
			Zenny->SetActorLocation(Where);
			Grid->Advance(Where);
			Grid->FlushAll(); // these tests check state, not timing: finish streaming at once
		}

		/** Streams like play: one frame's work, nothing forced. */
		void Step(const FVector& Where, int32 Frames = 1)
		{
			Zenny->SetActorLocation(Where);
			for (int32 F = 0; F < Frames; ++F)
			{
				Grid->Advance(Where);
			}
		}

		void Repair(FName Placement, const TFunctionRef<void()>& Prepare)
		{
			AGLGlitch* Glitch = Glitches->FindByPlacement(Placement);
			if (!Glitch)
			{
				return;
			}
			const FVector At = Glitch->GetActorLocation();
			Zenny->SetActorLocation(At + FVector(0, -300, 100));
			Pehlichi->SetActorLocation(At + FVector(0, -200, 0));
			Pehlichi->GetScan()->Scan();
			Prepare();
			Glitches->EvaluateRequirements();
			Pehlichi->GetCommands()->Issue(TEXT("Command.Pehlichi.Repair"), Zenny);
			for (int32 Step = 0; Step < 120; ++Step)
			{
				Glitches->EvaluateRequirements();
				Pehlichi->GetPositioning()->Advance(0.1f);
				Pehlichi->GetRepair()->Advance(0.1f);
			}
		}

		EGLGlitchState StateOf(FName Placement) const
		{
			const AGLGlitch* G = Glitches->FindByPlacement(Placement);
			return G ? G->GetGlitch()->GetState() : EGLGlitchState::Latent;
		}

		int32 GlitchActors() const { int32 N = 0; for (TActorIterator<AGLGlitch> It(Test.World); It; ++It) { N += IsValid(*It) ? 1 : 0; } return N; }
		int32 CreatureActors() const { int32 N = 0; for (TActorIterator<AGLCreature> It(Test.World); It; ++It) { N += IsValid(*It) ? 1 : 0; } return N; }
		int32 SalvageActors() const { int32 N = 0; for (TActorIterator<AGLSalvageNode> It(Test.World); It; ++It) { N += IsValid(*It) ? 1 : 0; } return N; }
		int32 Count(const TCHAR* Prefix) const { return Events.FilterByPredicate([Prefix](FName E) { return E.ToString().StartsWith(Prefix); }).Num(); }
	};

	FGLPlacedPiece GPiece(FName Def, FVector At, int32 Yaw = 0) { return { 0, Def, At, Yaw }; }
}

using namespace GLGridTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGridTorture, "Gridlands.Game.Grid.StreamingNeverLosesDuplicatesOrReplays", GLTestUtils::Flags)
bool FGLGridTorture::RunTest(const FString& Parameters)
{
	FGridScene S(TEXT("GLGridTorture"));
	TestTrue(TEXT("start: the origin is loaded"), S.Grid->IsLoaded(GOrigin));
	TestFalse(TEXT("and the diner lots are not"), S.Grid->IsLoaded(GLots));

	// --- Change cell A (origin): repair the lamp (salvaging its blocker), defeat the gremlin.
	S.Repair(GLamp, [&S]
	{
		AGLSalvageNode* Node = S.Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindSalvageNode(GBlocker);
		while (Node && !Node->GetSalvageable()->IsSalvaged())
		{
			Node->GetSalvageable()->Interact(S.Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
		}
	});
	TestEqual(TEXT("A: lamp repaired"), S.StateOf(GLamp), EGLGlitchState::Repaired);
	S.Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindCreature(GGremlin)->GetHealth()->ApplyDamage(1000.0, S.Zenny);

	// --- At the boundary both cells are loaded: build right at the edge, terraform across it.
	S.GoTo(GAtBoundary);
	TestTrue(TEXT("boundary: both cells loaded"), S.Grid->IsLoaded(GOrigin) && S.Grid->IsLoaded(GLots));
	S.Inventory->AddItem(TEXT("item.material.timber_plank"), 10);
	const FVector PieceAt(50900, 600, S.Terrain->HeightAt(FVector2D(50900, 600)));
	TestTrue(TEXT("A: a floor 2 m from the boundary"), S.Building->Place(S.Zenny, GPiece(TEXT("buildpiece.modern.timber_foundation"), PieceAt)).IsAllowed());
	TestTrue(TEXT("A: and a wall on it"), S.Building->Place(S.Zenny, GPiece(TEXT("buildpiece.modern.timber_wall"), PieceAt + FVector(0, 100, 30))).IsAllowed());
	FGLTerrainEdit Across;
	Across.Op = EGLTerrainOp::Raise;
	Across.Centre = FVector2D(51200, -1500);
	Across.RadiusCm = 250.0;
	Across.AmountCm = 120.0;
	TestTrue(TEXT("a mound raised across the cell edge"), S.Terrain->ApplyEdit(Across).bApplied);
	const int32 PiecesInA = S.Building->PiecesOfCell(GOrigin).Num();
	TestEqual(TEXT("A: the pieces belong to the origin cell"), PiecesInA, 2);
	const FGLHeightfield* FA = S.Terrain->FieldOf(GOrigin);
	const FGLHeightfield* FB = S.Terrain->FieldOf(GLots);
	// The shared edge: origin's last column is the lots' first column.
	const int32 Row = FMath::RoundToInt((-1500.0 - FA->GetOrigin().Y) / FA->GetSpacing());
	const int32 RowB = FMath::RoundToInt((-1500.0 - FB->GetOrigin().Y) / FB->GetSpacing());
	const float EdgeA = FA->VertexHeight(FA->GetVertsX() - 1, Row), EdgeB = FB->VertexHeight(0, RowB);
	TestTrue(FString::Printf(TEXT("no seam: both cells agree on the edge (%.0f vs %.0f cm)"), EdgeA, EdgeB), EdgeA == EdgeB && EdgeA > 50.f);
	const double MoundA = S.Terrain->HeightAt(FVector2D(51100, -1500)), MoundB = S.Terrain->HeightAt(FVector2D(51300, -1500));

	// --- Change cell B: repair the jukebox, dig a hole.
	S.GoTo(FVector(102400, 0, 100));
	S.Repair(GJukebox, [] {});
	TestEqual(TEXT("B: jukebox repaired"), S.StateOf(GJukebox), EGLGlitchState::Repaired);
	FGLTerrainEdit Hole;
	Hole.Op = EGLTerrainOp::Dig;
	Hole.Centre = FVector2D(102800, 2000);
	Hole.RadiusCm = 200.0;
	Hole.AmountCm = 100.0;
	TestTrue(TEXT("B: a hole"), S.Terrain->ApplyEdit(Hole).bApplied);
	const double HoleDepth = S.Terrain->HeightAt(FVector2D(102800, 2000));

	const int32 EventsBefore = S.Events.Num(), LinesBefore = S.Lines;

	// --- Cross back and forth, many times. Memory must not grow with every round trip (it once did,
	// until the kernel killed the process: destroyed chunk actors held their meshes until GC).
	double MemoryAfterFirstRound = 0.0;
	for (int32 Round = 0; Round < 5; ++Round)
	{
		// The game collects garbage periodically (every 60 s by default); a bare test world never
		// does, so do it here the way the engine would between crossings.
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		if (Round == 1)
		{
			MemoryAfterFirstRound = FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0);
		}
		S.GoTo(GDeepInLots);
		TestFalse(FString::Printf(TEXT("round %d: the origin streamed out"), Round), S.Grid->IsLoaded(GOrigin));
		TestNull(TEXT("  its glitches are gone"), S.Glitches->FindByPlacement(GLamp));
		TestEqual(TEXT("  its pieces are gone"), S.Building->PiecesOfCell(GOrigin).Num(), 0);
		TestFalse(TEXT("  its ground is gone"), S.Terrain->HasCell(GOrigin));
		TestEqual(TEXT("  no origin creature remains"), S.CreatureActors(), 0);
		S.GoTo(GInOrigin);
		TestFalse(FString::Printf(TEXT("round %d: the lots streamed out"), Round), S.Grid->IsLoaded(GLots));
		TestTrue(TEXT("  the origin is back"), S.Grid->IsLoaded(GOrigin));
	}
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	const double MemoryAfterAll = FPlatformMemory::GetStats().UsedPhysical / (1024.0 * 1024.0);
	TestTrue(FString::Printf(TEXT("memory is bounded across round trips (%.0f MB after round 1, %.0f MB after round 5)"), MemoryAfterFirstRound, MemoryAfterAll),
		MemoryAfterAll - MemoryAfterFirstRound < 1500.0);
	// --- Back in A: everything as it was, exactly once.
	TestEqual(TEXT("A: lamp still repaired"), S.StateOf(GLamp), EGLGlitchState::Repaired);
	TestTrue(TEXT("A: blocker still salvaged"), S.Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindSalvageNode(GBlocker)->GetSalvageable()->IsSalvaged());
	TestTrue(TEXT("A: gremlin still defeated"), S.Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindCreature(GGremlin)->IsDefeated());
	TestEqual(TEXT("A: both pieces back"), S.Building->PiecesOfCell(GOrigin).Num(), PiecesInA);
	TestEqual(TEXT("A: the mound is back on its side"), S.Terrain->HeightAt(FVector2D(51100, -1500)), MoundA);
	int32 Glitchy = 0;
	GLContent::Get().ForEachEntry([&](const FGLContentEntry& E) { const FGLPlacementDef* P = E.Definition.GetPtr<FGLPlacementDef>(); Glitchy += P && P->Kind == TEXT("glitch") && UGLPlacementSubsystem::IsPlacementOfCell(E.Id, GOrigin) ? 1 : 0; });
	TestEqual(TEXT("no duplicate glitches after 5 round trips"), S.GlitchActors(), Glitchy);
	TestEqual(TEXT("no duplicate creatures"), S.CreatureActors(), 1);
	TestEqual(TEXT("streaming made no gameplay events"), S.Events.Num(), EventsBefore);
	TestEqual(TEXT("and no dialogue replayed"), S.Lines, LinesBefore);
	// --- And B, from the other side.
	S.GoTo(GDeepInLots);
	TestEqual(TEXT("B: jukebox still repaired"), S.StateOf(GJukebox), EGLGlitchState::Repaired);
	TestEqual(TEXT("B: the hole is still there"), S.Terrain->HeightAt(FVector2D(102800, 2000)), HoleDepth);
	S.GoTo(GAtBoundary);
	TestEqual(TEXT("B: the mound's other half is back too"), S.Terrain->HeightAt(FVector2D(51300, -1500)), MoundB);
	TestTrue(FString::Printf(TEXT("the streamer did real work (%d loads, %d unloads)"), S.Grid->GetLoadCount(), S.Grid->GetUnloadCount()), S.Grid->GetUnloadCount() >= 10);

	// An edit that would reach into a cell that is not loaded is refused (the edge must agree).
	S.GoTo(GInOrigin);
	TestFalse(TEXT("from home, the lots are not loaded"), S.Grid->IsLoaded(GLots));
	FGLTerrainEdit Blind = Across;
	Blind.Centre = FVector2D(51180, 4000);
	const double Before = S.Terrain->HeightAt(FVector2D(51100, 4000));
	TestFalse(TEXT("an edit reaching into an unloaded cell is refused"), S.Terrain->ApplyEdit(Blind).bApplied);
	TestEqual(TEXT("and changes nothing on this side either"), S.Terrain->HeightAt(FVector2D(51100, 4000)), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGridSaveInB, "Gridlands.Game.Grid.SaveInTheSecondCellAndRestart", GLTestUtils::Flags)
bool FGLGridSaveInB::RunTest(const FString& Parameters)
{
	double HoleDepth = 0.0;
	{
		FGridScene S(TEXT("GLGridSaveA"));
		S.Repair(GLamp, [&S]
		{
			AGLSalvageNode* Node = S.Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindSalvageNode(GBlocker);
			while (Node && !Node->GetSalvageable()->IsSalvaged())
			{
				Node->GetSalvageable()->Interact(S.Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
			}
		});
		S.GoTo(FVector(102400, 0, 100));
		S.Repair(GJukebox, [] {});
		FGLTerrainEdit Hole;
		Hole.Op = EGLTerrainOp::Dig;
		Hole.Centre = FVector2D(102800, 2000);
		Hole.RadiusCm = 200.0;
		Hole.AmountCm = 100.0;
		S.Terrain->ApplyEdit(Hole);
		HoleDepth = S.Terrain->HeightAt(FVector2D(102800, 2000));
		S.GoTo(GDeepInLots);
		TestFalse(TEXT("saving while the origin is streamed out"), S.Grid->IsLoaded(GOrigin));
		TestTrue(TEXT("saved"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(GGridSlot));
	}
	FGridScene R(TEXT("GLGridSaveB"));
	// A fresh start begins in the origin; loading puts Zenny back in the lots.
	R.GoTo(FVector(0, -1200, 100));
	TArray<FString> Problems;
	TestTrue(TEXT("loaded"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(GGridSlot, &Problems));
	TestEqual(TEXT("no problems"), Problems.Num(), 0);
	TestTrue(TEXT("Zenny is back in the lots"), FVector::Dist2D(R.Zenny->GetActorLocation(), GDeepInLots) < 10.0);
	R.GoTo(R.Zenny->GetActorLocation());
	TestTrue(TEXT("the lots stream in"), R.Grid->IsLoaded(GLots));
	TestEqual(TEXT("with the jukebox repaired"), R.StateOf(GJukebox), EGLGlitchState::Repaired);
	TestEqual(TEXT("and the hole"), R.Terrain->HeightAt(FVector2D(102800, 2000)), HoleDepth);
	R.GoTo(GInOrigin);
	TestEqual(TEXT("walking home: the origin's lamp is repaired"), R.StateOf(GLamp), EGLGlitchState::Repaired);
	TestTrue(TEXT("and its blocker salvaged"), R.Test.World->GetSubsystem<UGLPlacementSubsystem>()->FindSalvageNode(GBlocker)->GetSalvageable()->IsSalvaged());
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(GGridSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGridDepth, "Gridlands.Game.Grid.StaticDepthAndEraAreIndependentPerCell", GLTestUtils::Flags)
bool FGLGridDepth::RunTest(const FString& Parameters)
{
	FGridScene S(TEXT("GLGridDepth"));
	const FGLCellDef* A = GLContent::Get().Find<FGLCellDef>(GOrigin);
	const FGLCellDef* B = GLContent::Get().Find<FGLCellDef>(GLots);
	const FGLBandDef* BandA = GLContent::Get().Find<FGLBandDef>(A->Band);
	const FGLBandDef* BandB = GLContent::Get().Find<FGLBandDef>(B->Band);
	TestTrue(TEXT("deeper band"), BandB->Depth > BandA->Depth);
	TestTrue(TEXT("different memory: the lots are mostly 1950s"), B->EraComposition[0].Era == FName(TEXT("era.memory.fifties")) && A->EraComposition[0].Era == FName(TEXT("era.memory.modern_day")));
	UGLStabilitySubsystem* Stability = S.Test.World->GetSubsystem<UGLStabilitySubsystem>();
	const FVector QuietB(102400 - 8000, 8000, 0); // in the lots, away from the jukebox's influence
	TestTrue(TEXT("baseline = band + the cell's own offset (independent data)"),
		FMath::IsNearlyEqual(Stability->BaselineAt(QuietB), BandB->BaselineInterference + B->InterferenceOffset, 1e-9));
	S.GoTo(FVector(102400, 0, 100));
	const FVector AtJukebox = S.Glitches->FindByPlacement(GJukebox)->GetActorLocation();
	const double HomeStatic = Stability->InterferenceAt(FVector(-3000, -3000, 0));
	const double LotsStatic = Stability->InterferenceAt(AtJukebox);
	TestTrue(FString::Printf(TEXT("crossing into the lots, static rises (%.2f -> %.2f)"), HomeStatic, LotsStatic), LotsStatic > HomeStatic + 0.3);
	const double ComposureBefore = Stability->NiceComposure();
	S.Repair(GJukebox, [] {});
	const double LotsAfter = Stability->InterferenceAt(AtJukebox);
	TestTrue(FString::Printf(TEXT("repairing its glitch calms it (%.2f -> %.2f)"), LotsStatic, LotsAfter), LotsAfter < LotsStatic);
	TestTrue(TEXT("without a lock: nothing stopped Zenny going there"), S.Grid->IsLoaded(GLots));
	// NICE's composure counts the whole Grid, including a cell that is streamed out.
	const double ComposureAfter = Stability->NiceComposure();
	S.GoTo(GInOrigin);
	TestFalse(TEXT("the lots stream out"), S.Grid->IsLoaded(GLots));
	TestTrue(FString::Printf(TEXT("and NICE still counts that repair (%.3f -> %.3f -> %.3f)"), ComposureBefore, ComposureAfter, Stability->NiceComposure()),
		ComposureAfter < ComposureBefore && FMath::IsNearlyEqual(Stability->NiceComposure(), ComposureAfter, 1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGridReversal, "Gridlands.Game.Grid.RapidReversalMidLoadKeepsEverything", GLTestUtils::Flags)
bool FGLGridReversal::RunTest(const FString& Parameters)
{
	// Race: a cell with kept state starts loading, gets its ground edited and the game saved while
	// its runtime layer has not come in yet, then Zenny turns back and it is cancelled mid-load.
	FGridScene S(TEXT("GLGridReversal"));
	S.GoTo(FVector(102400, 0, 100));
	S.Repair(GJukebox, [] {});
	FGLTerrainEdit Hole;
	Hole.Op = EGLTerrainOp::Dig;
	Hole.Centre = FVector2D(102800, 2000);
	Hole.RadiusCm = 200.0;
	Hole.AmountCm = 100.0;
	S.Terrain->ApplyEdit(Hole);
	const double HoleDepth = S.Terrain->HeightAt(FVector2D(102800, 2000));
	S.GoTo(GInOrigin);
	TestFalse(TEXT("home: the lots are stowed"), S.Grid->IsLoaded(GLots));

	// Walk up to the boundary: the lots start loading, but their authored level is slow to arrive.
	S.Grid->bHoldLevels = true;
	S.Step(GAtBoundary, 3);
	TestTrue(TEXT("the lots are loading"), S.Grid->IsLoaded(GLots));
	TestFalse(TEXT("but their runtime layer is not in"), S.Grid->IsRuntimeReady(GLots));
	const int32 FirstEpoch = S.Grid->GetEpoch(GLots);
	S.Terrain->EnsureReadyAt(FVector2D(51400, -3000)); // Zenny steps over the line: ground there, now
	FGLTerrainEdit Mid;
	Mid.Op = EGLTerrainOp::Raise;
	Mid.Centre = FVector2D(51500, -3000);
	Mid.RadiusCm = 200.0;
	Mid.AmountCm = 80.0;
	TestTrue(TEXT("an edit on the half-loaded cell's ground"), S.Terrain->ApplyEdit(Mid).bApplied);
	const double MidHeight = S.Terrain->HeightAt(FVector2D(51500, -3000));
	TestTrue(TEXT("saving mid-load"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(GGridSlot));

	// Turn back before it finishes.
	S.Step(GInOrigin, 2);
	TestFalse(TEXT("reversed: the lots load was cancelled"), S.Grid->IsLoaded(GLots));
	S.Grid->bHoldLevels = false;

	// Come back properly.
	S.GoTo(FVector(102400, 0, 100));
	TestTrue(TEXT("a new load (new epoch)"), S.Grid->GetEpoch(GLots) > FirstEpoch);
	TestEqual(TEXT("the jukebox is still repaired (a cancelled load never overwrote the kept state)"), S.StateOf(GJukebox), EGLGlitchState::Repaired);
	TestEqual(TEXT("the old hole is still there"), S.Terrain->HeightAt(FVector2D(102800, 2000)), HoleDepth);
	TestEqual(TEXT("and the edit made mid-load survived"), S.Terrain->HeightAt(FVector2D(51500, -3000)), MidHeight);
	int32 Jukeboxes = 0;
	for (TActorIterator<AGLGlitch> It(S.Test.World); It; ++It)
	{
		Jukeboxes += IsValid(*It) && It->GetGlitch()->GetPlacementId() == GJukebox ? 1 : 0;
	}
	TestEqual(TEXT("exactly one jukebox"), Jukeboxes, 1);

	// And the save made mid-load restores everything too.
	FGridScene R(TEXT("GLGridReversalRestart"));
	TestTrue(TEXT("restart"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(GGridSlot));
	R.GoTo(FVector(102400, 0, 100));
	TestEqual(TEXT("restart: jukebox repaired"), R.StateOf(GJukebox), EGLGlitchState::Repaired);
	TestEqual(TEXT("restart: the mid-load edit"), R.Terrain->HeightAt(FVector2D(51500, -3000)), MidHeight);
	TestEqual(TEXT("restart: the hole"), R.Terrain->HeightAt(FVector2D(102800, 2000)), HoleDepth);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(GGridSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLGridStale, "Gridlands.Game.Grid.StaleAsyncWorkNeverLandsAndZennyNeverFalls", GLTestUtils::Flags)
bool FGLGridStale::RunTest(const FString& Parameters)
{
	FGridScene S(TEXT("GLGridStale"));
	// Teleport straight into the middle of a cell nobody has loaded: ground under Zenny at once.
	const FVector Far(110000, 9000, 100);
	S.Step(Far, 1);
	FHitResult Hit;
	FCollisionQueryParams NotZenny(TEXT("GLGroundProbe"), false, S.Zenny);
	NotZenny.AddIgnoredActor(S.Pehlichi);
	const bool bGround = S.Test.World->LineTraceSingleByChannel(Hit, FVector(Far.X, Far.Y, 50000), FVector(Far.X, Far.Y, -50000), ECC_WorldStatic, NotZenny);
	AddInfo(FString::Printf(TEXT("first frame: trace hit %d at z %.1f (%s), field %.1f, lots ground %d, chunks %d"), bGround ? 1 : 0, Hit.ImpactPoint.Z,
		*GetNameSafe(Hit.GetActor()), S.Terrain->HeightAt(FVector2D(Far)), S.Terrain->HasCell(GLots) ? 1 : 0, S.Terrain->NumChunks()));
	TestTrue(TEXT("never over missing ground: a trace under Zenny hits terrain on the first frame"), bGround && FMath::IsNearlyEqual(Hit.ImpactPoint.Z, S.Terrain->HeightAt(FVector2D(Far)), 5.0));
	TestFalse(TEXT("while the rest of the cell is still streaming"), S.Terrain->IsCellComplete(GLots));

	// Mesh jobs in flight; edit a chunk whose job is running: its result is stale and must not land.
	S.Step(Far, 3);
	TestTrue(TEXT("chunk jobs are in flight"), S.Terrain->NumJobs() > 0);
	FGLTerrainEdit Edit;
	Edit.Op = EGLTerrainOp::Raise;
	Edit.Centre = FVector2D(Far.X + 7000, Far.Y); // a nearby chunk, not the one already built
	Edit.RadiusCm = 250.0;
	Edit.AmountCm = 150.0;
	TestTrue(TEXT("edit while its chunk is being built"), S.Terrain->ApplyEdit(Edit).bApplied);
	for (int32 F = 0; F < 400 && !S.Terrain->IsCellComplete(GLots); ++F)
	{
		S.Step(Far, 1);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("the cell completes by streaming alone"), S.Terrain->IsCellComplete(GLots));
	const bool bEdited = S.Test.World->LineTraceSingleByChannel(Hit, FVector(Edit.Centre, 50000), FVector(Edit.Centre, -50000), ECC_WorldStatic, NotZenny);
	TestTrue(FString::Printf(TEXT("the ground shows the edit, not the stale mesh (trace %.0f, field %.0f)"), Hit.ImpactPoint.Z, S.Terrain->HeightAt(Edit.Centre)),
		bEdited && FMath::IsNearlyEqual(Hit.ImpactPoint.Z, S.Terrain->HeightAt(Edit.Centre), 5.0));

	// Unload and reload quickly while jobs run: nothing from the old load survives or duplicates.
	S.Step(GInOrigin, 1);
	TestFalse(TEXT("unloaded"), S.Grid->IsLoaded(GLots));
	S.Step(Far, 2);
	S.Grid->FlushAll();
	int32 Live = 0, LeftoverShowing = 0;
	for (TActorIterator<AGLTerrainChunk> It(S.Test.World); It; ++It)
	{
		if (IsValid(*It) && It->IsBuilt())
		{
			Live += It->IsHidden() ? 0 : 1;
			LeftoverShowing += It->IsHidden() ? 1 : 0;
		}
	}
	TestEqual(TEXT("exactly one cell's chunks are live"), Live, 256);
	TestEqual(TEXT("no chunk from the old load still holds a mesh"), LeftoverShowing, 0);
	TestTrue(FString::Printf(TEXT("pooled chunk actors were reused (%d)"), S.Terrain->GetStats().ChunksReused), S.Terrain->GetStats().ChunksReused > 0);
	AddInfo(FString::Printf(TEXT("stale results dropped: %d, emergency chunks: %d, emergency fields: %d"),
		S.Terrain->GetStats().StaleDropped, S.Terrain->GetStats().EmergencyChunks, S.Terrain->GetStats().EmergencyFields));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
