#include "World/GLNavRegionSubsystem.h"

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "GridlandsGame.h"
#include "NavigationInvokerComponent.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

namespace
{
	constexpr double DemandSeconds = 1.0;
	constexpr double SweepEverySeconds = 0.5;
	constexpr double SweepGraceSeconds = 1.0; // long enough for a removal Recast still has in flight
	constexpr double SweepWatchSeconds = 10.0; // a finishing build task can put a removed tile back seconds later

	ARecastNavMesh* RecastOf(UWorld* World)
	{
		UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
		return Nav ? Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
	}
}

AGLNavRegionInvoker::AGLNavRegionInvoker()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	NavInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvoker"));
	NavInvoker->bAutoActivate = false;
}

void AGLNavRegionInvoker::SetRadius(double GenerationCm)
{
	NavInvoker->SetGenerationRadii(GenerationCm, GenerationCm * 1.5);
}

void AGLNavRegionInvoker::SetProviding(bool bOn)
{
	if (bOn != NavInvoker->IsActive())
	{
		NavInvoker->SetActive(bOn);
	}
}

bool AGLNavRegionInvoker::IsProviding() const
{
	return NavInvoker && NavInvoker->IsActive();
}

bool UGLNavRegionSubsystem::AddRegion(FName Placement, FName Definition, FName Cell, const FVector& Centre)
{
	const FGLNavRegionDef* Def = GLContent::Get().Find<FGLNavRegionDef>(Definition);
	if (!Def || Def->Extent.Num() < 3)
	{
		return false;
	}
	FGLNavRegion& Region = Regions.Add(Placement);
	Region.Placement = Placement;
	Region.Cell = Cell;
	Region.Bounds = FBox::BuildAABB(Centre, FVector(Def->Extent[0], Def->Extent[1], Def->Extent[2]) * 100.0);
	Region.RelevanceCm = (Def->RelevanceMargin > 0.0 ? Def->RelevanceMargin : 32.0) * 100.0;
	UE_LOG(LogGridlands, Log, TEXT("NavRegion: %s added, bounds %s"), *Placement.ToString(), *Region.Bounds.ToString());
	return true;
}

void UGLNavRegionSubsystem::RemoveCell(FName Cell)
{
	for (auto It = Regions.CreateIterator(); It; ++It)
	{
		if (It.Value().Cell == Cell)
		{
			if (AGLNavRegionInvoker* Invoker = It.Value().Invoker.Get())
			{
				Invoker->Destroy(); // its tiles go with it
			}
			It.RemoveCurrent();
		}
	}
}

void UGLNavRegionSubsystem::Update(const FVector& Zenny)
{
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	for (TPair<FName, FGLNavRegion>& Entry : Regions)
	{
		FGLNavRegion& Region = Entry.Value;
		const bool bRelevant = !bDevWithholdRegions && (Region.Bounds.ExpandBy(Region.RelevanceCm).IsInsideXY(Zenny) || Now < Region.DemandedUntil);
		AGLNavRegionInvoker* Invoker = Region.Invoker.Get();
		if (bRelevant && !Invoker)
		{
			// One invoker covering the region's footprint (invokers are circles: half its diagonal).
			Invoker = World->SpawnActor<AGLNavRegionInvoker>(Region.Bounds.GetCenter(), FRotator::ZeroRotator);
			if (Invoker)
			{
				Invoker->SetRadius(FVector2D(Region.Bounds.GetExtent()).Size());
				Region.Invoker = Invoker;
			}
		}
		if (Invoker)
		{
			if (Invoker->IsProviding() != bRelevant)
			{
				UE_LOG(LogGridlands, Log, TEXT("NavRegion: %s %s (Zenny %s)"), *Entry.Key.ToString(), bRelevant ? TEXT("providing") : TEXT("stopped"), *Zenny.ToCompactString());
			}
			Invoker->SetProviding(bRelevant);
		}
	}
	SweepStaleTiles(Now);
}

void UGLNavRegionSubsystem::SweepStaleTiles(double Now)
{
	if (Now < NextSweep)
	{
		return;
	}
	NextSweep = Now + SweepEverySeconds;
	ARecastNavMesh* Recast = RecastOf(GetWorld());
	if (!Recast)
	{
		return;
	}
	const TSet<FIntPoint>& Active = Recast->GetActiveTileSet();
	for (const FIntPoint& Tile : LastActive)
	{
		if (!Active.Contains(Tile))
		{
			LeftAt.Add(Tile, Now); // left (again) now
		}
	}
	TArray<FIntPoint> Remove;
	for (auto It = LeftAt.CreateIterator(); It; ++It)
	{
		if (Active.Contains(It.Key()))
		{
			It.RemoveCurrent(); // active again: not stale
			continue;
		}
		const double Since = Now - It.Value();
		if (Since < SweepGraceSeconds)
		{
			continue;
		}
		TArray<int32> Built;
		Recast->GetNavMeshTilesAt(It.Key().X, It.Key().Y, Built);
		if (Built.Num() > 0)
		{
			Remove.Add(It.Key()); // put back outside every invoker: remove it again, and keep watching
		}
		else if (Since > SweepWatchSeconds)
		{
			It.RemoveCurrent(); // stayed gone: done
		}
	}
	if (Remove.Num() > 0)
	{
		Recast->RemoveTiles(Remove);
		Swept += Remove.Num();
	}
	LastActive = Active;
}

int32 UGLNavRegionSubsystem::CountStaleTiles(const FVector& Around, int32 RadiusTiles) const
{
	ARecastNavMesh* Recast = RecastOf(GetWorld());
	if (!Recast)
	{
		return 0;
	}
	int32 CX = 0, CY = 0;
	Recast->GetNavMeshTileXY(Around, CX, CY);
	const TSet<FIntPoint>& Active = Recast->GetActiveTileSet();
	int32 Stale = 0;
	for (int32 X = CX - RadiusTiles; X <= CX + RadiusTiles; ++X)
	{
		for (int32 Y = CY - RadiusTiles; Y <= CY + RadiusTiles; ++Y)
		{
			TArray<int32> Built;
			Recast->GetNavMeshTilesAt(X, Y, Built);
			Stale += Built.Num() > 0 && !Active.Contains(FIntPoint(X, Y)) ? 1 : 0;
		}
	}
	return Stale;
}

bool UGLNavRegionSubsystem::ProvideFor(const FVector& At) const
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (bDevWithholdRegions)
	{
		return false;
	}
	for (TPair<FName, FGLNavRegion>& Entry : Regions)
	{
		if (Entry.Value.Bounds.IsInside(At))
		{
			Entry.Value.DemandedUntil = Now + DemandSeconds;
			return true;
		}
	}
	return false;
}

bool UGLNavRegionSubsystem::IsProvidedAt(const FVector& At) const
{
	for (const TPair<FName, FGLNavRegion>& Entry : Regions)
	{
		if (Entry.Value.Bounds.IsInside(At) && Entry.Value.Invoker.IsValid() && Entry.Value.Invoker->IsProviding())
		{
			return true;
		}
	}
	return false;
}

int32 UGLNavRegionSubsystem::ActiveRegions() const
{
	int32 N = 0;
	for (const TPair<FName, FGLNavRegion>& Entry : Regions)
	{
		N += Entry.Value.Invoker.IsValid() && Entry.Value.Invoker->IsProviding() ? 1 : 0;
	}
	return N;
}
