#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * P12 (ADR-0040): the daily-driver friction log. A playtest note (a category from a small stable vocabulary, a short
 * free-text note, and the build context the moment it was written) appended as one JSON line to
 * Saved/Playtest/friction.jsonl. Playtest evidence only: not gameplay state, never part of a save, and it records nothing
 * about the player beyond what build mode was doing.
 */
namespace GLFrictionLog
{
	GRIDLANDSGAME_API const TArray<FString>& Categories();
	GRIDLANDSGAME_API FString LogPath();
	/** Appends one note; false for an unknown category. The context comes from the first player's build mode. */
	GRIDLANDSGAME_API bool Write(UWorld* World, const FString& Category, const FString& Note);
	/** The F8 quick picker (a category, an optional note; Enter saves, Esc cancels). No-op without a game viewport. */
	GRIDLANDSGAME_API void OpenPicker(UWorld* World);
	GRIDLANDSGAME_API bool IsPickerOpen();
}
