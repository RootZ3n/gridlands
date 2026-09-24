#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FGLCellDef;

/** Anchor export (ADR-0018): the visual world's gameplay handles, as tracked, deterministic JSON. */
namespace GLAnchorExport
{
	/** Loads the map at LongPackageName (e.g. /Game/Gridlands/Maps/L_Origin); null if it does not exist. */
	GRIDLANDSEDITOR_API UWorld* LoadMap(const FString& LongPackageName);

	/**
	 * Renders every UGLAnchorComponent in World as the text of Data/anchor/<cell>.generated.json.
	 * Output is sorted and fixed-precision, so the same map always gives the same bytes.
	 * Problems (bad ids, wrong cell, duplicates) are appended to OutProblems.
	 */
	GRIDLANDSEDITOR_API FString Render(UWorld* World, const FString& CellId, TArray<FString>& OutProblems);

	/** Repo-relative path of a cell's anchor file, e.g. Data/anchor/origin.generated.json. */
	GRIDLANDSEDITOR_API FString FilePathForCell(const FString& CellId);
}
