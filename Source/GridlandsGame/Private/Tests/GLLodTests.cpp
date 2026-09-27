// P8 LOD pipeline: every visual's mesh carries the LODs its data budget declares (visual.*.lod), each
// within its triangle budget, switching at the declared screen sizes; cull distances come from data;
// and LODs are presentation only (no gameplay rule may depend on a mesh or LOD level, north star).

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/StaticMesh.h"
#include "Presentation/GLScatterPatch.h"
#include "Presentation/GLVisuals.h"
#include "StaticMeshResources.h"
#include "Structure/GLStructurePart.h"
#include "Structure/GLStructureSubsystem.h"
#include "Tests/GLTestUtils.h"
#include "World/GLGridSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLLodTests
{
	const FVector LodHome(0, -1200, 100);

	/** The art-mesh component on an actor (its mesh lives under Art/Meshes). */
	UStaticMeshComponent* ArtMeshOf(AActor* Actor)
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLLodBudgetsHold, "Gridlands.Game.Lod.EveryVisualMeshHoldsItsLodBudget", GLTestUtils::Flags)
bool FGLLodBudgetsHold::RunTest(const FString& Parameters)
{
	int32 Visuals = 0, Levels = 0;
	bool bGrass = false;
	GLContent::Get().ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLVisualDef* Def = Entry.Definition.GetPtr<FGLVisualDef>();
		if (!Def)
		{
			return;
		}
		++Visuals;
		const FString Id = Entry.Id.ToString();
		const FGLVisualLodDef& Budget = Def->Lod;
		UStaticMesh* Mesh = GLVisuals::LoadMesh(Def->Mesh);
		if (!TestNotNull(*FString::Printf(TEXT("%s: mesh %s is imported"), *Id, *Def->Mesh), Mesh))
		{
			return;
		}
		const FStaticMeshRenderData* Render = Mesh->GetRenderData();
		TestTrue(*FString::Printf(TEXT("%s: declares a budget"), *Id), Budget.MaxTriangles.Num() > 0 && Budget.MaxTriangles.Num() == Budget.ScreenSize.Num());
		TestEqual(*FString::Printf(TEXT("%s: %s has one LOD per budget level"), *Id, *Def->Mesh), Mesh->GetNumLODs(), Budget.MaxTriangles.Num());
		int32 Previous = MAX_int32;
		for (int32 L = 0; L < FMath::Min(Mesh->GetNumLODs(), Budget.MaxTriangles.Num()); ++L)
		{
			const int32 Triangles = Mesh->GetNumTriangles(L);
			TestTrue(*FString::Printf(TEXT("%s: LOD%d has %d triangles, budget %d"), *Id, L, Triangles, Budget.MaxTriangles[L]), Triangles > 0 && Triangles <= Budget.MaxTriangles[L]);
			TestTrue(*FString::Printf(TEXT("%s: LOD%d is lighter than LOD%d"), *Id, L, L - 1), Triangles < Previous);
			if (Render && L < MAX_STATIC_MESH_LODS)
			{
				TestTrue(*FString::Printf(TEXT("%s: LOD%d starts at screen size %.3f (data)"), *Id, L, Budget.ScreenSize[L]),
					FMath::IsNearlyEqual(Render->ScreenSize[L].Default, static_cast<float>(Budget.ScreenSize[L]), 1e-4f));
			}
			Previous = Triangles;
			++Levels;
		}
		bGrass |= Def->Mesh == TEXT("SM_GrassTuft") && Mesh->GetNumLODs() >= 3;
	});
	TestTrue(TEXT("every visual in the content was checked"), Visuals >= 26);
	TestTrue(TEXT("instanced grass has its reduced LODs"), bGrass);
	AddInfo(FString::Printf(TEXT("%d visuals, %d LOD levels within budget"), Visuals, Levels));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLCullDistancesFromData, "Gridlands.Game.Lod.CullDistancesComeFromData", GLTestUtils::Flags)
bool FGLCullDistancesFromData::RunTest(const FString& Parameters)
{
	GLTestUtils::FTestWorld Test(TEXT("GLLodCullWorld"));
	const FGLVisualDef* Grass = GLContent::Get().Find<FGLVisualDef>(TEXT("visual.foliage.grass_tuft"));
	const FGLVisualDef* Bush = GLContent::Get().Find<FGLVisualDef>(TEXT("visual.foliage.bush"));
	if (!TestTrue(TEXT("grass and bush visuals declare cull distances"), Grass && Bush && Grass->CullDistance > 0.0 && Bush->CullDistance > 0.0))
	{
		return false;
	}
	AGLScatterPatch* Patch = Test.World->SpawnActor<AGLScatterPatch>(FVector(0, 0, 0), FRotator::ZeroRotator);
	TestTrue(TEXT("a grass patch"), Patch && Patch->Setup(TEXT("placement.test.lod_grass"), TEXT("visual.foliage.grass_tuft"), 300.0, 10));
	const UHierarchicalInstancedStaticMeshComponent* Instances = Patch ? Patch->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>() : nullptr;
	if (TestNotNull(TEXT("its instances"), Instances))
	{
		TestEqual(TEXT("instanced grass stops drawing at its data distance"), static_cast<int32>(Instances->InstanceEndCullDistance), static_cast<int32>(FMath::RoundToInt(Grass->CullDistance * 100.0)));
		TestTrue(TEXT("and fades from nearer in"), Instances->InstanceStartCullDistance > 0 && Instances->InstanceStartCullDistance < Instances->InstanceEndCullDistance);
	}
	AActor* Holder = Test.World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Holder);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	UStaticMeshComponent* BushLook = GLVisuals::Attach(Holder, Root, TEXT("visual.foliage.bush"), FTransform::Identity);
	UStaticMeshComponent* TableLook = GLVisuals::Attach(Holder, Root, TEXT("visual.prop.side_table"), FTransform::Identity);
	if (TestTrue(TEXT("both visuals attach"), BushLook && TableLook))
	{
		TestEqual(TEXT("a bush stops drawing at its data distance"), BushLook->LDMaxDrawDistance, static_cast<float>(Bush->CullDistance * 100.0));
		TestEqual(TEXT("a visual that declares none keeps the engine default (draws at any distance)"), TableLook->LDMaxDrawDistance, 0.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLLodIsPresentationOnly, "Gridlands.Game.Lod.LodLevelNeverChangesGameplay", GLTestUtils::Flags)
bool FGLLodIsPresentationOnly::RunTest(const FString& Parameters)
{
	GLTestUtils::FTestWorld Test(TEXT("GLLodGameplayWorld"));
	UGLGridSubsystem* Grid = Test.World->GetSubsystem<UGLGridSubsystem>();
	Grid->bShowBoundaries = false;
	Grid->Enable(false);
	Grid->Advance(GLLodTests::LodHome);
	Grid->FlushAll();
	UGLStructureSubsystem* Structures = Test.World->GetSubsystem<UGLStructureSubsystem>();
	AGLStructurePart* Trunk = Structures->FindPart(TEXT("placement.origin.structure_pine_01"), TEXT("trunk"));
	UStaticMeshComponent* Look = Trunk ? GLLodTests::ArtMeshOf(Trunk) : nullptr;
	if (!TestTrue(TEXT("the pine trunk and its art mesh"), Trunk && Look && Look->GetStaticMesh()->GetNumLODs() >= 3))
	{
		return false;
	}
	// What gameplay sees of the trunk: a blocking trace across it (the hidden data shapes answer it).
	const FVector At = Trunk->GetActorLocation() + FVector(0, 0, 100);
	auto Probe = [&](FHitResult& Hit)
	{
		return Test.World->LineTraceSingleByChannel(Hit, At + FVector(600, 0, 0), At - FVector(600, 0, 0), ECC_Visibility);
	};
	FHitResult Full, Lowest;
	const bool bFull = Probe(Full);
	Look->SetForcedLodModel(Look->GetStaticMesh()->GetNumLODs()); // the lightest LOD (1-based)
	const bool bLowest = Probe(Lowest);
	Look->SetForcedLodModel(0);
	TestTrue(TEXT("the trunk blocks a trace at LOD0"), bFull);
	TestEqual(TEXT("and exactly the same way at its lightest LOD"), bLowest, bFull);
	TestTrue(TEXT("the same component answers"), Full.GetComponent() == Lowest.GetComponent() && Full.GetComponent() != Look);
	TestTrue(TEXT("at the same point"), Full.ImpactPoint.Equals(Lowest.ImpactPoint, 0.01));
	TestEqual(TEXT("the art mesh has no collision at any LOD"), Look->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	return true;
}

#endif
