// DEV ONLY (P7, ADR-0033): the dense authored stress fixture. It asks one question: does the runtime
// layer's spawning scale when a cell holds a town's worth of structures instead of one storefront?
// It is placements in memory (never Data/, never shipping), in the diner lots, built only from existing
// structure and visual definitions. Sites were chosen by the same support rule every authored structure
// must pass (every part supported on the cell's base ground at yaw 0); DenseFixtureStands re-checks it.

#include "World/GLPlacementSubsystem.h"

#include "GridlandsGame.h"

#if !UE_BUILD_SHIPPING

namespace
{
	struct FGLDenseSite
	{
		const TCHAR* Definition;
		double X; // cell-local cm
		double Y;
	};

	const FGLDenseSite DenseSites[] = {
#include "GLDenseProofSites.inl"
	};

	/** Vegetation among the buildings (presentation only). */
	const FGLDenseSite DenseScatter[] = {
		{ TEXT("visual.foliage.grass_tuft"), -30000, 12000 },
		{ TEXT("visual.foliage.grass_tuft"), -6000, 12000 },
		{ TEXT("visual.foliage.grass_tuft"), 18000, 12000 },
		{ TEXT("visual.foliage.grass_tuft"), -30000, 36000 },
		{ TEXT("visual.foliage.grass_tuft"), 18000, 36000 },
		{ TEXT("visual.foliage.flower"), -18000, 24000 },
		{ TEXT("visual.foliage.flower"), 6000, 24000 },
		{ TEXT("visual.foliage.bush"), 30000, 24000 },
	};
}

void UGLPlacementSubsystem::AddProofPlacement(FName CellId, FName Id, const FGLPlacementDef& Placement)
{
	TArray<TPair<FName, FGLPlacementDef>>& List = ProofPlacements.FindOrAdd(CellId);
	if (!List.ContainsByPredicate([Id](const TPair<FName, FGLPlacementDef>& E) { return E.Key == Id; }))
	{
		List.Add({ Id, Placement });
	}
}

int32 UGLPlacementSubsystem::AddDenseProof()
{
	const FName Lots(TEXT("cell.outer.diner_lots"));
	int32 Index = 0;
	for (const FGLDenseSite& Site : DenseSites)
	{
		FGLPlacementDef P;
		P.Kind = TEXT("structure");
		P.Definition = Site.Definition;
		P.Transform.Location = { Site.X, Site.Y, 0.0 };
		AddProofPlacement(Lots, FName(*FString::Printf(TEXT("placement.diner_lots.proof_dense_%03d"), Index++)), P);
	}
	for (const FGLDenseSite& Site : DenseScatter)
	{
		FGLPlacementDef P;
		P.Kind = TEXT("scatter");
		P.Definition = Site.Definition;
		P.Transform.Location = { Site.X, Site.Y, 0.0 };
		P.Radius = 20.0;
		P.Count = 700;
		AddProofPlacement(Lots, FName(*FString::Printf(TEXT("placement.diner_lots.proof_dense_%03d"), Index++)), P);
	}
	UE_LOG(LogGridlands, Log, TEXT("Placements: DEV dense proof fixture registered (%d placements in %s)"), Index, *Lots.ToString());
	return Index;
}

#endif
