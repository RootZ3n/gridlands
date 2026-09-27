// DEV ONLY (P9, ADR-0037): the dual-route encounter proof room. Deliberately ugly: dev walls on the lots'
// terrain, engine cubes for the mechanisms. It asks one question: does the model-first encounter architecture
// hold for both routes through the same space (direct: fight the patrols, defeat the warden through its health;
// environmental: slip past the patrols, time the fan against the noisy grate, lead the warden into the cage
// while it attacks, Pehlichi operates the cage) across streaming, save and restart? In-memory placements, like
// the P7/P8 fixtures: never Data/ placements (PRF-1), never shipping.

#include "World/GLPlacementSubsystem.h"

#include "GridlandsGame.h"
#include "Terrain/GLTerrainSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace
{
	struct FGLProofSite
	{
		const TCHAR* Kind;
		const TCHAR* Definition;
		double X; // room-local cm
		double Y;
		double Z; // lots-local ground height at the generated origin
		int32 YawQuarter;
		const TCHAR* Patrol; // "x,y;x,y" room-local cm (spawns)
	};

	const FGLProofSite ProofSites[] = {
#include "GLDungeonProofLayout.inl"
	};
}

// GENERATED once (P9) by a site search over the lots: the room origin where every wall and the grate stand by the
// authored-structure support rule. DungeonProofStands re-checks it.
const FVector UGLPlacementSubsystem::DungeonProofOrigin(-46000.0, 26000.0, 0.0);

FName UGLPlacementSubsystem::DungeonProofId(int32 Index)
{
	return FName(*FString::Printf(TEXT("placement.diner_lots.proof_dungeon_%03d"), Index));
}

int32 UGLPlacementSubsystem::DungeonProofCount()
{
	return UE_ARRAY_COUNT(ProofSites);
}

int32 UGLPlacementSubsystem::AddDungeonProof(const FVector& Origin)
{
	const FName Lots(TEXT("cell.outer.diner_lots"));
	for (int32 I = 0; I < UE_ARRAY_COUNT(ProofSites); ++I)
	{
		const FGLProofSite& Site = ProofSites[I];
		FGLPlacementDef P;
		P.Kind = Site.Kind;
		P.Definition = Site.Definition;
		P.Transform.Location = { Origin.X + Site.X, Origin.Y + Site.Y, Site.Z };
		P.Transform.Yaw = Site.YawQuarter * 90.0;
		TArray<FString> Points;
		FString(Site.Patrol).ParseIntoArray(Points, TEXT(";"));
		for (const FString& Point : Points)
		{
			FString X, Y;
			if (Point.Split(TEXT(","), &X, &Y))
			{
				FGLPlacementPointDef Waypoint;
				Waypoint.Location = { Origin.X + FCString::Atod(*X), Origin.Y + FCString::Atod(*Y), 0.0 };
				P.Patrol.Add(Waypoint);
			}
		}
		AddProofPlacement(Lots, DungeonProofId(I), P);
	}
	UE_LOG(LogGridlands, Log, TEXT("Placements: DEV dungeon proof room registered (%d placements in %s)"), UE_ARRAY_COUNT(ProofSites), *Lots.ToString());
	return UE_ARRAY_COUNT(ProofSites);
}

#endif
