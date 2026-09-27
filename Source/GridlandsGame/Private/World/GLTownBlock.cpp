// DEV ONLY (P8): the production-density town block. Where the P7 dense fixture (GLDenseProof) is a stress
// strip off the route, this is a credible town block ON the crossing route: the street the crossings walk
// (lots-local y = -30000) runs between two shop faces with an alley, back-lot parking, driveways, street
// trees, sidewalk props, yards with grass, flowers and bushes, gremlins, glitches and salvage nodes. It is
// placements in memory (never Data/, never shipping), built only from existing definitions, and it uses
// every runtime path a cell's content uses: structures, scatter, creatures, glitches, salvage nodes.

#include "World/GLPlacementSubsystem.h"

#include "GridlandsGame.h"

#if !UE_BUILD_SHIPPING

namespace
{
	struct FGLTownSite
	{
		const TCHAR* Kind;
		const TCHAR* Definition;
		double X; // lots-local cm
		double Y;
		double Z;
		int32 YawQuarter;
		double RadiusM; // scatter
		int32 Count;    // scatter
		const TCHAR* Binding; // "name=SALVAGE<i>": the i-th salvage node of the block
	};

	const FGLTownSite TownSites[] = {
#include "GLTownBlockSites.inl"
	};
}

FName UGLPlacementSubsystem::TownBlockId(int32 Index)
{
	return FName(*FString::Printf(TEXT("placement.diner_lots.proof_town_%03d"), Index));
}

int32 UGLPlacementSubsystem::AddTownBlock()
{
	const FName Lots(TEXT("cell.outer.diner_lots"));
	TArray<FName> Salvage;
	for (int32 I = 0; I < UE_ARRAY_COUNT(TownSites); ++I)
	{
		if (FCString::Strcmp(TownSites[I].Kind, TEXT("salvage_node")) == 0)
		{
			Salvage.Add(TownBlockId(I));
		}
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(TownSites); ++I)
	{
		const FGLTownSite& Site = TownSites[I];
		FGLPlacementDef P;
		P.Kind = Site.Kind;
		P.Definition = Site.Definition;
		P.Transform.Location = { Site.X, Site.Y, Site.Z };
		P.Transform.Yaw = Site.YawQuarter * 90.0;
		P.Radius = Site.RadiusM;
		P.Count = Site.Count;
		FString Name, Target;
		if (FString(Site.Binding).Split(TEXT("=SALVAGE"), &Name, &Target))
		{
			const int32 Which = FCString::Atoi(*Target);
			if (Salvage.IsValidIndex(Which))
			{
				P.Bindings.Add(Name, Salvage[Which]);
			}
		}
		AddProofPlacement(Lots, TownBlockId(I), P);
	}
	UE_LOG(LogGridlands, Log, TEXT("Placements: DEV town block registered (%d placements in %s)"), UE_ARRAY_COUNT(TownSites), *Lots.ToString());
	return UE_ARRAY_COUNT(TownSites);
}

#endif
