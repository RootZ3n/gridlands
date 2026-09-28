#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "GLNavRegionSubsystem.generated.h"

class UNavigationInvokerComponent;

/** P9: the navigation invoker an active authored region carries (one per region, sized to cover it). */
UCLASS(NotPlaceable)
class GRIDLANDSGAME_API AGLNavRegionInvoker : public AActor
{
	GENERATED_BODY()

public:
	AGLNavRegionInvoker();
	void SetRadius(double GenerationCm);
	void SetProviding(bool bOn);
	bool IsProviding() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UNavigationInvokerComponent> NavInvoker;
};

/** P9: an authored navigation region (placement kind nav_region): a bounded encounter space. */
struct FGLNavRegion
{
	FName Placement;
	FName Cell;
	FBox Bounds = FBox(ForceInit);
	double RelevanceCm = 3200.0;
	double DemandedUntil = -1e9;
	TWeakObjectPtr<AGLNavRegionInvoker> Invoker;
};

/**
 * ADR-0029 as amended (P9): NAVIGATION EXISTS WHERE ACTIVE GAMEPLAY REQUIRES IT. An authored region provides
 * navigation for its declared volume while Zenny is relevant to it (inside, or within its relevance margin) or
 * while gameplay inside it needs navigation (an active creature asked within the last second). Creatures inside
 * an active region carry no invoker of their own, so its cost is its area, not its creature count. Still
 * localized: no region is ever active far from Zenny and its creatures, and nothing is whole-cell.
 */
UCLASS()
class GRIDLANDSGAME_API UGLNavRegionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool AddRegion(FName Placement, FName Definition, FName Cell, const FVector& Centre);
	void RemoveCell(FName Cell);
	/** Each streaming step (UGLGridSubsystem::Advance): regions switch on or off for Zenny's position and demand. */
	void Update(const FVector& Zenny);

	/**
	 * A creature at At needs navigation now. True if an authored region covers At (it becomes demanded, and is
	 * active by the next update), so the creature needs no invoker of its own.
	 */
	bool ProvideFor(const FVector& At) const;
	/** Whether a region covering At is providing navigation right now (tests, evidence). */
	bool IsProvidedAt(const FVector& At) const;
	int32 ActiveRegions() const;
	/**
	 * P9: built navigation tiles outside Recast's active set right now (should be none: localized navigation
	 * leaves nothing behind). Scans tile columns within RadiusTiles of the origin tile of Around.
	 */
	int32 CountStaleTiles(const FVector& Around, int32 RadiusTiles) const;
	/** DEV ONLY (measurement contrast): regions provide nothing, so every active creature carries its own invoker. */
	bool bDevWithholdRegions = false;
	/** Tiles the sweep removed so far (evidence). */
	int32 GetSweptTiles() const { return Swept; }
	const TMap<FName, FGLNavRegion>& GetRegions() const { return Regions; }

private:
	/**
	 * P9 defect fix: a tile that leaves the active set is removed by Recast, but a build task finishing after the
	 * removal puts it back, outside every invoker, and nothing removes it again (measured: 17 such tiles after
	 * a 300 m walk). Tiles that left the set are tracked and removed again once they are still built a moment later.
	 */
	void SweepStaleTiles(double Now);

	/** Mutable: demand is recorded by the const query creatures make every step. */
	mutable TMap<FName, FGLNavRegion> Regions;
	TSet<FIntPoint> LastActive;
	TMap<FIntPoint, double> LeftAt;
	double NextSweep = 0.0;
	int32 Swept = 0;
};
