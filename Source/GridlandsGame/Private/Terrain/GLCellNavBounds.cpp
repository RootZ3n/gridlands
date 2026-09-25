#include "Terrain/GLCellNavBounds.h"

#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"

AGLCellNavBounds::AGLCellNavBounds()
{
	Extent = CreateDefaultSubobject<UBoxComponent>(TEXT("Extent"));
	Extent->SetupAttachment(GetBrushComponent());
	Extent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Extent->SetCanEverAffectNavigation(false);
	Extent->SetHiddenInGame(true);
}

void AGLCellNavBounds::SetExtent(const FVector& HalfExtentCm)
{
	Extent->SetBoxExtent(HalfExtentCm, false);
	Extent->UpdateBounds();
}
