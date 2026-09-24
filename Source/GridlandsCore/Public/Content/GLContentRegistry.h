#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "StructUtils/InstancedStruct.h"

/** One problem found while loading Data/. Rule codes match Docs/CONTENT-IDS-AND-TAGS.md where they overlap. */
struct GRIDLANDSCORE_API FGLContentProblem
{
	FString Rule;
	FString File;
	FString Message;

	FString ToString() const { return FString::Printf(TEXT("%s %s: %s"), *Rule, *File, *Message); }
};

/** A loaded entity: the source JSON (kept for round-trip checks) and its typed definition. */
struct GRIDLANDSCORE_API FGLContentEntry
{
	FName Id;
	FName Kind;
	/** Path relative to the repository root, e.g. "Data/item/material/copper_wire.json". */
	FString File;
	TSharedPtr<FJsonObject> Source;
	FInstancedStruct Definition;
};

/**
 * Loads every JSON entity under Data/ into typed, read-only definitions keyed by id (ADR-0021).
 *
 * Pure: file IO and JSON only, no world. Tools/data.sh validate is the full rule set; the
 * loader re-checks what it depends on (id grammar, kind, path, closed keys) and never guesses:
 * a file it cannot type exactly is a problem, not a partial entity.
 */
class GRIDLANDSCORE_API FGLContentRegistry
{
public:
	/** Loads every entity under RepoRoot/Data. Returns true when no problem was recorded. */
	bool LoadRepository(const FString& RepoRoot);

	const TArray<FGLContentProblem>& GetProblems() const { return Problems; }
	int32 Num() const { return Entries.Num(); }
	const FGLContentEntry* FindEntry(FName Id) const { return Entries.Find(Id); }
	bool HasAnchor(FName AnchorId) const { return AnchorIds.Contains(AnchorId); }
	void ForEachEntry(TFunctionRef<void(const FGLContentEntry&)> Visit) const;

	/** The typed definition for Id, or nullptr if absent or of a different kind. */
	template <typename TDefinition>
	const TDefinition* Find(FName Id) const
	{
		const FGLContentEntry* Entry = Entries.Find(Id);
		return Entry && Entry->Definition.GetScriptStruct() == TDefinition::StaticStruct() ? Entry->Definition.GetPtr<TDefinition>() : nullptr;
	}

	/** The definition struct for a kind, or nullptr if the kind has no typed definition yet. */
	static const UScriptStruct* StructForKind(FName Kind);
	/** Every kind that has a typed definition. */
	static TArray<FName> TypedKinds();

	/** JSON key paths of Json that name no property of Struct (closed schemas in C++). */
	static void FindUnknownKeys(const UStruct* Struct, const FJsonObject& Json, const FString& Path, TArray<FString>& OutPaths);
	/** Key paths a struct accepts, in the notation of Data/_registry/schema.generated.json. */
	static void CollectKeyPaths(const UStruct* Struct, const FString& Prefix, TArray<FString>& OutPaths);
	/** True if every field of Source survives Struct -> JSON unchanged; otherwise OutMismatch names the first field that did not. */
	static bool SurvivesRoundTrip(const FJsonObject& Source, const UScriptStruct* Struct, const void* Memory, FString& OutMismatch);

private:
	void AddProblem(const FString& Rule, const FString& File, const FString& Message);
	void LoadEntityFile(const FString& DataDir, const FString& AbsolutePath, const FString& RelativePath);
	void LoadAnchorFile(const FString& AbsolutePath, const FString& RelativePath);

	TMap<FName, FGLContentEntry> Entries;
	TSet<FName> RegisteredKinds;
	TSet<FName> AnchorIds;
	TArray<FGLContentProblem> Problems;
};
