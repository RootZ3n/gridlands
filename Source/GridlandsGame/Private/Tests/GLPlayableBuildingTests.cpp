// P12 (ADR-0040) playable building: build mode through its public intents, with every claim the UI makes checked against
// the commit: what is shown is what happens.

#include "Building/GLBuildCatalog.h"
#include "Building/GLBuildModeComponent.h"
#include "Building/GLBuildText.h"
#include "Building/GLBuildingSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Dialogue/GLDialogueDirector.h"
#include "GameFramework/Character.h"
#include "HAL/FileManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Misc/Paths.h"
#include "Save/GLSaveSubsystem.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLPlayableTests
{
	const FName PBFoundation(TEXT("buildpiece.modern.timber_foundation"));
	const FName PBWall(TEXT("buildpiece.modern.timber_wall"));
	const FName PBWindowWall(TEXT("buildpiece.modern.window_wall"));
	const FName PBPost(TEXT("buildpiece.modern.porch_post"));
	const FName PBUpper(TEXT("buildpiece.modern.upper_floor"));
	const FName PBStair(TEXT("buildpiece.modern.timber_stair"));
	const FName PBCrate(TEXT("buildpiece.modern.storage_crate"));
	const FName PBCore(TEXT("buildpiece.modern.base_core"));
	const FName PBColumn(TEXT("buildpiece.roman.column"));
	const FName PBLogWall(TEXT("buildpiece.modern.log_wall"));
	const FName PBStud(TEXT("item.component.stud"));
	const FName PBPlank(TEXT("item.material.timber_plank"));
	const FName PBClapboard(TEXT("finish.victorian.clapboard"));
	const FName PBBoards(TEXT("finish.modern.timber_board_wall"));
	const FName PBBoardsWindow(TEXT("finish.modern.timber_board_window"));
	const FString PBSlot(TEXT("automation-test-playable-building"));

	FString PBProfile(const TCHAR* Name) { return FPaths::ProjectSavedDir() / TEXT("Automation") / FString::Printf(TEXT("build-profile-%s.json"), Name); }

	/** A flat 64 m field and Zenny as a pawn with build mode (a pawn nobody controls aims from its eyes). */
	struct FPBScene
	{
		GLTestUtils::FTestWorld Test;
		ACharacter* Zenny = nullptr;
		UGLInventoryComponent* Inventory = nullptr;
		UGLBuildModeComponent* Build = nullptr;
		UGLBuildingSubsystem* Building = nullptr;
		UGLStructureSubsystem* Structures = nullptr;

		explicit FPBScene(const TCHAR* Name, bool bFreshProfile = true) : Test(Name)
		{
			UWorld* World = Test.World;
			UGLTerrainSubsystem* Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			Terrain->Setup(FVector2D(-3200, -3200), 1, 1, 65, 100.0, 0.f, 400.0, 400.0);
			Terrain->FlushAll(); // the ground collides (the aim traces it, as in the game)
			World->GetSubsystem<UGLDialogueDirector>()->bShowOnScreen = false;
			Building = World->GetSubsystem<UGLBuildingSubsystem>();
			Structures = World->GetSubsystem<UGLStructureSubsystem>();
			Zenny = World->SpawnActor<ACharacter>(FVector(0, -600, 100), FRotator::ZeroRotator);
			Inventory = NewObject<UGLInventoryComponent>(Zenny);
			Inventory->RegisterComponent();
			Build = NewObject<UGLBuildModeComponent>(Zenny);
			Build->RegisterComponent();
			if (bFreshProfile)
			{
				IFileManager::Get().Delete(*PBProfile(Name));
			}
			Build->SetProfilePath(PBProfile(Name));
			// Everything building needs to know (as the operator's starter kit teaches).
			UGLKnowledgeSubsystem* Knowledge = World->GetSubsystem<UGLKnowledgeSubsystem>();
			GLContent::Get().ForEachEntry([Knowledge](const FGLContentEntry& Entry)
			{
				if (const FGLBuildPieceDef* Def = Entry.Definition.GetPtr<FGLBuildPieceDef>())
				{
					for (const FName& K : Def->UnlockedBy) { Knowledge->Learn(K); }
				}
				if (const FGLFinishDef* Def = Entry.Definition.GetPtr<FGLFinishDef>())
				{
					for (const FName& K : Def->UnlockedBy) { Knowledge->Learn(K); }
				}
			});
		}

		/** Zenny looks at a world point (the next RefreshView / commit aims there). */
		void AimAt(const FVector& Target) const
		{
			Zenny->SetActorRotation((Target - Zenny->GetPawnViewLocation()).Rotation());
		}

		void Stand(const FVector& At) const { Zenny->SetActorLocation(At); }

		/** Places directly (scene setup, not under test). Returns the new id, 0 when refused. */
		int32 Put(FName Def, const FVector& At, int32 Yaw = 0) const
		{
			FGLPlacedPiece P;
			P.Def = Def;
			P.Location = At;
			P.YawStep = Yaw;
			return Building->Place(Zenny, P).IsAllowed() && Building->GetPieces().Num() ? Building->GetPieces().Last().Id : 0;
		}

		int32 Newest() const { return Building->GetPieces().Num() ? Building->GetPieces().Last().Id : 0; }
		/** The newest piece's def (None when there is none): null-safe for assertions. */
		FName NewestDef() const { const FGLPlacedPiece* P = Find(Newest()); return P ? P->Def : NAME_None; }
		int32 Count() const { return Building->GetPieces().Num(); }
		const FGLPlacedPiece* Find(int32 Id) const
		{
			const FGLStructurePartRuntime* Part = Structures->FindPlayerPiece(Id);
			return Part ? &Part->Piece : nullptr;
		}
		void Tick(float Seconds) const { Build->TickComponent(Seconds, LEVELTICK_All, nullptr); }
		void Enter() const
		{
			if (Build->GetMode() != EGLToolMode::Build)
			{
				Build->ToggleBuild();
			}
		}
		/** A browser selection by intents only: the category, then the piece. */
		FString Choose(FName Piece) const
		{
			FString Log;
			const TArray<FGLCatalogCategory>& Catalog = Build->GetCatalog();
			const int32 Category = Catalog.IndexOfByPredicate([Piece](const FGLCatalogCategory& C) { return C.Pieces.Contains(Piece); });
			Build->ToggleBrowser();
			Build->RefreshView();
			Log += FString::Printf(TEXT("target cat %d; open cat %d idx %d; "), Category, Build->GetView().BrowseCategory, Build->GetView().BrowseIndex);
			for (int32 Guard = 0; Guard < 64 && Build->GetView().BrowseCategory != Category; ++Guard) { Build->BrowseCategory(1); Build->RefreshView(); }
			Log += FString::Printf(TEXT("at cat %d; "), Build->GetView().BrowseCategory);
			for (int32 Guard = 0; Guard < 64 && Build->PiecesOf(Category).IsValidIndex(Build->GetView().BrowseIndex)
				&& Build->PiecesOf(Category)[Build->GetView().BrowseIndex] != Piece; ++Guard) { Build->BrowseMove(1); Build->RefreshView(); }
			Log += FString::Printf(TEXT("at idx %d (%s); "), Build->GetView().BrowseIndex, Build->PiecesOf(Category).IsValidIndex(Build->GetView().BrowseIndex)
				? *Build->PiecesOf(Category)[Build->GetView().BrowseIndex].ToString() : TEXT("?"));
			Build->BrowseSelect();
			Build->RefreshView(); // the view is computed per frame; render it as the HUD would
			Log += FString::Printf(TEXT("selected %s state %d"), *Build->GetSelectedPiece().ToString(), (int32)Build->GetView().State);
			return Log;
		}
	};

	/** A wall stack on a foundation's east edge: floor, then walls up to Height (GREEN, GREEN, YELLOW, then RED). */
	TArray<int32> PBWallStack(FPBScene& S, const FVector& At, int32 Height)
	{
		TArray<int32> Ids;
		Ids.Add(S.Put(PBFoundation, At));
		for (int32 I = 0; I < Height; ++I)
		{
			Ids.Add(S.Put(PBWall, At + FVector(100, 0, 30 + 250 * I), GLStructureRules::QuarterTurnSteps));
		}
		return Ids;
	}
}

using namespace GLPlayableTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBCatalog, "Gridlands.Game.PlayableBuilding.TheBrowserIsDataDrivenAndErasAreFiltersNotLocks", GLTestUtils::Flags)
bool FGLPBCatalog::RunTest(const FString& Parameters)
{
	const TArray<FGLCatalogCategory> Catalog = GLBuildCatalog::Build(GLContent::Get());
	TMap<FName, int32> Seen;
	for (int32 I = 0; I < Catalog.Num(); ++I)
	{
		TestTrue(FString::Printf(TEXT("categories in data order (%s)"), *Catalog[I].Id.ToString()), I == 0 || Catalog[I - 1].Order < Catalog[I].Order);
		for (const FName& Piece : Catalog[I].Pieces) { Seen.FindOrAdd(Piece) += 1; }
	}
	int32 Buildable = 0;
	GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLBuildPieceDef* Def = Entry.Definition.GetPtr<FGLBuildPieceDef>();
		if (Def && Def->Buildable)
		{
			++Buildable;
			TestEqual(FString::Printf(TEXT("%s is in exactly one category"), *Entry.Id.ToString()), Seen.FindRef(Entry.Id), 1);
		}
		if (Def && !Def->Buildable)
		{
			TestFalse(FString::Printf(TEXT("world-only %s is never offered"), *Entry.Id.ToString()), Seen.Contains(Entry.Id));
		}
	});
	TestEqual(TEXT("every buildable piece is offered"), Seen.Num(), Buildable);
	TestEqual(TEXT("the stair is under its explicit category"), GLBuildCatalog::CategoryOf(GLContent::Get(), PBStair), FName(TEXT("buildcategory.building.vertical_access")));
	TestEqual(TEXT("a stud wall falls back by its role"), GLBuildCatalog::CategoryOf(GLContent::Get(), PBWall), FName(TEXT("buildcategory.building.walls_openings")));
	TestEqual(TEXT("a Roman column sits with posts, by what it does"), GLBuildCatalog::CategoryOf(GLContent::Get(), PBColumn), FName(TEXT("buildcategory.building.posts_columns")));
	FPBScene S(TEXT("PBCatalog"));
	S.Enter();
	const int32 Posts = S.Build->GetCatalog().IndexOfByPredicate([](const FGLCatalogCategory& C) { return C.Id == FName(TEXT("buildcategory.building.posts_columns")); });
	S.Build->SetEraFilter(TEXT("era.memory.roman"));
	TestTrue(TEXT("an era filter narrows the list"), S.Build->PiecesOf(Posts).Num() < S.Build->GetCatalog()[Posts].Pieces.Num() && S.Build->PiecesOf(Posts).Contains(PBColumn));
	S.Build->SelectPiece(PBLogWall);
	TestEqual(TEXT("and never locks: a log wall is still chosen under a Roman filter"), S.Build->GetSelectedPiece(), PBLogWall);
	S.Build->SetEraFilter(NAME_None);
	S.Inventory->AddItem(PBStud, 100);
	S.Inventory->AddItem(PBPlank, 100);
	S.Inventory->AddItem(TEXT("item.material.timber_log"), 20);
	S.Inventory->AddItem(TEXT("item.material.cut_stone"), 20);
	// Architectural nonsense is valid when the structural rules say so: a Roman column next to a log wall's foundation.
	S.Put(PBFoundation, FVector(0, 0, 0));
	S.Build->SelectPiece(PBColumn);
	S.Stand(FVector(0, -200, 100));
	S.AimAt(FVector(0, 300, 0));
	S.Build->Primary();
	TestEqual(TEXT("a Roman column beside a modern foundation is placed"), S.NewestDef(), PBColumn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBBrowser, "Gridlands.Game.PlayableBuilding.BrowserSelectionCommitsTheIntendedPiece", GLTestUtils::Flags)
bool FGLPBBrowser::RunTest(const FString& Parameters)
{
	FPBScene S(TEXT("PBBrowser"));
	S.Inventory->AddItem(PBStud, 100);
	S.Inventory->AddItem(PBPlank, 100);
	S.Enter();
	S.Choose(PBPost);
	TestEqual(TEXT("the browser returns to placing"), S.Build->GetView().State, EGLBuildState::Place);
	TestEqual(TEXT("with the chosen piece"), S.Build->GetSelectedPiece(), PBPost);
	S.AimAt(FVector(0, 0, 0));
	const int32 Before = S.Count();
	S.Build->ToggleBrowser();
	S.Build->Primary(); // in the browser the primary action selects; it never places
	TestEqual(TEXT("LMB in the browser placed nothing"), S.Count(), Before);
	S.Build->RefreshView();
	TestEqual(TEXT("it chose and returned to placing"), S.Build->GetView().State, EGLBuildState::Place);
	S.Choose(PBPost);
	S.Build->Primary();
	TestEqual(TEXT("one piece placed"), S.Count(), Before + 1);
	TestEqual(TEXT("exactly the chosen variant"), S.NewestDef(), PBPost);
	// The wheel: the next variant in the same category is the one committed, and back again.
	const TArray<FName> Variants = S.Build->PiecesOf(S.Build->GetCatalog().IndexOfByPredicate([](const FGLCatalogCategory& C) { return C.Pieces.Contains(PBPost); }));
	const int32 At = Variants.IndexOfByKey(PBPost);
	if (!TestTrue(TEXT("the posts category has variants"), Variants.Num() > 2 && At != INDEX_NONE))
	{
		return true;
	}
	S.Inventory->AddItem(TEXT("item.material.cut_stone"), 20);
	S.Build->CycleVariant(-1);
	const FName Previous = Variants[(At - 1 + Variants.Num()) % Variants.Num()];
	TestEqual(TEXT("the wheel selects the previous variant in its category"), S.Build->GetSelectedPiece(), Previous);
	S.AimAt(FVector(-200, 0, 0));
	S.Build->Primary();
	TestEqual(TEXT("and that variant is the one committed"), S.NewestDef(), Previous);
	if (S.NewestDef() != Previous)
	{
		AddInfo(S.Build->StatusLine());
	}
	S.Build->CycleVariant(1);
	TestEqual(TEXT("and forward again"), S.Build->GetSelectedPiece(), PBPost);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBCost, "Gridlands.Game.PlayableBuilding.CostAndSourceShownAreTheCommitsConsumption", GLTestUtils::Flags)
bool FGLPBCost::RunTest(const FString& Parameters)
{
	FPBScene S(TEXT("PBCost"));
	S.Inventory->AddItem(PBPlank, 40);
	S.Inventory->AddItem(PBStud, 20);
	S.Stand(FVector(-300, -400, 100));
	const int32 Core = S.Put(PBCore, FVector(-800, -800, 0));
	S.Put(PBFoundation, FVector(400, -600, 0));
	const int32 Crate = S.Put(PBCrate, FVector(400, -600, 30));
	const int32 FloorId = S.Put(PBFoundation, FVector(0, 0, 0));
	if (!TestTrue(TEXT("a base, a crate on a floor, a floor to build on"), Core && Crate && FloorId))
	{
		return false;
	}
	S.Building->StorageOf(Crate)->Add(GLContent::Get(), PBStud, 4);
	const int32 HeldBefore = S.Inventory->CountOf(PBStud);
	S.Enter();
	S.Build->SelectPiece(PBWall);
	S.Build->RotateQuarter(1);
	S.AimAt(FVector(85, 0, 30)); // the floor's top, just inside its east edge
	S.Build->RefreshView();
	const FGLBuildView View = S.Build->GetView();
	if (!TestTrue(TEXT("a wall candidate on the floor's edge"), View.bHasCandidate && View.Cost.Lines.Num() == 1))
	{
		return false;
	}
	const FGLCostLine Line = View.Cost.Lines[0];
	TestTrue(TEXT("shown: 6 studs, 4 in base storage, the rest yours"), Line.Needed == 6 && Line.InStorage == 4 && Line.Personal == HeldBefore);
	TestTrue(TEXT("shown: base storage first (4), then you (2)"), Line.FromStorage == 4 && Line.FromPersonal == 2 && View.Cost.bAffordable);
	TestTrue(TEXT("shown from this base"), !View.Cost.Claim.IsNone() && View.Cost.Containers.Contains(Crate));
	S.Build->Primary();
	TestEqual(TEXT("placed"), S.NewestDef(), PBWall);
	TestEqual(TEXT("the crate gave exactly what was shown"), 4 - S.Building->StorageOf(Crate)->CountOf(PBStud), Line.FromStorage);
	TestEqual(TEXT("Zenny gave exactly what was shown"), HeldBefore - S.Inventory->CountOf(PBStud), Line.FromPersonal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBSnap, "Gridlands.Game.PlayableBuilding.SnapMarkerAndYawShownAreWhatIsCommitted", GLTestUtils::Flags)
bool FGLPBSnap::RunTest(const FString& Parameters)
{
	FPBScene S(TEXT("PBSnap"));
	S.Inventory->AddItem(PBStud, 100);
	S.Inventory->AddItem(PBPlank, 100);
	const int32 FloorId = S.Put(PBFoundation, FVector(0, 0, 0));
	S.Enter();
	S.Build->SelectPiece(PBWall);
	S.Build->RotateQuarter(1);
	S.AimAt(FVector(85, 10, 30));
	S.Build->RefreshView();
	const FGLBuildView View = S.Build->GetView();
	if (!TestTrue(TEXT("snapped to the floor"), View.bHasCandidate && View.Snap.bSnapped && View.Snap.TargetPieceId == FloorId))
	{
		return false;
	}
	S.Build->Primary();
	const FGLPlacedPiece* PlacedWall = S.Find(S.Newest());
	if (!TestTrue(TEXT("the wall placed"), PlacedWall && PlacedWall->Def == PBWall))
	{
		return false;
	}
	TestTrue(TEXT("where the ghost was"), PlacedWall->Location.Equals(View.Candidate.Location, 0.01) && PlacedWall->YawStep == View.Candidate.YawStep);
	const FGLBuildPieceDef* Def = GLContent::Get().Find<FGLBuildPieceDef>(PBWall);
	const FGLWorldSocket* Own = GLStructureRules::Sockets(*Def, *PlacedWall).FindByPredicate([&View](const FGLWorldSocket& Socket) { return Socket.Name == View.Snap.OwnSocket; });
	TestTrue(TEXT("its marked socket meets the marked target socket"), Own && Own->Location.Equals(View.Snap.TargetLocation, 0.5));
	const FGLBuildPieceDef* FloorDef = GLContent::Get().Find<FGLBuildPieceDef>(PBFoundation);
	TestTrue(TEXT("and the marked target socket is the floor's"), GLStructureRules::Sockets(*FloorDef, *S.Find(FloorId)).ContainsByPredicate([&View](const FGLWorldSocket& Socket)
		{ return Socket.Name == View.Snap.TargetSocket && Socket.Location.Equals(View.Snap.TargetLocation, 0.01); }));
	// Yaw: 90 + 15 - 2.5 - 90 = 12.5 degrees, shown and committed (free placement on the ground).
	S.Build->SelectPiece(PBPost);
	S.Build->RotateQuarter(1);
	S.Build->Rotate15();
	S.Build->RotateFine(-1);
	S.Build->RotateQuarter(-1);
	S.Build->RotateQuarter(1); // the wall's quarter turn from before is still held (persistent rotation)
	S.Build->RotateQuarter(-1);
	S.Build->RotateQuarter(-1); // Shift+Z: -90 (an odd number of quarter turns, so +90 cannot pass for it)
	S.AimAt(FVector(-400, -300, 0));
	S.Build->RefreshView();
	const int32 Shown = S.Build->GetView().YawStep;
	TestEqual(TEXT("the yaw shown is 12.5 degrees (90 held + 15 - 2.5 - 90)"), GLStructureRules::YawDegrees(Shown), 12.5);
	S.Build->Primary();
	if (!TestEqual(TEXT("a post placed"), S.NewestDef(), PBPost))
	{
		return false;
	}
	TestEqual(TEXT("and committed"), S.Find(S.Newest())->YawStep, Shown);
	TestEqual(TEXT("the ghost's yaw is the committed yaw"), S.Find(S.Newest())->YawStep, S.Build->GetView().Candidate.YawStep);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBReasons, "Gridlands.Game.PlayableBuilding.StructuralStateAndReasonAreTheCanonicalCheck", GLTestUtils::Flags)
bool FGLPBReasons::RunTest(const FString& Parameters)
{
	FPBScene S(TEXT("PBReasons"));
	S.Inventory->AddItem(PBStud, 100);
	S.Inventory->AddItem(PBPlank, 100);
	const TArray<int32> Stack = PBWallStack(S, FVector(0, 0, 0), 2);
	S.Stand(FVector(0, -350, 100));
	S.Enter();
	S.Build->SelectPiece(PBWall);
	S.Build->RotateQuarter(1);
	auto Shown = [&S](const FVector& Aim)
	{
		S.AimAt(Aim);
		S.Build->RefreshView();
		return S.Build->GetView();
	};
	auto Agrees = [this, &S](const TCHAR* What, const FGLBuildView& View)
	{
		const FGLBuildCheck Canon = S.Building->Check(S.Zenny, View.Candidate);
		TestTrue(FString::Printf(TEXT("%s: the state shown is the canonical check"), What), Canon.Refusal == View.Check.Refusal && Canon.Preview == View.Check.Preview);
		const FGLStructurePartRuntime* Blocking = S.Structures->FindPlayerPiece(Canon.BlockingPieceId);
		const FGLBuildPieceDef* BlockingDef = Blocking ? GLContent::Get().Find<FGLBuildPieceDef>(Blocking->Piece.Def) : nullptr;
		TestEqual(FString::Printf(TEXT("%s: the reason is generated from it"), What), View.Reason, GLBuildText::Explain(Canon, BlockingDef ? BlockingDef->DisplayName.ToLower() : FString()));
	};
	const FGLBuildView Yellow = Shown(FVector(100, 0, 30 + 500 + 20));
	TestTrue(TEXT("the third wall: LIMIT, a word and an icon too"), Yellow.bHasCandidate && Yellow.Check.Preview == EGLPreview::Yellow
		&& GLBuildText::StateWord(Yellow.Check.Preview) == TEXT("LIMIT") && GLBuildText::StateIcon(Yellow.Check.Preview) == TEXT("[!]"));
	Agrees(TEXT("LIMIT"), Yellow);
	S.Build->Primary();
	const FGLBuildView Red = Shown(FVector(100, 0, 30 + 750 + 20));
	TestTrue(TEXT("the fourth: NO, nothing holds it up"), Red.bHasCandidate && Red.Check.Refusal == EGLBuildRefusal::Unsupported && Red.Reason == TEXT("Nothing holds it up"));
	Agrees(TEXT("Unsupported"), Red);
	TestEqual(TEXT("the commit refuses for the reason shown"), S.Building->Place(S.Zenny, Red.Candidate).Refusal, Red.Check.Refusal);
	// Overlap: a foundation laid over a post (far from the post's socket): the blocking piece is named.
	S.Stand(FVector(-1200, -500, 100));
	const int32 Post = S.Put(PBPost, FVector(-1200, 0, 0));
	S.Build->SelectPiece(PBFoundation);
	S.Build->RotateQuarter(-1);
	const FGLBuildView Overlap = Shown(FVector(-1240, 30, 0));
	TestTrue(TEXT("NO: the post is in the way, by its id"), Overlap.bHasCandidate && Overlap.Check.Refusal == EGLBuildRefusal::Overlaps && Overlap.Check.BlockingPieceId == Post);
	TestTrue(TEXT("and by name"), Overlap.Reason.Contains(GLContent::Get().Find<FGLBuildPieceDef>(PBPost)->DisplayName.ToLower()));
	Agrees(TEXT("Overlaps"), Overlap);
	// Missing items: the shortfall shown is the check's.
	S.Inventory->RemoveItem(PBPlank, S.Inventory->CountOf(PBPlank));
	S.Stand(FVector(1500, 1000, 100));
	const FGLBuildView Short = Shown(FVector(1500, 1500, 0));
	TestTrue(TEXT("NO: needs planks"), Short.bHasCandidate && Short.Check.Refusal == EGLBuildRefusal::MissingItems && Short.Check.MissingItem == PBPlank && Short.Check.MissingNeeded == 2);
	TestTrue(TEXT("the reason names the item and the numbers"), Short.Reason.Contains(GLBuildText::ItemName(PBPlank)) && Short.Reason.Contains(TEXT("2")));
	Agrees(TEXT("MissingItems"), Short);
	TestEqual(TEXT("the commit refuses for the reason shown"), S.Building->Place(S.Zenny, Short.Candidate).Refusal, Short.Check.Refusal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBRemoval, "Gridlands.Game.PlayableBuilding.RemovalPreviewAndConfirmationFollowTheCanonicalPrediction", GLTestUtils::Flags)
bool FGLPBRemoval::RunTest(const FString& Parameters)
{
	FPBScene S(TEXT("PBRemoval"));
	S.Inventory->AddItem(PBStud, 100);
	S.Inventory->AddItem(PBPlank, 100);
	const TArray<int32> Stack = PBWallStack(S, FVector(0, 0, 0), 3);
	S.Enter();
	S.Build->ToggleRemoveMode();
	S.AimAt(FVector(100, 0, 30 + 125)); // the lowest wall
	S.Build->RefreshView();
	FGLBuildView View = S.Build->GetView();
	TestEqual(TEXT("the target is the lowest wall"), View.RemoveTarget, Stack[1]);
	TestEqual(TEXT("the prediction shown is the canonical one"), View.Predicted, S.Building->PreviewRemoval(Stack[1]));
	TestTrue(TEXT("the two walls above come down"), View.Predicted.Contains(Stack[2]) && View.Predicted.Contains(Stack[3]) && View.bNeedsConfirm);
	S.Build->Primary();
	S.Tick(0.2f);
	TestNotNull(TEXT("a press is not a confirmation: it still stands"), S.Find(Stack[1]));
	S.Build->PrimaryReleased();
	S.Tick(0.5f);
	TestNotNull(TEXT("released early: nothing happens"), S.Find(Stack[1]));
	S.Build->Primary();
	S.AimAt(FVector(100, 0, 30 + 375)); // the target changes mid-hold
	S.Tick(0.5f);
	TestTrue(TEXT("a hold never transfers to another piece"), S.Find(Stack[1]) && S.Find(Stack[2]) && S.Structures->FindPlayerPiece(Stack[2])->State == EGLStructurePartState::Intact);
	S.Build->PrimaryReleased();
	S.AimAt(FVector(100, 0, 30 + 125));
	S.Build->RefreshView();
	S.Build->Primary();
	S.Tick(0.25f);
	S.Tick(0.25f);
	TestNull(TEXT("held: the lowest wall is removed"), S.Find(Stack[1]) && S.Structures->FindPlayerPiece(Stack[1])->State == EGLStructurePartState::Intact ? S.Find(Stack[1]) : nullptr);
	TestTrue(TEXT("and exactly the predicted walls came down"), S.Structures->FindPlayerPiece(Stack[2]) && S.Structures->FindPlayerPiece(Stack[2])->State == EGLStructurePartState::Debris
		&& S.Structures->FindPlayerPiece(Stack[3]) && S.Structures->FindPlayerPiece(Stack[3])->State == EGLStructurePartState::Debris);
	// A safe removal (nothing else falls) is one click, whatever the piece: a base core with nothing on it.
	S.Stand(FVector(-1600, 1100, 100));
	const int32 Core = S.Put(PBCore, FVector(-1600, 1600, 0));
	S.AimAt(FVector(-1600, 1600, 40));
	S.Build->RefreshView();
	TestTrue(TEXT("the base core: nothing else falls, no confirmation"), S.Build->GetView().RemoveTarget == Core && !S.Build->GetView().bNeedsConfirm);
	S.Build->Primary();
	TestNull(TEXT("removed with one click"), S.Find(Core));
	// The toggle setting: a first press arms, a second press on the same piece and prediction confirms.
	const TArray<int32> Second = PBWallStack(S, FVector(1600, 1600, 0), 2);
	S.Build->SetConfirmMode(EGLHoldMode::Toggle);
	S.Stand(FVector(1600, 1000, 100));
	S.AimAt(FVector(1700, 1600, 30 + 125));
	S.Build->RefreshView();
	S.Build->Primary();
	S.Tick(1.f);
	TestNotNull(TEXT("toggle: armed, not removed"), S.Find(Second[1]));
	S.Build->Primary();
	TestTrue(TEXT("toggle: the second press removes it"), !S.Find(Second[1]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBSalvage, "Gridlands.Game.PlayableBuilding.SalvagePreviewIsTheRecovery", GLTestUtils::Flags)
bool FGLPBSalvage::RunTest(const FString& Parameters)
{
	FPBScene S(TEXT("PBSalvage"));
	S.Inventory->AddItem(PBStud, 100);
	S.Inventory->AddItem(PBPlank, 100);
	const TArray<int32> A = PBWallStack(S, FVector(0, 0, 0), 1), B = PBWallStack(S, FVector(600, 0, 0), 1);
	S.Stand(FVector(350, -450, 100));
	S.Enter();
	S.Build->ToggleRemoveMode();
	auto Recovered = [&S](const TMap<FName, int32>& Before)
	{
		TMap<FName, int32> Delta;
		for (const FName& Item : { PBStud, PBPlank, FName(TEXT("item.material.scrap_timber")) })
		{
			if (const int32 D = S.Inventory->CountOf(Item) - Before.FindRef(Item)) { Delta.Add(Item, D); }
		}
		return Delta;
	};
	auto Snapshot = [&S]() { return TMap<FName, int32>{ { PBStud, S.Inventory->CountOf(PBStud) }, { PBPlank, S.Inventory->CountOf(PBPlank) },
		{ FName(TEXT("item.material.scrap_timber")), S.Inventory->CountOf(TEXT("item.material.scrap_timber")) } }; };
	S.AimAt(FVector(100, 0, 150));
	S.Build->RefreshView();
	const FGLBuildView Careful = S.Build->GetView();
	TestTrue(TEXT("both paths' yields are shown"), Careful.YieldCareful.Num() > 0 && Careful.YieldDestructive.Num() > 0 && Careful.YieldCareful.FindRef(PBStud) > Careful.YieldDestructive.FindRef(PBStud));
	TMap<FName, int32> Before = Snapshot();
	S.Build->Primary();
	TestEqual(TEXT("careful: recovered exactly what was shown"), GLBuildText::Items(Recovered(Before)), GLBuildText::Items(Careful.YieldCareful));
	S.Build->TogglePath();
	S.AimAt(FVector(700, 0, 150));
	S.Build->RefreshView();
	const FGLBuildView Smash = S.Build->GetView();
	Before = Snapshot();
	S.Build->Primary();
	TestEqual(TEXT("smash: recovered exactly what was shown"), GLBuildText::Items(Recovered(Before)), GLBuildText::Items(Smash.YieldDestructive));
	TestEqual(TEXT("and the current path's yield is that one"), GLBuildText::Items(Smash.Yield), GLBuildText::Items(Smash.YieldDestructive));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBFinish, "Gridlands.Game.PlayableBuilding.TheChosenFinishIsTheOneInstalledAndRemembered", GLTestUtils::Flags)
bool FGLPBFinish::RunTest(const FString& Parameters)
{
	FPBScene S(TEXT("PBFinish"));
	S.Inventory->AddItem(PBStud, 100);
	S.Inventory->AddItem(PBPlank, 100);
	const TArray<int32> A = PBWallStack(S, FVector(0, 0, 0), 1), B = PBWallStack(S, FVector(600, 0, 0), 1);
	if (!TestTrue(TEXT("two framed walls"), A[1] && B[1]))
	{
		return false;
	}
	S.Stand(FVector(350, -450, 100));
	S.Enter();
	S.Build->ToggleFinishMode();
	S.AimAt(FVector(100, 0, 150));
	S.Build->RefreshView();
	TestTrue(TEXT("the frame's finishes are offered (not only the first)"), S.Build->GetView().FinishChoices.Contains(PBClapboard) && S.Build->GetView().FinishChoices.Contains(PBBoards));
	TestFalse(TEXT("a window-wall finish is not offered on a solid wall"), S.Build->GetView().FinishChoices.Contains(PBBoardsWindow));
	for (int32 Guard = 0; Guard < 8 && S.Build->GetView().Finish != PBClapboard; ++Guard)
	{
		S.Build->CycleFinish(1);
	}
	const FGLCostView Cost = S.Build->GetView().FinishCost;
	const int32 Planks = S.Inventory->CountOf(PBPlank);
	S.Build->Primary();
	TestTrue(TEXT("the chosen finish is installed"), S.Find(A[1])->Layers.Contains(PBClapboard) && !S.Find(A[1])->Layers.Contains(PBBoards));
	TestEqual(TEXT("presented finished"), S.Building->ShownPhaseOf(A[1]), FName(TEXT("finish")));
	TestEqual(TEXT("it cost what was shown"), Planks - S.Inventory->CountOf(PBPlank), Cost.Lines.Num() ? Cost.Lines[0].FromPersonal + Cost.Lines[0].FromStorage : -1);
	S.AimAt(FVector(700, 0, 150));
	S.Build->RefreshView();
	TestEqual(TEXT("remembered for walls: the next wall offers clapboard first"), S.Build->GetView().Finish, PBClapboard);
	S.Build->Primary();
	TestTrue(TEXT("repeated finishing: one click"), S.Find(B[1])->Layers.Contains(PBClapboard));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBClaim, "Gridlands.Game.PlayableBuilding.TheClaimShownIsTheCanonicalClaim", GLTestUtils::Flags)
bool FGLPBClaim::RunTest(const FString& Parameters)
{
	FPBScene S(TEXT("PBClaim"));
	S.Inventory->AddItem(PBStud, 100);
	S.Inventory->AddItem(PBPlank, 100);
	S.Put(PBCore, FVector(0, 0, 0));
	S.Enter();
	S.Build->RefreshView();
	const TArray<FGLClaim> Claims = S.Building->Claims();
	const FGLBuildView& View = S.Build->GetView();
	if (!TestTrue(TEXT("inside: the claim is shown"), Claims.Num() == 1 && View.bShowClaim && View.bInsideClaim && View.ClaimAreas.Num() == Claims[0].Areas.Num()))
	{
		return false;
	}
	TestTrue(TEXT("its areas are the canonical claim's"), View.ClaimAreas[0].Centre.Equals(Claims[0].Areas[0].Centre, 0.01) && FMath::IsNearlyEqual(View.ClaimAreas[0].RadiusCm, Claims[0].Areas[0].RadiusCm));
	S.Stand(FVector(Claims[0].Areas[0].RadiusCm + 2000.0, 0, 100));
	S.Build->RefreshView();
	TestFalse(TEXT("far outside: not shown"), S.Build->GetView().bShowClaim);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBProfile, "Gridlands.Game.PlayableBuilding.FavoritesPersistAndRecentsAreDeterministic", GLTestUtils::Flags)
bool FGLPBProfile::RunTest(const FString& Parameters)
{
	{
		FPBScene S(TEXT("PBProfile"));
		S.Inventory->AddItem(PBStud, 100);
		S.Inventory->AddItem(PBPlank, 100);
		S.Enter();
		S.Build->SelectPiece(PBStair);
		S.Build->PinFavorite(2);
		S.Build->SelectPiece(PBFoundation);
		S.AimAt(FVector(0, 0, 0));
		S.Build->Primary();
		S.Build->SelectPiece(PBPost);
		S.AimAt(FVector(-400, -200, 0));
		S.Build->Primary();
		S.Build->SelectPiece(PBFoundation);
		S.AimAt(FVector(400, -100, 0));
		S.Build->Primary();
		TestEqual(TEXT("recents: most recent first, no repeats"), S.Build->GetRecents(), TArray<FName>{ PBFoundation, PBPost });
		S.Build->SelectFavorite(2);
		TestEqual(TEXT("a favorite selects its piece"), S.Build->GetSelectedPiece(), PBStair);
	}
	FPBScene R(TEXT("PBProfile"), /*bFreshProfile=*/false);
	TestEqual(TEXT("the favorite persists"), R.Build->GetFavorites()[2], PBStair);
	TestEqual(TEXT("so do the recents, in order"), R.Build->GetRecents(), TArray<FName>{ PBFoundation, PBPost });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBModes, "Gridlands.Game.PlayableBuilding.ModesNeverCommitTheWrongActionAndTheUIIsNotAuthority", GLTestUtils::Flags)
bool FGLPBModes::RunTest(const FString& Parameters)
{
	FPBScene S(TEXT("PBModes"));
	S.Inventory->AddItem(PBStud, 100);
	S.Inventory->AddItem(PBPlank, 100);
	const TArray<int32> Stack = PBWallStack(S, FVector(0, 0, 0), 2);
	S.Enter();
	S.Build->SelectPiece(PBPost);
	const int32 Count = S.Count();
	// REMOVE aimed at the ground: nothing; FINISH aimed at the ground: nothing.
	S.Build->ToggleRemoveMode();
	S.AimAt(FVector(-1500, 1500, 0));
	S.Build->Primary();
	S.Build->ToggleFinishMode();
	S.Build->Primary();
	TestEqual(TEXT("no placement from REMOVE or FINISH"), S.Count(), Count);
	// A hold in REMOVE is cancelled by leaving REMOVE: the action never completes in another mode.
	S.Build->ToggleRemoveMode();
	S.AimAt(FVector(100, 0, 155));
	S.Build->RefreshView();
	TestTrue(TEXT("(the lowest wall would bring the one above down)"), S.Build->GetView().bNeedsConfirm);
	S.Build->Primary();
	S.Build->ToggleRemoveMode(); // back to PLACE mid-hold
	S.Tick(0.6f);
	TestTrue(TEXT("leaving REMOVE cancelled the hold"), S.Find(Stack[1]) != nullptr);
	S.Build->Primary(); // PLACE: a post, at the wall the aim still points to? refused or placed, never a removal
	TestTrue(TEXT("and PLACE never removes"), S.Find(Stack[1]) != nullptr);
	// The UI is not authority: the commit re-aims and re-checks at its own moment.
	S.Stand(FVector(-1100, -700, 100));
	S.AimAt(FVector(-900, -300, 0));
	S.Build->RefreshView();
	const FVector Shown = S.Build->GetView().Candidate.Location;
	S.AimAt(FVector(-1300, 0, 0)); // aimed elsewhere before the click; the stale view is never used
	S.Build->Primary();
	if (!TestEqual(TEXT("a post placed at the click"), S.NewestDef(), PBPost))
	{
		return false;
	}
	TestTrue(TEXT("placed where Zenny aims at the click, not where an old view said"), !S.Find(S.Newest())->Location.Equals(Shown, 1.0)
		&& FVector::Dist2D(S.Find(S.Newest())->Location, FVector(-1300, 0, 0)) < 50.0);
	S.AimAt(FVector(-900, -300, 0));
	S.Build->RefreshView();
	TestTrue(TEXT("(affordable when shown)"), S.Build->GetView().Check.IsAllowed());
	S.Inventory->RemoveItem(PBStud, S.Inventory->CountOf(PBStud));
	const int32 Now = S.Count();
	S.Build->Primary();
	TestEqual(TEXT("materials gone after the view: the commit refuses"), S.Count(), Now);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBWindow, "Gridlands.Game.PlayableBuilding.AWindowWallIsAnOrdinaryComponentWithAnOpening", GLTestUtils::Flags)
bool FGLPBWindow::RunTest(const FString& Parameters)
{
	FString Fingerprint;
	int32 Window = 0;
	{
		FPBScene S(TEXT("PBWindow"));
		S.Inventory->AddItem(PBStud, 100);
		S.Inventory->AddItem(PBPlank, 100);
		const int32 FloorId = S.Put(PBFoundation, FVector(0, 0, 0));
		S.Enter();
		S.Build->SelectPiece(PBWindowWall);
		S.Build->RotateQuarter(1);
		S.AimAt(FVector(85, 0, 30));
		S.Build->Primary();
		Window = S.Newest();
		if (!TestTrue(TEXT("placed on the floor's edge like a wall"), FloorId && Window && S.Find(Window)->Def == PBWindowWall))
		{
			return false;
		}
		TestEqual(TEXT("as its frame"), S.Building->ShownPhaseOf(Window), FName(TEXT("frame")));
		TestTrue(TEXT("it stands by the ordinary rules"), S.Building->Support().FindRef(Window) > 0.0);
		// The opening is real: a trace through it passes, a trace through the wall below the sill hits the wall.
		auto Hits = [&S, Window](double Z)
		{
			FHitResult Hit;
			const bool bHit = S.Test.World->LineTraceSingleByChannel(Hit, FVector(-200, 0, Z), FVector(400, 0, Z), ECC_Visibility);
			return bHit && S.Building->PieceIdAt(Hit) == Window;
		};
		TestFalse(TEXT("through the window opening: nothing"), Hits(30 + 150));
		TestTrue(TEXT("below the sill: the wall"), Hits(30 + 50));
		S.Build->ToggleFinishMode();
		S.AimAt(FVector(100, 50, 30 + 50));
		S.Build->RefreshView();
		TestTrue(TEXT("its finishes: clapboard and window boards, not the solid wall's boards"), S.Build->GetView().FinishChoices.Contains(PBClapboard)
			&& S.Build->GetView().FinishChoices.Contains(PBBoardsWindow) && !S.Build->GetView().FinishChoices.Contains(PBBoards));
		for (int32 Guard = 0; Guard < 8 && S.Build->GetView().Finish != PBClapboard; ++Guard) { S.Build->CycleFinish(1); }
		S.Build->Primary();
		TestTrue(TEXT("finished in clapboard"), S.Find(Window)->Layers.Contains(PBClapboard));
		TestFalse(TEXT("finished, the opening is still open"), Hits(30 + 150));
		TestEqual(TEXT("removing its floor brings it down (ordinary support)"), S.Building->PreviewRemoval(FloorId), TArray<int32>{ Window });
		Fingerprint = FString::Printf(TEXT("%s %s %d %s"), *S.Find(Window)->Def.ToString(), *S.Find(Window)->Location.ToString(), S.Find(Window)->YawStep,
			*FString::JoinBy(S.Find(Window)->Layers, TEXT(","), [](FName L) { return L.ToString(); }));
		TestTrue(TEXT("saved"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(PBSlot));
	}
	FPBScene R(TEXT("PBWindowRestart"));
	TArray<FString> Problems;
	TestTrue(TEXT("restart loads"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(PBSlot, &Problems));
	R.Structures->PumpPresentation(FVector::ZeroVector, 0.0);
	const FGLPlacedPiece* Back = R.Find(Window);
	TestTrue(TEXT("the window wall is back exactly, finish and all"), Back && FString::Printf(TEXT("%s %s %d %s"), *Back->Def.ToString(), *Back->Location.ToString(), Back->YawStep,
		*FString::JoinBy(Back->Layers, TEXT(","), [](FName L) { return L.ToString(); })) == Fingerprint);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(PBSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLPBStairs, "Gridlands.Game.PlayableBuilding.AStraightStairIsAnOrdinaryStructuralComponent", GLTestUtils::Flags)
bool FGLPBStairs::RunTest(const FString& Parameters)
{
	// A room's floor with stud walls east and west, an upper floor on them, and two more floors south: build mode snaps a
	// stair's head to the upper floor's south edge, its feet rest on the floors below. The same rules as any piece.
	FString Fingerprint;
	int32 Stair = 0;
	{
		FPBScene S(TEXT("PBStairs"));
		S.Inventory->AddItem(PBStud, 300);
		S.Inventory->AddItem(PBPlank, 300);
		S.Stand(FVector(0, -900, 100));
		const int32 F1 = S.Put(PBFoundation, FVector(0, 0, 0)), F2 = S.Put(PBFoundation, FVector(0, -200, 0)), F3 = S.Put(PBFoundation, FVector(0, -400, 0));
		const int32 East = S.Put(PBWall, FVector(100, 0, 30), GLStructureRules::QuarterTurnSteps), West = S.Put(PBWall, FVector(-100, 0, 30), GLStructureRules::QuarterTurnSteps);
		const int32 Upper = S.Put(PBUpper, FVector(0, 0, 280));
		if (!TestTrue(TEXT("floors, walls and an upper floor on them"), F1 && F2 && F3 && East && West && Upper && S.Building->Support().FindRef(Upper) > 0.0))
		{
			return false;
		}
		S.Enter();
		S.Build->SelectPiece(PBStair);
		S.AimAt(FVector(0, -100, 290)); // the upper floor's south edge
		S.Build->RefreshView();
		const FGLBuildView View = S.Build->GetView();
		TestTrue(TEXT("the stair's head snaps to the upper floor's edge"), View.bHasCandidate && View.Snap.bSnapped && View.Snap.TargetPieceId == Upper && View.Snap.OwnSocket == FName(TEXT("head")));
		TestTrue(TEXT("its feet land on the floors below"), View.Candidate.Location.Equals(FVector(0, -300, 30), 1.0));
		TestTrue(TEXT("GREEN"), View.Check.IsAllowed());
		S.Build->Primary();
		Stair = S.Newest();
		if (!TestEqual(TEXT("placed"), S.NewestDef(), PBStair))
		{
			return false;
		}
		TestTrue(TEXT("it stands"), S.Building->Support().FindRef(Stair) > 0.0);
		TestTrue(TEXT("instanced like any quiescent piece"), S.Structures->IsInstanced(Stair));
		TestEqual(TEXT("removing it brings nothing down (the upper floor stands on its walls)"), S.Building->PreviewRemoval(Stair), TArray<int32>());
		TestFalse(TEXT("removing one floor under it does not bring it down (the other still holds it)"), S.Building->PreviewRemoval(F3).Contains(Stair));
		// A stair up from an upper floor (a second flight): take the walls under that floor away and both come down, by the
		// canonical rules (a stair on a ground floor is on the ground: it never falls for losing that floor).
		S.Stand(FVector(1200, -900, 100));
		S.Put(PBFoundation, FVector(1200, 0, 0));
		const int32 E2 = S.Put(PBWall, FVector(1300, 0, 30), GLStructureRules::QuarterTurnSteps), W2 = S.Put(PBWall, FVector(1100, 0, 30), GLStructureRules::QuarterTurnSteps);
		const int32 U2 = S.Put(PBUpper, FVector(1200, 0, 280));
		const int32 Flight = S.Put(PBStair, FVector(1200, 0, 300));
		if (!TestTrue(TEXT("a second flight on an upper floor"), E2 && W2 && U2 && Flight))
		{
			return false;
		}
		S.Building->Dismantle(S.Zenny, E2);
		const TArray<int32> Predicted = S.Building->PreviewRemoval(W2);
		TestTrue(TEXT("removing the last wall under that floor predicts the floor and the flight fall"), Predicted.Contains(U2) && Predicted.Contains(Flight));
		S.Building->Remove(S.Zenny, W2, EGLSalvagePath::Careful);
		TestTrue(TEXT("and the flight collapses (debris)"), S.Structures->FindPlayerPiece(Flight) && S.Structures->FindPlayerPiece(Flight)->State == EGLStructurePartState::Debris);
		Fingerprint = FString::Printf(TEXT("%s %s %d"), *S.Find(Stair)->Def.ToString(), *S.Find(Stair)->Location.ToString(), S.Find(Stair)->YawStep);
		TestTrue(TEXT("saved"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(PBSlot));
	}
	FPBScene R(TEXT("PBStairsRestart"));
	TArray<FString> Problems;
	TestTrue(TEXT("restart loads"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(PBSlot, &Problems));
	R.Structures->PumpPresentation(FVector::ZeroVector, 0.0);
	TestTrue(TEXT("the stair is back exactly"), R.Find(Stair) && FString::Printf(TEXT("%s %s %d"), *R.Find(Stair)->Def.ToString(), *R.Find(Stair)->Location.ToString(), R.Find(Stair)->YawStep) == Fingerprint);
	TestTrue(TEXT("still standing"), R.Building->Support().FindRef(Stair) > 0.0);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(PBSlot));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
