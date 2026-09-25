#include "Building/GLBuildPiece.h"
#include "Building/GLBuildingSubsystem.h"
#include "Components/SceneComponent.h"
#include "Dialogue/GLDialogueDirector.h"
#include "Events/GLEventSubsystem.h"
#include "HAL/FileManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"
#include "Save/GLSaveSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLBuildingTests
{
	const FName Floor(TEXT("buildpiece.modern.timber_foundation"));
	const FName Wall(TEXT("buildpiece.modern.timber_wall"));
	const FName Door(TEXT("buildpiece.modern.timber_doorway"));
	const FName Roof(TEXT("buildpiece.modern.timber_roof"));
	const FName Plank(TEXT("item.material.timber_plank"));
	const FName Soil(TEXT("item.material.soil"));
	const FString BuildSlot = TEXT("automation-test-building");

	/** A 64 m flat field with a builder who carries an inventory. */
	struct FBuildScene
	{
		GLTestUtils::FTestWorld Test;
		AActor* Zenny = nullptr;
		UGLInventoryComponent* Inventory = nullptr;
		UGLBuildingSubsystem* Building = nullptr;
		UGLTerrainSubsystem* Terrain = nullptr;
		TArray<FName> Events;

		explicit FBuildScene(const TCHAR* Name, bool bKnowsTimber = true) : Test(Name)
		{
			UWorld* World = Test.World;
			Terrain = World->GetSubsystem<UGLTerrainSubsystem>();
			Terrain->Setup(FVector2D(-3200, -3200), 1, 1, 65, 100.0, 0.f, 400.0, 400.0);
			Building = World->GetSubsystem<UGLBuildingSubsystem>();
			Zenny = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Zenny);
			Zenny->SetRootComponent(Root);
			Root->RegisterComponent();
			Inventory = NewObject<UGLInventoryComponent>(Zenny);
			Inventory->RegisterComponent();
			if (bKnowsTimber)
			{
				World->GetSubsystem<UGLKnowledgeSubsystem>()->Learn(TEXT("knowledge.style.modern_timber_frame"));
			}
			World->GetSubsystem<UGLDialogueDirector>()->bShowOnScreen = false;
			World->GetSubsystem<UGLEventSubsystem>()->Subscribe(GLTestUtils::Tag(TEXT("Event")),
				FGLGameplayEventDelegate::CreateLambda([this](const FGLGameplayEvent& Event) { Events.Add(Event.Tag.GetTagName()); }));
		}

		FGLPlacedPiece P(FName Def, FVector At, int32 Yaw = 0) const { return { 0, Def, At, Yaw }; }

		/** The same 4 m shelter as the Core test, placed through the real transaction. */
		TArray<FGLPlacedPiece> ShelterPlan() const
		{
			return {
				P(Floor, FVector(-100, -100, 0)), P(Floor, FVector(100, -100, 0)), P(Floor, FVector(-100, 100, 0)), P(Floor, FVector(100, 100, 0)),
				P(Wall, FVector(-100, -200, 30)), P(Door, FVector(100, -200, 30)), P(Wall, FVector(-100, 200, 30)), P(Wall, FVector(100, 200, 30)),
				P(Wall, FVector(-200, -100, 30), 1), P(Wall, FVector(-200, 100, 30), 1), P(Wall, FVector(200, -100, 30), 1), P(Wall, FVector(200, 100, 30), 1),
				P(Roof, FVector(-100, -100, 280)), P(Roof, FVector(100, -100, 280)), P(Roof, FVector(-100, 100, 280), 2), P(Roof, FVector(100, 100, 280), 2),
			};
		}

		int32 IdAt(FName Def, FVector At) const
		{
			const FGLPlacedPiece* Found = Building->GetPieces().FindByPredicate([&](const FGLPlacedPiece& Piece) { return Piece.Def == Def && Piece.Location.Equals(At, 1.0); });
			return Found ? Found->Id : 0;
		}

		bool Trace(const FVector2D& At, double& OutZ) const
		{
			FHitResult Hit;
			const bool bHit = Test.World->LineTraceSingleByChannel(Hit, FVector(At.X, At.Y, 2000), FVector(At.X, At.Y, -2000), ECC_WorldStatic);
			OutZ = Hit.ImpactPoint.Z;
			return bHit;
		}
	};
}

using namespace GLBuildingTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLBuildShelter, "Gridlands.Game.Building.ShelterFromInventoryPersistsAcrossReload", GLTestUtils::Flags)
bool FGLBuildShelter::RunTest(const FString& Parameters)
{
	TMap<int32, double> SupportBefore;
	{
		FBuildScene Scene(TEXT("GLBuildShelter"));
		Scene.Inventory->AddItem(Plank, 30);
		Scene.Inventory->AddItem(Plank, 2); // 32 = 16 pieces x 2
		for (const FGLPlacedPiece& Piece : Scene.ShelterPlan())
		{
			const FGLBuildCheck Result = Scene.Building->Place(Scene.Zenny, Piece);
			TestTrue(FString::Printf(TEXT("%s at %s placed (%s)"), *Piece.Def.ToString(), *Piece.Location.ToCompactString(), *Result.Reason), Result.IsAllowed());
		}
		TestEqual(TEXT("16 pieces stand"), Scene.Building->GetPieces().Num(), 16);
		TestEqual(TEXT("exactly 32 planks were spent, none duplicated or lost"), Scene.Inventory->CountOf(Plank), 0);
		double RoofZ = 0.0;
		TestTrue(TEXT("the roof is solid: a trace from the sky stops on it"), Scene.Trace(FVector2D(-100, -150), RoofZ) && RoofZ > 280.0);
		TestTrue(TEXT("Event.Building.Placed per piece"), Scene.Events.FilterByPredicate([](FName E) { return E == TEXT("Event.Building.Placed"); }).Num() == 16);
		SupportBefore = Scene.Building->Support();
		TestTrue(TEXT("saved"), Scene.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(BuildSlot));
	}
	FBuildScene Reloaded(TEXT("GLBuildShelterReloaded"));
	TestEqual(TEXT("a fresh world has no pieces"), Reloaded.Building->GetPieces().Num(), 0);
	TArray<FString> Problems;
	TestTrue(TEXT("loaded"), Reloaded.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(BuildSlot, &Problems));
	TestEqual(TEXT("no problems"), Problems.Num(), 0);
	TestEqual(TEXT("all 16 pieces are back"), Reloaded.Building->GetPieces().Num(), 16);
	TestEqual(TEXT("each with its actor"), Reloaded.Building->GetPieces().FilterByPredicate([&](const FGLPlacedPiece& Piece) { return Reloaded.Building->FindActor(Piece.Id) != nullptr; }).Num(), 16);
	TestTrue(TEXT("support is recomputed identically (derived, not saved)"), Reloaded.Building->Support().OrderIndependentCompareEqual(SupportBefore));
	double RoofZ = 0.0;
	TestTrue(TEXT("the reloaded roof is solid too"), Reloaded.Trace(FVector2D(-100, -150), RoofZ) && RoofZ > 280.0);
	TestFalse(TEXT("loading does not pay for pieces again"), Reloaded.Events.Contains(FName(TEXT("Event.Building.Placed"))));
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(BuildSlot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLBuildRefusals, "Gridlands.Game.Building.RefusalsChangeNothing", GLTestUtils::Flags)
bool FGLBuildRefusals::RunTest(const FString& Parameters)
{
	FBuildScene Unknown(TEXT("GLBuildUnknown"), false);
	Unknown.Inventory->AddItem(Plank, 10);
	TestEqual(TEXT("without the style: refused"), Unknown.Building->Place(Unknown.Zenny, Unknown.P(Floor, FVector(0, 0, 0))).Refusal, EGLBuildRefusal::NotKnown);
	TestEqual(TEXT("planks untouched"), Unknown.Inventory->CountOf(Plank), 10);

	FBuildScene Scene(TEXT("GLBuildRefusals"));
	TestEqual(TEXT("no planks: refused"), Scene.Building->Place(Scene.Zenny, Scene.P(Floor, FVector(0, 0, 0))).Refusal, EGLBuildRefusal::MissingItems);
	Scene.Inventory->AddItem(Plank, 10);
	TestEqual(TEXT("a wall on nothing: refused"), Scene.Building->Place(Scene.Zenny, Scene.P(Wall, FVector(0, 0, 30))).Refusal, EGLBuildRefusal::Unsupported);
	TestEqual(TEXT("and it cost nothing"), Scene.Inventory->CountOf(Plank), 10);
	TestEqual(TEXT("no piece exists"), Scene.Building->GetPieces().Num(), 0);
	TestTrue(TEXT("a refusal is an event (dialogue can react to failed building)"), Scene.Events.Contains(FName(TEXT("Event.Building.Refused"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLBuildDemolish, "Gridlands.Game.Building.DemolitionRefundsEverythingThatFalls", GLTestUtils::Flags)
bool FGLBuildDemolish::RunTest(const FString& Parameters)
{
	FBuildScene Scene(TEXT("GLBuildDemolish"));
	Scene.Inventory->AddItem(Plank, 30);
	Scene.Inventory->AddItem(Plank, 2);
	for (const FGLPlacedPiece& Piece : Scene.ShelterPlan())
	{
		Scene.Building->Place(Scene.Zenny, Piece);
	}
	TestEqual(TEXT("all planks used"), Scene.Inventory->CountOf(Plank), 0);
	const int32 WallUnderRoof = Scene.IdAt(Wall, FVector(-100, -200, 30));
	const int32 RoofAbove = Scene.IdAt(Roof, FVector(-100, -100, 280));
	const FGLDemolishResult Result = Scene.Building->Demolish(Scene.Zenny, WallUnderRoof);
	TestTrue(TEXT("demolished"), Result.IsDone());
	TestEqual(TEXT("the wall and the roof slope it held"), Result.Removed, TArray<int32>{ WallUnderRoof, RoofAbove });
	TestEqual(TEXT("both refunded in full"), Scene.Inventory->CountOf(Plank), 4);
	TestNull(TEXT("the roof's actor is gone"), Scene.Building->FindActor(RoofAbove));
	TestTrue(TEXT("the collapse is announced"), Scene.Events.Contains(FName(TEXT("Event.Building.Collapsed"))));
	TestEqual(TEXT("14 pieces remain"), Scene.Building->GetPieces().Num(), 14);

	// No room for the refund: nothing happens (no silent loss).
	FBuildScene Full(TEXT("GLBuildDemolishFull"));
	Full.Inventory->AddItem(Plank, 2);
	Full.Building->Place(Full.Zenny, Full.P(Floor, FVector(0, 0, 0)));
	for (int32 Slot = 0; Full.Inventory->GetInventory().GetStacks().Num() < Full.Inventory->GetInventory().GetMaxSlots() && Slot < 64; ++Slot)
	{
		Full.Inventory->AddItem(TEXT("item.tool.pry_bar"), 1); // stack size 1: one slot each
	}
	const int32 FloorId = Full.Building->GetPieces()[0].Id;
	TestEqual(TEXT("refused when the planks would not fit"), Full.Building->Demolish(Full.Zenny, FloorId).Refusal, EGLDemolishRefusal::NoRoomForRefund);
	TestEqual(TEXT("the floor still stands"), Full.Building->GetPieces().Num(), 1);
	TestNotNull(TEXT("with its actor"), Full.Building->FindActor(FloorId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLTerraformPlay, "Gridlands.Game.Terrain.TerraformConservesProtectsAndPersists", GLTestUtils::Flags)
bool FGLTerraformPlay::RunTest(const FString& Parameters)
{
	const FName Dig(TEXT("terraform.shovel.dig")), Raise(TEXT("terraform.shovel.raise")), Flatten(TEXT("terraform.shovel.flatten"));
	{
		FBuildScene Scene(TEXT("GLTerraform"));
		TestFalse(TEXT("no shovel: no digging"), Scene.Terrain->Terraform(Scene.Zenny, Dig, FVector2D(1000, 0)).bApplied);
		TestEqual(TEXT("and no soil"), Scene.Inventory->CountOf(Soil), 0);
		Scene.Inventory->AddItem(TEXT("item.tool.shovel"), 1);
		TestFalse(TEXT("raising needs soil"), Scene.Terrain->Terraform(Scene.Zenny, Raise, FVector2D(1000, 0)).bApplied);

		TestTrue(TEXT("dig"), Scene.Terrain->Terraform(Scene.Zenny, Dig, FVector2D(1000, 0)).bApplied);
		TestEqual(TEXT("digging yields soil"), Scene.Inventory->CountOf(Soil), 1);
		double Z = 0.0;
		TestTrue(TEXT("collision follows the dig: a trace lands in the hole"), Scene.Trace(FVector2D(1000, 0), Z) && FMath::IsNearlyEqual(Z, -50.0, 2.0));
		TestTrue(TEXT("raise elsewhere"), Scene.Terrain->Terraform(Scene.Zenny, Raise, FVector2D(-1000, 0)).bApplied);
		TestEqual(TEXT("raising spends the soil"), Scene.Inventory->CountOf(Soil), 0);
		TestTrue(TEXT("collision follows the mound"), Scene.Trace(FVector2D(-1000, 0), Z) && FMath::IsNearlyEqual(Z, 50.0, 2.0));
		// Two more strokes each way: a 1.5 m mound from the soil of a 1.5 m hole.
		for (int32 Stroke = 0; Stroke < 2; ++Stroke)
		{
			Scene.Terrain->Terraform(Scene.Zenny, Dig, FVector2D(1000, 0));
			Scene.Terrain->Terraform(Scene.Zenny, Raise, FVector2D(-1000, 0));
		}
		TestEqual(TEXT("every spadeful accounted for"), Scene.Inventory->CountOf(Soil), 0);
		TestEqual(TEXT("mound height"), Scene.Terrain->HeightAt(FVector2D(-1000, 0)), 150.0);

		// Building on edited ground: a floor on the mound's flank is refused; flatten, then it stands.
		Scene.Inventory->AddItem(Plank, 4);
		const FVector Flank(-900, 0, 0);
		FGLPlacedPiece OnSlope;
		Scene.Building->Snap(Floor, Flank, 0, OnSlope);
		const FGLBuildCheck Sloped = Scene.Building->Check(Scene.Zenny, OnSlope);
		TestEqual(FString::Printf(TEXT("floor refused on the slope (%s)"), *Sloped.Reason), Sloped.Refusal, EGLBuildRefusal::Unsupported);
		for (int32 Stroke = 0; Stroke < 4; ++Stroke)
		{
			Scene.Terrain->Terraform(Scene.Zenny, Flatten, FVector2D(Flank));
		}
		FGLPlacedPiece Levelled;
		Scene.Building->Snap(Floor, Flank, 0, Levelled);
		const FGLBuildCheck Level = Scene.Building->Place(Scene.Zenny, Levelled);
		TestTrue(FString::Printf(TEXT("after flattening, the floor stands (%s)"), *Level.Reason), Level.IsAllowed());
		// And the ground under it is protected: digging there is refused and yields nothing.
		const FGLTerrainEditResult Under = Scene.Terrain->Terraform(Scene.Zenny, Dig, FVector2D(Levelled.Location));
		TestFalse(TEXT("digging under the floor is refused"), Under.bApplied);
		TestEqual(TEXT("and yields no soil"), Scene.Inventory->CountOf(Soil), 0);
		TestTrue(TEXT("saved"), Scene.Test.World->GetSubsystem<UGLSaveSubsystem>()->SaveToSlot(BuildSlot));
	}
	FBuildScene Reloaded(TEXT("GLTerraformReloaded"));
	TestEqual(TEXT("fresh ground is flat"), Reloaded.Terrain->HeightAt(FVector2D(1000, 0)), 0.0);
	Reloaded.Test.World->GetSubsystem<UGLSaveSubsystem>()->LoadFromSlot(BuildSlot);
	TestTrue(TEXT("the hole is back"), FMath::IsNearlyEqual(Reloaded.Terrain->HeightAt(FVector2D(1000, 0)), -150.0, 0.5));
	double Z = 0.0;
	TestTrue(TEXT("with collision"), Reloaded.Trace(FVector2D(1000, 0), Z) && FMath::IsNearlyEqual(Z, -150.0, 2.0));
	TestEqual(TEXT("the floor on levelled ground is back"), Reloaded.Building->GetPieces().Num(), 1);
	TestTrue(TEXT("and still supported by the restored ground"), Reloaded.Building->Support().FindRef(Reloaded.Building->GetPieces()[0].Id) > 0.0);
	IFileManager::Get().Delete(*UGLSaveSubsystem::SlotPath(BuildSlot));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
