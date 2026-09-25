#include "Save/GLWorldSave.h"

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace GLSaveCodec
{
	FString ToJson(const FGLWorldSave& Save)
	{
		FGLWorldSave Copy = Save;
		Copy.SchemaVersion = FGLWorldSave::CurrentVersion;
		for (FGLSavedGlitch& Glitch : Copy.Glitches)
		{
			Glitch.State = FGLGlitchLifecycle::ToPersistedState(Glitch.State); // never save Repairing
		}
		FString Text;
		FJsonObjectConverter::UStructToJsonObjectString(Copy, Text);
		return Text;
	}

	bool FromJson(const FString& Text, FGLWorldSave& OutSave, FString& OutProblem)
	{
		TSharedPtr<FJsonObject> Object;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) || !Object.IsValid())
		{
			OutProblem = TEXT("unreadable save");
			return false;
		}
		int32 Version = 0;
		if (!Object->TryGetNumberField(TEXT("schemaVersion"), Version))
		{
			OutProblem = TEXT("save has no schemaVersion");
			return false;
		}
		if (Version > FGLWorldSave::CurrentVersion)
		{
			OutProblem = FString::Printf(TEXT("save version %d is newer than this build (%d)"), Version, FGLWorldSave::CurrentVersion);
			return false;
		}
		// Migrations go here, oldest first: if (Version == 1) { ...; Version = 2; }
		FGLWorldSave Parsed;
		if (!FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(), &Parsed))
		{
			OutProblem = TEXT("save does not match the save format");
			return false;
		}
		for (FGLSavedGlitch& Glitch : Parsed.Glitches)
		{
			Glitch.State = FGLGlitchLifecycle::ToPersistedState(Glitch.State);
		}
		Parsed.SchemaVersion = FGLWorldSave::CurrentVersion;
		OutSave = MoveTemp(Parsed);
		return true;
	}
}
