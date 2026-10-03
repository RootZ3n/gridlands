// P11 (ADR-0039) Building v1 in game worlds: the WINCHESTER fixture end to end (with save/restart and a save mid-fall),
// shared base storage, claims, container collapse, refusal instead of loss, basements, the renewal gate and density.

#include "Building/GLBuildPiece.h"
#include "Building/GLBuildingSubsystem.h"
#include "Building/GLClaimRules.h"
#include "Components/SceneComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Dialogue/GLDialogueDirector.h"
#include "Events/GLEventSubsystem.h"
#include "Fabrication/GLFabricatorComponent.h"
#include "HAL/FileManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Save/GLSaveSubsystem.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLWinchesterHouse.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLBuildingV1GameTests
{
	const FString V1Slot = TEXT("automation-test-building-v1");
	const FName V1GFloor(TEXT("buildpiece.modern.timber_foundation"));
	const FName V1GWall(TEXT("buildpiece.modern.timber_wall"));
	const FName V1GCrate(TEXT("buildpiece.modern.storage_crate"));
	const FName V1GCore(TEXT("buildpiece.modern.base_core"));
	const FName V1GStud(TEXT("item.component.stud"));
	const FName V1GPlank(TEXT("item.material.timber_plank"));
	const FName V1GScrap(TEXT("item.material.scrap_timber"));

	/** A flat 64 m field, the builder Zenny (inventory, fabricator), the building subsystems. */
	struct FV1Scene
	{
		GLTestUtils::FTestWorld Test;
		AActor* Zenny = nullptr;
		UGLInventoryComponent* Inventory = nullptr;
		UGLBuildingSubsystem* Building = nullptr;
		UGLStructureSubsystem* Structures = nullptr;
		UGLTerrainSubsystem* Terrain = nullptr;
		TArray<FName> Events;

		explicit FV1Scene(const TCHAR* Name) : Test(Name)
		{
			UWorld* World = Test.World;
			Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			Terrain->Setup(FVector2D(-3200, -3200), 1, 1, 65, 100.0, 0.f, 400.0, 400.0);
			Building = World->GetSubsystem<UGLBuildingSubsystem>();
			Structures = World->GetSubsystem<UGLStructureSubsystem>();
			Zenny = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Zenny);
			Zenny->SetRootComponent(Root);
			Root->RegisterComponent();
			Inventory = NewObject<UGLInventoryComponent>(Zenny);
			Inventory->RegisterComponent();
			NewObject<UGLFabricatorComponent>(Zenny)->RegisterComponent();
			World->GetSubsystem<UGLKnowledgeSubsystem>()->Learn(TEXT("knowledge.style.modern_timber_frame"));
			World->GetSubsystem<UGLDialogueDirector>()->bShowOnScreen = false;
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event) { Events.Add(Event.Tag.GetTagName()); }));
		}

		void PresentAll() const { Structures->PumpPresentation(FVector::ZeroVector, 0.0); }
		int32 Place(FName Def, const FVector& At, int32 Yaw = 0) const
		{
			FGLPlacedPiece P;
			P.Def = Def;
			P.Location = At;
			P.YawStep = Yaw;
			if (!Building->Place(Zenny, P).IsAllowed())
			{
				return 0;
			}
			const TArray<FGLPlacedPiece> All = Building->GetPieces();
			return All.Num() ? All.Last().Id : 0;
		}
	};

	FString V1Failures(const FGLWinchesterHouse& House)
	{
		FString Out;
		for (const FGLWinchesterCheck& Check : House.Checks)
		{
			if (!Check.bPass)
			{
				Out += FString::Printf(TEXT("[%s: %s] "), *Check.Name, *Check.Detail);
			}
		}
		return Out;
	}
}

using namespace GLBuildingV1GameTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Winchester, "Gridlands.Game.BuildingV1.WinchesterHouseFrameFinishCollapseAndPersistence", GLTestUtils::Flags)
bool FGLV1Winchester::RunTest(const FString& Parameters)
{
	FGLWinchesterHouse House;
	House.Anchor = FVector(0, 0, 0);
	FString Before;
	{
		FV1Scene S(TEXT("GLV1Winchester"));
		TestTrue(TEXT("pad"), House.PreparePad(S.Test.World));
		TestTrue(TEXT("base established"), House.EstablishBase(S.Test.World, S.Zenny));
		TestTrue(TEXT("framed (with the 45 degree bay)"), House.Frame(S.Test.World, S.Zenny));
		TestTrue(TEXT("a log sawn into studs"), House.Saw(S.Test.World, S.Zenny));
		TestTrue(TEXT("finished"), House.Finish(S.Test.World, S.Zenny));
		TestEqual(TEXT("PREVIEW == REALITY for every placement"), House.PreviewMismatches, 0);
		// The bay at 45 degrees protects its oriented footprint from terraforming, not its enclosing box.
		const FGLStructurePartRuntime* Post = S.Structures->FindPlayerPiece(House.BayIds[2]);
		TestTrue(TEXT("a bay post's ground is protected"), Post && S.Building->IsUnderStructure(FVector2D(Post->Piece.Location)));
		// A point 75 cm out along a world diagonal (away from the house): outside the post's rotated footprint grown by the
		// 50 cm margin (|local| 106 > 60), inside that grown footprint's enclosing box (85): only an oriented test frees it.
		const FVector2D Corner = FVector2D(Post->Piece.Location) + FVector2D(75.0, -75.0);
		const FVector2D LocalCorner = GLStructureRules::RotateXY(Corner - FVector2D(Post->Piece.Location), -Post->Piece.YawStep);
		TestTrue(TEXT("(that point is outside the grown rotated footprint)"), FMath::Max(FMath::Abs(LocalCorner.X), FMath::Abs(LocalCorner.Y)) > 60.0);
		TestFalse(TEXT("the corner of its enclosing box is not protected (oriented footprint)"), S.Building->IsUnderStructure(Corner) || S.Structures->IsUnderStructure(Corner, 50.0));
		Before = House.Fingerprint(S.Test.World);
		TestTrue(TEXT("saved"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(V1Slot));
	}
	{
		FV1Scene R(TEXT("GLV1WinchesterRestart"));
		TArray<FString> Problems;
		TestTrue(TEXT("restart loads"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(V1Slot, &Problems));
		TestEqual(TEXT("no load problems"), FString::Join(Problems, TEXT("; ")), FString());
		R.PresentAll();
		TestEqual(TEXT("every piece, yaw, layer, content and owner is back exactly"), House.Fingerprint(R.Test.World), Before);
		const AGLBuildPiece* BayWall = R.Building->FindActor(House.BayIds[1]);
		TestEqual(TEXT("the bay wall is presented finished"), BayWall ? BayWall->ShownPhase() : FName(), FName(TEXT("finish")));
		TestEqual(TEXT("one claim again (derived from the core)"), R.Building->Claims().Num(), 1);
		// Structural manipulation: the porch.
		TestTrue(TEXT("porch post out, then the Roman column: the predicted roof falls"), House.PorchCollapse(R.Test.World, R.Zenny));
		TestEqual(TEXT("its fall is in flight"), R.Structures->ActiveCollapses(), 1);
		TestTrue(TEXT("saved mid-fall"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(V1Slot));
	}
	FV1Scene M(TEXT("GLV1WinchesterMidfall"));
	TArray<FString> Problems;
	TestTrue(TEXT("the mid-fall save loads"), M.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(V1Slot, &Problems));
	TestEqual(TEXT("no problems"), FString::Join(Problems, TEXT("; ")), FString());
	M.PresentAll();
	TestEqual(TEXT("the fall resumes (SUPPORT FAILURE != IMPACT survives a restart)"), M.Structures->ActiveCollapses(), 1);
	M.Structures->Advance(6.0);
	TestEqual(TEXT("it lands exactly once"), M.Structures->ImpactCount(), 1);
	M.Structures->Advance(2.0);
	TestEqual(TEXT("and never again"), M.Structures->ImpactCount(), 1);
	const int32 PorchRoofId = House.Ids.FindRef(TEXT("porch_roof"));
	AGLBuildPiece* Debris = M.Building->FindActor(PorchRoofId);
	AGLStructurePart* DebrisPart = Cast<AGLStructurePart>(Debris);
	TestTrue(TEXT("the porch roof lies as debris"), DebrisPart && M.Structures->FindPlayerPiece(PorchRoofId)->State == EGLStructurePartState::Debris);
	const int32 StudsBefore = M.Inventory->CountOf(V1GStud), ScrapBefore = M.Inventory->CountOf(V1GScrap), PlanksBefore = M.Inventory->CountOf(V1GPlank);
	M.Zenny->SetActorLocation(DebrisPart ? DebrisPart->GetActorLocation() + FVector(0, 300, 0) : FVector::ZeroVector);
	int32 Hits = 0;
	while (DebrisPart && !DebrisPart->GetSalvageable()->IsSalvaged() && Hits++ < 50)
	{
		DebrisPart->GetSalvageable()->Interact(M.Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
	}
	TestTrue(TEXT("the collapse debris salvages"), DebrisPart && DebrisPart->GetSalvageable()->IsSalvaged());
	TestTrue(TEXT("into scrap (collapse path)"), M.Inventory->CountOf(V1GScrap) - ScrapBefore >= 4);
	TestEqual(TEXT("no pristine studs from a collapse"), M.Inventory->CountOf(V1GStud), StudsBefore);
	TestEqual(TEXT("no pristine boards either"), M.Inventory->CountOf(V1GPlank), PlanksBefore);
	TestTrue(TEXT("careful dismantle (intact studs) and a smash (degraded)"), House.Salvage(M.Test.World, M.Zenny));
	TestTrue(FString::Printf(TEXT("every WINCHESTER check holds %s"), *V1Failures(House)), House.AllPassed());
	for (const FGLWinchesterCheck& Check : House.Checks)
	{
		AddInfo(FString::Printf(TEXT("%s %s: %s"), Check.bPass ? TEXT("PASS") : TEXT("FAIL"), *Check.Name, *Check.Detail));
	}
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(V1Slot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Storage, "Gridlands.Game.BuildingV1.BaseStorageClaimsAndContainersNeverLoseAnything", GLTestUtils::Flags)
bool FGLV1Storage::RunTest(const FString& Parameters)
{
	FV1Scene S(TEXT("GLV1Storage"));
	S.Inventory->AddItem(V1GPlank, 30);
	S.Zenny->SetActorLocation(FVector(-300, 0, 100));
	const int32 Core = S.Place(V1GCore, FVector(0, 0, 0));
	const int32 NearFloor = S.Place(V1GFloor, FVector(400, 0, 0));
	const int32 FarFloor = S.Place(V1GFloor, FVector(1200, 0, 0));
	const int32 Near = S.Place(V1GCrate, FVector(400, 0, 30));
	const int32 Far = S.Place(V1GCrate, FVector(1200, 0, 30));
	TestTrue(TEXT("a base core, two floors and two crates"), Core && NearFloor && FarFloor && Near && Far);
	TestEqual(TEXT("one claim"), S.Building->Claims().Num(), 1);
	TestFalse(TEXT("P11: a second base core whose claim overlaps is refused"), S.Place(V1GCore, FVector(2000, 0, 0)) != 0);
	S.Building->StorageOf(Near)->Add(GLContent::Get(), V1GStud, 4);
	S.Building->StorageOf(Far)->Add(GLContent::Get(), V1GStud, 20);
	S.Inventory->AddItem(V1GStud, 10);
	// A wall framed near the near crate: 6 studs, base storage first (nearest crate first), Zenny's own untouched.
	S.Place(V1GFloor, FVector(400, 600, 0));
	const int32 FramedWall = S.Place(V1GWall, FVector(400, 700, 30));
	TestTrue(TEXT("a wall is framed from base storage"), FramedWall != 0);
	TestEqual(TEXT("the nearest crate first"), S.Building->StorageOf(Near)->CountOf(V1GStud), 0);
	TestEqual(TEXT("then the next"), S.Building->StorageOf(Far)->CountOf(V1GStud), 18);
	TestEqual(TEXT("Zenny's own studs untouched"), S.Inventory->CountOf(V1GStud), 10);
	const FGLMaterialSources Sources = S.Building->SourcesFor(S.Zenny, FVector(400, 700, 30));
	TestEqual(TEXT("documented order: crates by distance, then Zenny"), Sources.Containers, TArray<int32>({ Near, Far }));
	// Outside the claim, storage is not magically connected.
	S.Zenny->SetActorLocation(FVector(20000, 0, 100));
	TestEqual(TEXT("away from the base, only Zenny's inventory"), S.Building->SourcesFor(S.Zenny, FVector(20000, 0, 0)).Containers.Num(), 0);
	S.Zenny->SetActorLocation(FVector(-300, 0, 100));
	// A non-empty crate cannot be dismantled; its floor can, and then the crate collapses with its contents.
	TestEqual(TEXT("a non-empty crate refuses careful dismantling"), S.Building->Dismantle(S.Zenny, Far).Refusal, EGLDemolishRefusal::StorageNotEmpty);
	TestEqual(TEXT("removing its floor predicts the crate falls"), S.Building->PreviewRemoval(FarFloor), TArray<int32>{ Far });
	const FGLDemolishResult Under = S.Building->Dismantle(S.Zenny, FarFloor);
	TestEqual(TEXT("and it does"), Under.Collapsed, TArray<int32>{ Far });
	TestEqual(TEXT("a falling crate leaves the sources at once (support failure, not impact)"), S.Building->SourcesFor(S.Zenny, FVector(400, 700, 30)).Containers, TArray<int32>{ Near });
	const FGLStructurePartRuntime* Fallen = S.Structures->FindPlayerPiece(Far);
	TestTrue(TEXT("its contents stay with its debris"), Fallen && Fallen->Contents.CountOf(V1GStud) == 18);
	// Save and restore mid-fall: the contents come back exactly.
	TestTrue(TEXT("saved"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(V1Slot));
	FV1Scene R(TEXT("GLV1StorageRestart"));
	TArray<FString> Problems;
	TestTrue(TEXT("restart"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(V1Slot, &Problems));
	R.PresentAll();
	R.Structures->Advance(5.0);
	const FGLStructurePartRuntime* Restored = R.Structures->FindPlayerPiece(Far);
	TestTrue(TEXT("restored debris keeps its contents"), Restored && Restored->State == EGLStructurePartState::Debris && Restored->Contents.CountOf(V1GStud) == 18);
	// Salvaging it refuses rather than destroy anything when Zenny has no room, then succeeds once he has.
	AGLStructurePart* Debris = Cast<AGLStructurePart>(R.Building->FindActor(Far));
	R.Zenny->SetActorLocation(FVector(20000, 0, 100)); // outside the claim: only his own inventory takes the yield
	for (int32 Slot = 0; R.Inventory->GetInventory().GetStacks().Num() < R.Inventory->GetInventory().GetMaxSlots() && Slot < 64; ++Slot)
	{
		R.Inventory->AddItem(TEXT("item.tool.pry_bar"), 1);
	}
	const int32 StudsHeld = R.Inventory->CountOf(V1GStud);
	int32 Hits = 0;
	while (Debris && !Debris->GetSalvageable()->IsSalvaged() && Hits++ < 30)
	{
		Debris->GetSalvageable()->Interact(R.Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
	}
	TestFalse(TEXT("full: the finishing hit is refused"), Debris && Debris->GetSalvageable()->IsSalvaged());
	TestTrue(TEXT("nothing destroyed: the contents are still there"), R.Structures->FindPlayerPiece(Far) && R.Structures->FindPlayerPiece(Far)->Contents.CountOf(V1GStud) == 18);
	TestTrue(TEXT("and NICE heard about it (inventory full)"), R.Events.Contains(FName(TEXT("Event.Player.InventoryFull"))));
	while (R.Inventory->CountOf(TEXT("item.tool.pry_bar")) > 0)
	{
		R.Inventory->RemoveItem(TEXT("item.tool.pry_bar"), 1);
	}
	Hits = 0;
	while (Debris && !Debris->GetSalvageable()->IsSalvaged() && Hits++ < 30)
	{
		Debris->GetSalvageable()->Interact(R.Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
	}
	TestTrue(TEXT("with room, the debris salvages"), Debris && Debris->GetSalvageable()->IsSalvaged());
	TestEqual(TEXT("and its contents come back whole (all 18 studs)"), R.Inventory->CountOf(V1GStud), StudsHeld + 18);
	TestTrue(TEXT("with the crate's own collapse scrap"), R.Inventory->CountOf(V1GScrap) >= 3);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(V1Slot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Basement, "Gridlands.Game.BuildingV1.ABasementIsAnOpenPitThenAStructure", GLTestUtils::Flags)
bool FGLV1Basement::RunTest(const FString& Parameters)
{
	// Compatibility only (operator): DIG FIRST -> BUILD SECOND, within one height per vertex. No overhangs, no cavities.
	FV1Scene S(TEXT("GLV1Basement"));
	const FVector2D Pit(800, 800);
	const double Depth = 250.0;
	for (int32 Pass = 0; Pass < 6; ++Pass)
	{
		for (const FVector2D& Offset : { FVector2D(0, 0), FVector2D(150, 0), FVector2D(-150, 0), FVector2D(0, 150), FVector2D(0, -150) })
		{
			FGLTerrainEdit Edit;
			Edit.Op = EGLTerrainOp::Flatten;
			Edit.Centre = Pit + Offset;
			Edit.RadiusCm = 350.0;
			Edit.AmountCm = 500.0;
			Edit.TargetHeightCm = -Depth;
			S.Terrain->ApplyEdit(Edit);
		}
	}
	TestTrue(FString::Printf(TEXT("an open pit, %.0f cm deep at its floor"), -S.Terrain->HeightAt(Pit)), FMath::IsNearlyEqual(S.Terrain->HeightAt(Pit), -Depth, 2.0));
	S.Inventory->AddItem(V1GPlank, 10);
	S.Inventory->AddItem(V1GStud, 24);
	S.Zenny->SetActorLocation(FVector(Pit, 100));
	const int32 PitFloor = S.Place(V1GFloor, FVector(Pit, -Depth));
	TestTrue(TEXT("a foundation on the pit floor"), PitFloor != 0);
	const int32 Retaining = S.Place(V1GWall, FVector(Pit + FVector2D(0, 100), -Depth + 30));
	TestTrue(TEXT("a retaining/foundation wall up to grade"), Retaining != 0);
	const int32 Upper = S.Place(V1GWall, FVector(Pit + FVector2D(0, 100), -Depth + 30 + 250));
	TestTrue(TEXT("and building upward above grade"), Upper != 0);
	TestTrue(TEXT("the upper wall stands on the basement wall"), S.Building->Support().FindRef(Upper) > 0.0);
	// The ground under the basement's foundation is protected (no undermining in P11).
	FGLTerrainEdit Under;
	Under.Op = EGLTerrainOp::Dig;
	Under.Centre = Pit;
	Under.RadiusCm = 100.0;
	Under.AmountCm = 50.0;
	TestFalse(TEXT("digging under it afterwards is refused"), S.Terrain->Terraform(S.Zenny, TEXT("terraform.shovel.dig"), Pit).bApplied);
	TestTrue(TEXT("saved"), S.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(V1Slot));
	FV1Scene R(TEXT("GLV1BasementRestart"));
	TArray<FString> Problems;
	TestTrue(TEXT("restart"), R.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(V1Slot, &Problems));
	TestTrue(TEXT("the pit is still a pit"), FMath::IsNearlyEqual(R.Terrain->HeightAt(Pit), -Depth, 2.0));
	TestEqual(TEXT("and the basement stands in it"), R.Building->GetPieces().Num(), 3);
	TestTrue(TEXT("supported as before"), R.Building->Support().FindRef(Upper) > 0.0);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(V1Slot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Renewal, "Gridlands.Game.BuildingV1.WorldRenewalNeverTouchesPlayerConstruction", GLTestUtils::Flags)
bool FGLV1Renewal::RunTest(const FString& Parameters)
{
	FV1Scene S(TEXT("GLV1Renewal"));
	const FName Carport(TEXT("placement.test.renewable_carport"));
	TestTrue(TEXT("an authored structure"), S.Structures->SpawnStructure(Carport, TEXT("structure.modern.carport"), NAME_None, FVector(2000, 2000, 0), 0));
	S.Structures->FindMutable(Carport)->Parts[0].State = EGLStructurePartState::Removed; // salvaged by someone, long ago
	TestEqual(TEXT("authored, outside any claim: renewable"), S.Structures->Renew(Carport, S.Building->Claims()), UGLStructureSubsystem::ERenewal::Renewed);
	TestEqual(TEXT("and renewed whole"), S.Structures->Find(Carport)->Parts[0].State, EGLStructurePartState::Intact);
	S.Inventory->AddItem(V1GPlank, 10);
	S.Zenny->SetActorLocation(FVector(1500, 1500, 100));
	const int32 Core = S.Place(V1GCore, FVector(1500, 1500, 0));
	const int32 BaseFloor = S.Place(V1GFloor, FVector(1500, 1900, 0));
	TestTrue(TEXT("a base next to it"), Core && BaseFloor);
	TestEqual(TEXT("authored inside a claim: never renewed"), S.Structures->Renew(Carport, S.Building->Claims()), UGLStructureSubsystem::ERenewal::InsideClaim);
	const FName Mine = UGLStructureSubsystem::PlayerKey(S.Structures->FindPlayerPiece(BaseFloor)->Piece.Cell);
	TestEqual(TEXT("player construction: never renewed, claim or not"), S.Structures->Renew(Mine, {}), UGLStructureSubsystem::ERenewal::PlayerOwned);
	TestEqual(TEXT("and it is all still there"), S.Building->GetPieces().Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1Density, "Gridlands.Game.BuildingV1.ThreeHundredPiecesScaleLinearly", GLTestUtils::Flags)
bool FGLV1Density::RunTest(const FString& Parameters)
{
	// The 300-piece player base and twice that: support, a placement preview, a removal prediction, capture and restore.
	// Linear work must stay linear (the pre-P11 socket links were quadratic in pieces).
	auto Measure = [this](int32 Units, TMap<FString, double>& Out)
	{
		FV1Scene S(*FString::Printf(TEXT("GLV1Dense%d"), Units));
		const FName Cell = S.Building->CellFor(FVector(-2800, -2800, 0));
		const TArray<FGLSavedPiece> Saved = FGLWinchesterHouse::DensePlayerBaseSaved(FVector(-2800, -2800, 0), Units, [](const FVector2D&) { return 0.0; }, 1, Cell);
		double T = FPlatformTime::Seconds();
		S.Building->RestoreCell(Cell, Saved, {});
		Out.Add(TEXT("restoreMs"), (FPlatformTime::Seconds() - T) * 1000.0);
		const TArray<FGLPlacedPiece> Pieces = S.Building->GetPieces();
		Out.Add(TEXT("pieces"), Pieces.Num());
		T = FPlatformTime::Seconds();
		const TMap<int32, double> Support = GLStructureRules::ComputeSupport(GLContent::Get(), Pieces, [](const FVector2D&) { return 0.0; });
		Out.Add(TEXT("supportMs"), (FPlatformTime::Seconds() - T) * 1000.0);
		int32 Standing = 0;
		for (const TPair<int32, double>& Entry : Support) { Standing += Entry.Value > 0.0 ? 1 : 0; }
		Out.Add(TEXT("standing"), Standing);
		FGLPlacedPiece Candidate;
		Candidate.Def = V1GFloor;
		Candidate.Location = FVector(2800, 2800, 0);
		S.Inventory->AddItem(V1GPlank, 2);
		T = FPlatformTime::Seconds();
		const FGLBuildCheck Check = S.Building->Check(S.Zenny, Candidate);
		Out.Add(TEXT("previewMs"), (FPlatformTime::Seconds() - T) * 1000.0);
		T = FPlatformTime::Seconds();
		S.Building->PreviewRemoval(Pieces[0].Id);
		Out.Add(TEXT("removalPredictionMs"), (FPlatformTime::Seconds() - T) * 1000.0);
		TArray<FGLSavedPiece> Captured;
		T = FPlatformTime::Seconds();
		S.Structures->CapturePlayerCell(Cell, Captured);
		Out.Add(TEXT("captureMs"), (FPlatformTime::Seconds() - T) * 1000.0);
		TestEqual(FString::Printf(TEXT("%d units: capture keeps every piece"), Units), Captured.Num(), Saved.Num());
		TestEqual(FString::Printf(TEXT("%d units: every piece stands"), Units), Standing, Pieces.Num());
		TestTrue(FString::Printf(TEXT("%d units: the preview still answers"), Units), Check.IsAllowed());
	};
	TMap<FString, double> A, B;
	Measure(50, A);
	Measure(100, B);
	TestTrue(FString::Printf(TEXT("at least 300 pieces (%.0f)"), A[TEXT("pieces")]), A[TEXT("pieces")] >= 300);
	for (const TCHAR* Key : { TEXT("supportMs"), TEXT("previewMs"), TEXT("removalPredictionMs"), TEXT("captureMs"), TEXT("restoreMs") })
	{
		AddInfo(FString::Printf(TEXT("%s: %.0f pieces %.3f ms, %.0f pieces %.3f ms (x%.2f)"), Key, A[TEXT("pieces")], A[Key], B[TEXT("pieces")], B[Key], B[Key] / FMath::Max(1e-6, A[Key])));
		TestTrue(FString::Printf(TEXT("%s at 300 pieces stays under 10 ms (%.3f)"), Key, A[Key]), A[Key] < 10.0);
		TestTrue(FString::Printf(TEXT("%s grows roughly linearly: x%.2f for twice the pieces"), Key, B[Key] / FMath::Max(1e-6, A[Key])), B[Key] < A[Key] * 3.0 + 0.5);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLV1StoreTake, "Gridlands.Game.BuildingV1.StoringAndTakingNeverLosesAnything", GLTestUtils::Flags)
bool FGLV1StoreTake::RunTest(const FString& Parameters)
{
	// The player's own verbs on a crate (E: store materials, L: take everything): tools stay in hand, what does not fit
	// stays where it was, nothing is lost or duplicated.
	FV1Scene S(TEXT("GLV1StoreTake"));
	S.Inventory->AddItem(V1GPlank, 12);
	S.Zenny->SetActorLocation(FVector(-300, 0, 100));
	S.Place(V1GFloor, FVector(400, 0, 0));
	const int32 Crate = S.Place(V1GCrate, FVector(400, 0, 30));
	AGLStructurePart* Part = Cast<AGLStructurePart>(S.Building->FindActor(Crate));
	TestTrue(TEXT("a crate"), Part != nullptr);
	if (!Part)
	{
		return false;
	}
	S.Inventory->AddItem(V1GStud, 40);
	S.Inventory->AddItem(TEXT("item.tool.pry_bar"), 1);
	TArray<FGLInteractionOption> Options;
	Part->GetInteractionOptions(S.Zenny, Options);
	TestTrue(TEXT("the crate offers Store materials first"), Options.Num() >= 2 && Options[0].Verb == GLTestUtils::Tag(TEXT("Interact.Store")) && Options[0].bEnabled);
	const int32 Planks = S.Inventory->CountOf(V1GPlank);
	TestTrue(TEXT("store"), Part->Interact(S.Zenny, GLTestUtils::Tag(TEXT("Interact.Store"))));
	TestEqual(TEXT("every stud in the crate"), S.Building->StorageOf(Crate)->CountOf(V1GStud), 40);
	TestEqual(TEXT("and every plank"), S.Building->StorageOf(Crate)->CountOf(V1GPlank), Planks);
	TestEqual(TEXT("Zenny keeps his tools"), S.Inventory->CountOf(TEXT("item.tool.pry_bar")), 1);
	TestEqual(TEXT("and carries no materials"), S.Inventory->CountOf(V1GStud) + S.Inventory->CountOf(V1GPlank), 0);
	TestTrue(TEXT("an event the autosave hears"), S.Events.Contains(FName(TEXT("Event.Storage.Changed"))));
	// Take with almost no room: what fits comes out, the rest stays in the crate.
	for (int32 Slot = 0; S.Inventory->GetInventory().GetStacks().Num() < S.Inventory->GetInventory().GetMaxSlots() - 1 && Slot < 64; ++Slot)
	{
		S.Inventory->AddItem(TEXT("item.tool.shovel"), 1);
	}
	TestTrue(TEXT("take"), Part->Interact(S.Zenny, GLTestUtils::Tag(TEXT("Interact.Take"))));
	const int32 Out = S.Inventory->CountOf(V1GStud) + S.Inventory->CountOf(V1GPlank);
	const int32 Kept = S.Building->StorageOf(Crate)->CountOf(V1GStud) + S.Building->StorageOf(Crate)->CountOf(V1GPlank);
	TestTrue(TEXT("one slot's worth came out"), Out > 0);
	TestEqual(TEXT("nothing lost or duplicated"), Out + Kept, 40 + Planks);
	TestTrue(TEXT("the rest stayed in the crate, and NICE noticed"), Kept > 0 && S.Events.Contains(FName(TEXT("Event.Player.InventoryFull"))));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
