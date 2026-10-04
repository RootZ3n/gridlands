#pragma once

#include "CoreMinimal.h"

class FGLContentRegistry;

/** One piece-browser category: its buildable pieces in display order. */
struct GRIDLANDSCORE_API FGLCatalogCategory
{
	FName Id;
	FString DisplayName;
	int32 Order = 0;
	TArray<FName> Pieces;
};

/**
 * P12 (ADR-0040): the piece browser's catalogue, derived from data alone (pure). Categories in their data order; a piece
 * sits in its explicit category, else in the one category whose fallbackRoles hold its role (CAT-1 makes that total); a
 * piece the data leaves unplaced (never in valid data) lands in a last "Other" category rather than disappearing. Pieces
 * inside a category are ordered by display name, then id. Eras and styles are filters over this, never locks: nothing
 * here groups or forbids by era.
 */
namespace GLBuildCatalog
{
	inline const FName OtherCategory = TEXT("buildcategory.building.other");

	GRIDLANDSCORE_API TArray<FGLCatalogCategory> Build(const FGLContentRegistry& Content);
	/** The category a piece resolves to (None for an unknown or world-only piece). */
	GRIDLANDSCORE_API FName CategoryOf(const FGLContentRegistry& Content, FName Piece);
}
