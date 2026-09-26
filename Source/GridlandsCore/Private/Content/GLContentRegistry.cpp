#include "Content/GLContentRegistry.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentId.h"
#include "GridlandsCore.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

namespace GLContentRegistry
{
	struct FKindStruct
	{
		const TCHAR* Kind;
		UScriptStruct* (*Struct)();
	};

	// Kinds with a typed definition. Must equal the kinds with a schema in Tools/gldata/schema.py;
	// Gridlands.Core.Content.SchemaKeysMatchValidator fails if they drift.
	const FKindStruct KindStructs[] = {
		{ TEXT("era"), &FGLEraDef::StaticStruct },
		{ TEXT("band"), &FGLBandDef::StaticStruct },
		{ TEXT("yield"), &FGLYieldCategoryDef::StaticStruct },
		{ TEXT("settings"), &FGLSettingsPresetDef::StaticStruct },
		{ TEXT("material"), &FGLMaterialDef::StaticStruct },
		{ TEXT("item"), &FGLItemDef::StaticStruct },
		{ TEXT("knowledge"), &FGLKnowledgeDef::StaticStruct },
		{ TEXT("recipe"), &FGLRecipeDef::StaticStruct },
		{ TEXT("salvage"), &FGLSalvageDef::StaticStruct },
		{ TEXT("buildpiece"), &FGLBuildPieceDef::StaticStruct },
		{ TEXT("capability"), &FGLCapabilityDef::StaticStruct },
		{ TEXT("glitch"), &FGLGlitchDef::StaticStruct },
		{ TEXT("cell"), &FGLCellDef::StaticStruct },
		{ TEXT("placement"), &FGLPlacementDef::StaticStruct },
		{ TEXT("exchange"), &FGLExchangeDef::StaticStruct },
		{ TEXT("puzzle"), &FGLPuzzleDef::StaticStruct },
		{ TEXT("terraform"), &FGLTerraformDef::StaticStruct },
		{ TEXT("creature"), &FGLCreatureDef::StaticStruct },
		{ TEXT("storm"), &FGLStormDef::StaticStruct },
		{ TEXT("structure"), &FGLStructureDef::StaticStruct },
		{ TEXT("tuning"), &FGLTuningDef::StaticStruct },
		{ TEXT("visual"), &FGLVisualDef::StaticStruct },
	};

	/** Top-level entries under Data/ that are not entity kinds. */
	bool IsSkipped(const FString& FirstSegment)
	{
		return FirstSegment == TEXT("_registry") || FirstSegment == TEXT("_aliases.json") || FirstSegment == TEXT("README.md");
	}

	TSharedPtr<FJsonObject> ReadJsonObject(const FString& Path, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutError = TEXT("unreadable file");
			return nullptr;
		}
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
		{
			OutError = FString::Printf(TEXT("unreadable JSON: %s"), *Reader->GetErrorMessage());
			return nullptr;
		}
		return Object;
	}

	/** The JSON key for a property: the property name with its first letter lower-cased. */
	FString JsonKeyFor(const FProperty* Property)
	{
		FString Key = Property->GetName();
		if (!Key.IsEmpty())
		{
			Key[0] = FChar::ToLower(Key[0]);
		}
		return Key;
	}

	const FProperty* FindPropertyForKey(const UStruct* Struct, const FString& Key)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (JsonKeyFor(*It).Equals(Key, ESearchCase::CaseSensitive))
			{
				return *It;
			}
		}
		return nullptr;
	}

	TSharedPtr<FJsonValue> FindField(const FJsonObject& Object, const FString& Key)
	{
		for (const auto& Pair : Object.Values)
		{
			if (FString(*Pair.Key).Equals(Key, ESearchCase::CaseSensitive))
			{
				return Pair.Value;
			}
		}
		return nullptr;
	}

	bool ValuesMatch(const TSharedPtr<FJsonValue>& Source, const TSharedPtr<FJsonValue>& Exported, const FString& Path, FString& OutMismatch);

	/** Every field of Source exists in Exported with an equal value (Exported may add defaulted fields). */
	bool ObjectSubsetMatches(const FJsonObject& Source, const FJsonObject& Exported, const FString& Path, FString& OutMismatch)
	{
		for (const auto& Pair : Source.Values)
		{
			const FString Key(*Pair.Key);
			const FString FieldPath = Path.IsEmpty() ? Key : Path + TEXT(".") + Key;
			const TSharedPtr<FJsonValue> Other = FindField(Exported, Key);
			if (!Other.IsValid())
			{
				OutMismatch = FString::Printf(TEXT("%s was dropped"), *FieldPath);
				return false;
			}
			if (!ValuesMatch(Pair.Value, Other, FieldPath, OutMismatch))
			{
				return false;
			}
		}
		return true;
	}

	bool ValuesMatch(const TSharedPtr<FJsonValue>& Source, const TSharedPtr<FJsonValue>& Exported, const FString& Path, FString& OutMismatch)
	{
		switch (Source->Type)
		{
		case EJson::Number:
			if (Exported->Type == EJson::Number && Exported->AsNumber() == Source->AsNumber())
			{
				return true;
			}
			break;
		case EJson::String:
			if (Exported->Type == EJson::String && Exported->AsString().Equals(Source->AsString(), ESearchCase::CaseSensitive))
			{
				return true;
			}
			break;
		case EJson::Boolean:
			if (Exported->Type == EJson::Boolean && Exported->AsBool() == Source->AsBool())
			{
				return true;
			}
			break;
		case EJson::Array:
		{
			if (Exported->Type != EJson::Array)
			{
				break;
			}
			const TArray<TSharedPtr<FJsonValue>>& A = Source->AsArray();
			const TArray<TSharedPtr<FJsonValue>>& B = Exported->AsArray();
			if (A.Num() != B.Num())
			{
				OutMismatch = FString::Printf(TEXT("%s has %d elements, re-exported %d"), *Path, A.Num(), B.Num());
				return false;
			}
			for (int32 Index = 0; Index < A.Num(); ++Index)
			{
				if (!ValuesMatch(A[Index], B[Index], FString::Printf(TEXT("%s[%d]"), *Path, Index), OutMismatch))
				{
					return false;
				}
			}
			return true;
		}
		case EJson::Object:
			if (Exported->Type == EJson::Object)
			{
				return ObjectSubsetMatches(*Source->AsObject(), *Exported->AsObject(), Path, OutMismatch);
			}
			break;
		default:
			break;
		}
		OutMismatch = FString::Printf(TEXT("%s changed on round trip"), *Path);
		return false;
	}
}

using namespace GLContentRegistry;

const UScriptStruct* FGLContentRegistry::StructForKind(FName Kind)
{
	for (const FKindStruct& Entry : KindStructs)
	{
		if (Kind == FName(Entry.Kind))
		{
			return Entry.Struct();
		}
	}
	return nullptr;
}

TArray<FName> FGLContentRegistry::TypedKinds()
{
	TArray<FName> Kinds;
	for (const FKindStruct& Entry : KindStructs)
	{
		Kinds.Add(FName(Entry.Kind));
	}
	return Kinds;
}

void FGLContentRegistry::FindUnknownKeys(const UStruct* Struct, const FJsonObject& Json, const FString& Path, TArray<FString>& OutPaths)
{
	for (const auto& Pair : Json.Values)
	{
		const FString Key(*Pair.Key);
		const FString KeyPath = Path.IsEmpty() ? Key : Path + TEXT(".") + Key;
		const FProperty* Property = FindPropertyForKey(Struct, Key);
		if (!Property)
		{
			OutPaths.Add(KeyPath);
			continue;
		}
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (Pair.Value->Type == EJson::Object)
			{
				FindUnknownKeys(StructProperty->Struct, *Pair.Value->AsObject(), KeyPath, OutPaths);
			}
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			const FStructProperty* Inner = CastField<FStructProperty>(ArrayProperty->Inner);
			if (Inner && Pair.Value->Type == EJson::Array)
			{
				for (const TSharedPtr<FJsonValue>& Element : Pair.Value->AsArray())
				{
					if (Element->Type == EJson::Object)
					{
						FindUnknownKeys(Inner->Struct, *Element->AsObject(), KeyPath + TEXT("[]"), OutPaths);
					}
				}
			}
		}
	}
}

void FGLContentRegistry::CollectKeyPaths(const UStruct* Struct, const FString& Prefix, TArray<FString>& OutPaths)
{
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		const FString Path = Prefix.IsEmpty() ? JsonKeyFor(*It) : Prefix + TEXT(".") + JsonKeyFor(*It);
		OutPaths.Add(Path);
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(*It))
		{
			CollectKeyPaths(StructProperty->Struct, Path, OutPaths);
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(*It))
		{
			if (const FStructProperty* Inner = CastField<FStructProperty>(ArrayProperty->Inner))
			{
				CollectKeyPaths(Inner->Struct, Path + TEXT("[]"), OutPaths);
			}
		}
		else if (CastField<FMapProperty>(*It))
		{
			OutPaths.Add(Path + TEXT("{}"));
		}
	}
}

bool FGLContentRegistry::SurvivesRoundTrip(const FJsonObject& Source, const UScriptStruct* Struct, const void* Memory, FString& OutMismatch)
{
	const TSharedRef<FJsonObject> Exported = MakeShared<FJsonObject>();
	if (!FJsonObjectConverter::UStructToJsonObject(Struct, Memory, Exported))
	{
		OutMismatch = TEXT("could not re-export the struct");
		return false;
	}
	return ObjectSubsetMatches(Source, *Exported, FString(), OutMismatch);
}

void FGLContentRegistry::ForEachEntry(TFunctionRef<void(const FGLContentEntry&)> Visit) const
{
	for (const TPair<FName, FGLContentEntry>& Pair : Entries)
	{
		Visit(Pair.Value);
	}
}

void FGLContentRegistry::AddProblem(const FString& Rule, const FString& File, const FString& Message)
{
	Problems.Add({ Rule, File, Message });
}

bool FGLContentRegistry::LoadRepository(const FString& RepoRoot)
{
	Entries.Reset();
	RegisteredKinds.Reset();
	Anchors.Reset();
	Problems.Reset();

	const FString DataDir = FPaths::Combine(RepoRoot, TEXT("Data"));
	const FString KindsFile = FPaths::Combine(DataDir, TEXT("_registry"), TEXT("kinds.json"));
	FString Error;
	const TSharedPtr<FJsonObject> KindsJson = ReadJsonObject(KindsFile, Error);
	const TArray<TSharedPtr<FJsonValue>>* KindsArray = nullptr;
	if (!KindsJson.IsValid() || !KindsJson->TryGetArrayField(TEXT("kinds"), KindsArray))
	{
		AddProblem(TEXT("ID-9"), TEXT("Data/_registry/kinds.json"), Error.IsEmpty() ? TEXT("missing 'kinds' list") : Error);
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Kind : *KindsArray)
	{
		RegisteredKinds.Add(FName(*Kind->AsString()));
	}

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *DataDir, TEXT("*.json"), true, false);
	Files.Sort();
	for (const FString& File : Files)
	{
		FString Relative = File;
		FPaths::MakePathRelativeTo(Relative, *(DataDir + TEXT("/")));
		TArray<FString> Parts;
		Relative.ParseIntoArray(Parts, TEXT("/"));
		if (Parts.Num() == 0 || IsSkipped(Parts[0]))
		{
			continue;
		}
		const FString RepoRelative = TEXT("Data/") + Relative;
		if (!RegisteredKinds.Contains(FName(*Parts[0])))
		{
			AddProblem(TEXT("ID-9"), RepoRelative, FString::Printf(TEXT("top-level folder '%s' is not a registered kind"), *Parts[0]));
			continue;
		}
		if (Parts[0] == TEXT("anchor"))
		{
			LoadAnchorFile(File, RepoRelative);
		}
		else
		{
			LoadEntityFile(DataDir, File, RepoRelative);
		}
	}

	for (const FGLContentProblem& Problem : Problems)
	{
		UE_LOG(LogGridlandsCore, Warning, TEXT("Content: %s"), *Problem.ToString());
	}
	UE_LOG(LogGridlandsCore, Log, TEXT("Content: loaded %d entities and %d anchors from %s with %d problem(s)"),
		Entries.Num(), Anchors.Num(), *DataDir, Problems.Num());
	return Problems.Num() == 0;
}

void FGLContentRegistry::LoadEntityFile(const FString& DataDir, const FString& AbsolutePath, const FString& RelativePath)
{
	FString Error;
	const TSharedPtr<FJsonObject> Json = ReadJsonObject(AbsolutePath, Error);
	if (!Json.IsValid())
	{
		AddProblem(TEXT("SCHEMA"), RelativePath, Error);
		return;
	}
	FString Id;
	if (!Json->TryGetStringField(TEXT("id"), Id) || !Json->HasField(TEXT("schemaVersion")))
	{
		AddProblem(TEXT("ID-8"), RelativePath, TEXT("entity needs 'schemaVersion' and 'id'"));
		return;
	}
	FString IdProblem;
	if (!GLContentId::IsValid(Id, &IdProblem))
	{
		AddProblem(TEXT("ID-1"), RelativePath, FString::Printf(TEXT("'%s': %s"), *Id, *IdProblem));
		return;
	}
	TArray<FString> Parts;
	RelativePath.ParseIntoArray(Parts, TEXT("/"));
	const FString FolderKind = Parts.Num() > 1 ? Parts[1] : FString();
	const FString Kind = GLContentId::KindOf(Id);
	if (Kind != FolderKind)
	{
		AddProblem(TEXT("ID-2"), RelativePath, FString::Printf(TEXT("id kind '%s' does not match folder kind '%s'"), *Kind, *FolderKind));
		return;
	}
	const FString Expected = TEXT("Data/") + Id.Replace(TEXT("."), TEXT("/")) + TEXT(".json");
	if (RelativePath != Expected)
	{
		AddProblem(TEXT("ID-4"), RelativePath, FString::Printf(TEXT("file path must mirror id: expected %s"), *Expected));
		return;
	}
	const FName IdName(*Id);
	if (Entries.Contains(IdName))
	{
		AddProblem(TEXT("ID-3"), RelativePath, FString::Printf(TEXT("duplicate id, also defined in %s"), *Entries[IdName].File));
		return;
	}
	const UScriptStruct* Struct = StructForKind(FName(*Kind));
	if (!Struct)
	{
		AddProblem(TEXT("SCHEMA"), RelativePath, FString::Printf(TEXT("kind '%s' has no typed definition yet"), *Kind));
		return;
	}
	TArray<FString> Unknown;
	FindUnknownKeys(Struct, *Json, FString(), Unknown);
	if (Unknown.Num() > 0)
	{
		AddProblem(TEXT("CXX-1"), RelativePath, FString::Printf(TEXT("no C++ field for: %s"), *FString::Join(Unknown, TEXT(", "))));
		return;
	}

	FGLContentEntry Entry;
	Entry.Id = IdName;
	Entry.Kind = FName(*Kind);
	Entry.File = RelativePath;
	Entry.Source = Json;
	Entry.Definition.InitializeAs(Struct);
	FText Failure;
	if (!FJsonObjectConverter::JsonObjectToUStruct(Json.ToSharedRef(), Struct, Entry.Definition.GetMutableMemory(), 0, 0, false, &Failure))
	{
		AddProblem(TEXT("CXX-2"), RelativePath, FString::Printf(TEXT("conversion failed: %s"), *Failure.ToString()));
		return;
	}
	Entries.Add(IdName, MoveTemp(Entry));
}

void FGLContentRegistry::LoadAnchorFile(const FString& AbsolutePath, const FString& RelativePath)
{
	const FString Name = FPaths::GetCleanFilename(RelativePath);
	if (!Name.EndsWith(TEXT(".generated.json")) || RelativePath.Len() - Name.Len() != FString(TEXT("Data/anchor/")).Len())
	{
		AddProblem(TEXT("ID-4"), RelativePath, TEXT("anchors live only in Data/anchor/<cell>.generated.json"));
		return;
	}
	FString Error;
	const TSharedPtr<FJsonObject> Json = ReadJsonObject(AbsolutePath, Error);
	const TArray<TSharedPtr<FJsonValue>>* AnchorValues = nullptr;
	if (!Json.IsValid() || !Json->TryGetArrayField(TEXT("anchors"), AnchorValues))
	{
		AddProblem(TEXT("SCHEMA"), RelativePath, Error.IsEmpty() ? TEXT("anchor file needs an 'anchors' list") : Error);
		return;
	}
	for (const TSharedPtr<FJsonValue>& Anchor : *AnchorValues)
	{
		FString Id;
		if (Anchor->Type == EJson::Object && Anchor->AsObject()->TryGetStringField(TEXT("id"), Id) && GLContentId::IsValid(Id))
		{
			const TSharedPtr<FJsonObject> Object = Anchor->AsObject();
			FGLAnchorRecord& Record = Anchors.Add(FName(*Id));
			Record.Id = FName(*Id);
			auto ReadVector = [](const TArray<TSharedPtr<FJsonValue>>* Values, FVector& Out)
			{
				if (Values && Values->Num() == 3)
				{
					Out = FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
				}
			};
			const TSharedPtr<FJsonObject>* Transform = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
			if (Object->TryGetObjectField(TEXT("transform"), Transform))
			{
				(*Transform)->TryGetArrayField(TEXT("location"), Values);
				ReadVector(Values, Record.Location);
				(*Transform)->TryGetNumberField(TEXT("yaw"), Record.Yaw);
			}
			if (Object->TryGetArrayField(TEXT("boundsExtent"), Values))
			{
				ReadVector(Values, Record.BoundsExtent);
			}
		}
		else
		{
			AddProblem(TEXT("ID-1"), RelativePath, TEXT("anchor without a valid id"));
		}
	}
}
