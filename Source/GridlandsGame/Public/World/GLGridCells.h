#pragma once

#include "CoreMinimal.h"

struct FGLCellDef;

/** Grid-cell geometry from content (P3, ADR-0026): which cell a world point is in, and where cells are. */
namespace GLGridCells
{
	/** The cell whose square contains a world point (cm), or None. */
	GRIDLANDSGAME_API FName CellAt(const FVector2D& WorldCm);
	/** Every cell with a pitch, sorted by id. */
	GRIDLANDSGAME_API TArray<FName> AllCells();
	/** Distance (cm) from a point to a cell's square (0 inside). */
	GRIDLANDSGAME_API double DistanceToCell(const FGLCellDef& Cell, const FVector2D& WorldCm);
}
