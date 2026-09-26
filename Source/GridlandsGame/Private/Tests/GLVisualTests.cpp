// P7 visual pipeline compatibility (ADR-0032): visuals are presentation only (no collision, no
// navigation), follow structural collapse, vegetation follows the ground, corruption stays sparse.

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Inventory/GLInventoryComponent.h"
#include "Materials/MaterialInterface.h"
#include "Presentation/GLScatterPatch.h"
#include "Presentation/GLVisuals.h"
#include "Salvage/GLSalvageableComponent.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Terrain/GLTerrainSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLGridSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLVisualTests
{
	const FVector VHome(0, -1200, 100);

	struct FVisualScene
	{
		GLTestUtils::FTestWorld Test;
		ACharacter* Zenny = nullptr;
		UGLInventoryComponent* Inventory = nullptr;
		UGLTerrainSubsystem* Terrain = nullptr;
		UGLStructureSubsystem* Structures = nullptr;

		explicit FVisualScene(const TCHAR* Name) : Test(Name)
		{
			Zenny = Test.World->SpawnActor<ACharacter>(VHome, FRotator::ZeroRotator);
			Inventory = NewObject<UGLInventoryComponent>(Zenny);
			Inventory->RegisterComponent();
			UGLGridSubsystem* Grid = Test.World->GetSubsystem<UGLGridSubsystem>();
			Grid->bShowBoundaries = false;
			Grid->Enable(false);
			Grid->Advance(VHome);
			Grid->FlushAll();
			Terrain = Test.World->GetSubsystem<UGLTerrainSubsystem>();
			Structures = Test.World->GetSubsystem<UGLStructureSubsystem>();
		}

		/** The art-mesh component a visual attached to an actor (the one whose mesh is under Art/Meshes). */
		static UStaticMeshComponent* VisualOf(AActor* Actor)
		{
			TArray<UStaticMeshComponent*> Meshes;
			Actor->GetComponents(Meshes);
			for (UStaticMeshComponent* M : Meshes)
			{
				if (M->GetStaticMesh() && M->GetStaticMesh()->GetPathName().StartsWith(GLVisuals::MeshesPath))
				{
					return M;
				}
			}
			return nullptr;
		}
	};
}

using GLVisualTests::FVisualScene;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLVisualsArePresentation, "Gridlands.Game.Presentation.VisualsCarryNoCollisionAndFollowCollapse", GLTestUtils::Flags)
bool FGLVisualsArePresentation::RunTest(const FString& Parameters)
{
	FVisualScene S(TEXT("GLVisualsWorld"));
	const FName Pine(TEXT("placement.origin.structure_pine_01"));
	AGLStructurePart* Trunk = S.Structures->FindPart(Pine, TEXT("trunk"));
	UStaticMeshComponent* Look = Trunk ? FVisualScene::VisualOf(Trunk) : nullptr;
	if (!TestNotNull(TEXT("the pine trunk has its art mesh (visual.nature.pine)"), Look))
	{
		return false;
	}
	TestEqual(TEXT("the visual has no collision"), Look->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("and never affects navigation"), Look->CanEverAffectNavigation());
	TestTrue(TEXT("it is outlined (writes custom depth)"), Look->bRenderCustomDepth);
	// The authoritative data boxes still block, invisibly.
	bool bBlockingBox = false;
	TArray<UStaticMeshComponent*> Meshes;
	Trunk->GetComponents(Meshes);
	for (UStaticMeshComponent* M : Meshes)
	{
		bBlockingBox |= M != Look && M->GetCollisionEnabled() != ECollisionEnabled::NoCollision && !M->IsVisible();
	}
	TestTrue(TEXT("collision stays on the hidden data shapes"), bBlockingBox);
	// Fell the tree: the look moves with the authoritative pose, all the way to rest.
	AGLStructurePart* Stump = S.Structures->FindPart(Pine, TEXT("stump"));
	S.Zenny->SetActorLocation(FVector(-1700, -4500, 100));
	for (int32 Hit = 0; Stump && Hit < 20 && !Stump->GetSalvageable()->IsSalvaged(); ++Hit)
	{
		Stump->GetSalvageable()->Interact(S.Zenny, GLTestUtils::Tag(TEXT("Interact.Salvage")));
	}
	for (int32 F = 0; F < 400; ++F)
	{
		S.Structures->Advance(1.0 / 60.0);
	}
	const FGLStructureRuntime* Tree = S.Structures->Find(Pine);
	const FGLStructurePartRuntime* Part = Tree ? Tree->Parts.FindByPredicate([](const FGLStructurePartRuntime& P) { return P.Name == FName(TEXT("trunk")); }) : nullptr;
	TestTrue(TEXT("the trunk is debris"), Part && Part->State == EGLStructurePartState::Debris);
	TestTrue(TEXT("its look lies where the authoritative rest is"), Part && Look->GetComponentLocation().Equals(Part->Rest.GetLocation(), 1.0));
	TestTrue(TEXT("and still carries no collision"), Look->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLVegetationFollowsGround, "Gridlands.Game.Presentation.VegetationFollowsTheGround", GLTestUtils::Flags)
bool FGLVegetationFollowsGround::RunTest(const FString& Parameters)
{
	FVisualScene S(TEXT("GLVegetationWorld"));
	const FVector2D Centre(-9000, -9000);
	AGLScatterPatch* Patch = S.Test.World->SpawnActor<AGLScatterPatch>(FVector(Centre, S.Terrain->HeightAt(Centre)), FRotator::ZeroRotator);
	if (!TestTrue(TEXT("a grass patch"), Patch && Patch->Setup(TEXT("placement.test.grass"), TEXT("visual.foliage.grass_tuft"), 600.0, 400)))
	{
		return false;
	}
	const int32 Before = Patch->GetInstanceCount();
	TestEqual(TEXT("every tuft grows on untouched lawn"), Before, 400);
	Patch->Rebuild();
	TestEqual(TEXT("deterministic"), Patch->GetInstanceCount(), Before);
	// Dig a pit in the middle through the real terraform path (its event re-plants the patch).
	S.Inventory->AddItem(TEXT("item.tool.shovel"), 1);
	S.Zenny->SetActorLocation(FVector(Centre + FVector2D(250, 0), S.Terrain->HeightAt(Centre) + 100.0)); // within reach, as a player digs
	for (int32 I = 0; I < 4; ++I)
	{
		for (const FVector2D& D : { FVector2D(0, 0), FVector2D(120, 0), FVector2D(-120, 0), FVector2D(0, 120), FVector2D(0, -120) })
		{
			S.Terrain->Terraform(S.Zenny, TEXT("terraform.shovel.dig"), Centre + D);
		}
	}
	const int32 After = Patch->GetInstanceCount();
	AddInfo(FString::Printf(TEXT("tufts: %d before the pit, %d after"), Before, After));
	TestTrue(TEXT("nothing grows on the exposed earth"), After < Before);
	UHierarchicalInstancedStaticMeshComponent* Instances = Patch->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>();
	bool bOnEarth = false, bFloating = false;
	for (int32 I = 0; I < Instances->GetInstanceCount(); ++I)
	{
		FTransform T;
		Instances->GetInstanceTransform(I, T, true);
		bOnEarth |= S.Terrain->EditedAt(FVector2D(T.GetLocation())) > 5.0;
		bFloating |= FMath::Abs(T.GetLocation().Z - S.Terrain->HeightAt(FVector2D(T.GetLocation()))) > 2.0;
	}
	TestFalse(TEXT("no tuft stands on dug ground"), bOnEarth);
	TestFalse(TEXT("every tuft sits on the ground"), bFloating);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCorruptionIsSparse, "Gridlands.Game.Presentation.CorruptionIsSparseAndPrecise", GLTestUtils::Flags)
bool FGLCorruptionIsSparse::RunTest(const FString& Parameters)
{
	FVisualScene S(TEXT("GLCorruptionWorld"));
	AActor* Holder = S.Test.World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Holder);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	UStaticMeshComponent* Phone = GLVisuals::Attach(Holder, Root, TEXT("visual.prop.rotary_phone_glitched"));
	if (!TestNotNull(TEXT("the glitched phone"), Phone))
	{
		return false;
	}
	UMaterialInterface* Corrupt = LoadObject<UMaterialInterface>(nullptr, GLVisuals::CorruptionMaterialPath);
	int32 Cubes = 0;
	TArray<UStaticMeshComponent*> Meshes;
	Holder->GetComponents(Meshes);
	for (UStaticMeshComponent* M : Meshes)
	{
		if (M->GetMaterial(0) == Corrupt)
		{
			++Cubes;
			TestEqual(TEXT("a corruption cube has no collision"), M->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			TestFalse(TEXT("and is never outlined (precise, not illustrated)"), M->bRenderCustomDepth);
			TestTrue(TEXT("it is the engine's exact cube"), M->GetStaticMesh() && M->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/Cube")));
		}
	}
	TestEqual(TEXT("ONE impossible cube replaces part of the receiver"), Cubes, 1);
	int32 Worst = 0;
	GLContent::Get().ForEachEntry([&Worst](const FGLContentEntry& Entry)
	{
		if (const FGLVisualDef* V = Entry.Definition.GetPtr<FGLVisualDef>())
		{
			Worst = FMath::Max(Worst, V->Corruption.Num());
		}
	});
	TestTrue(TEXT("no visual carries more than 4 cubes (VIS-2)"), Worst <= 4);
	return true;
}

#endif
