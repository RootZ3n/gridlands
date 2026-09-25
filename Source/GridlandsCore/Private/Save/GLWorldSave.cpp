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
		for (FGLSavedCell& Cell : Copy.Cells)
		{
			for (FGLSavedGlitch& Glitch : Cell.Glitches)
			{
				Glitch.State = FGLGlitchLifecycle::ToPersistedState(Glitch.State);
			}
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
		FGLWorldSave Parsed;
		if (!FJsonObjectConverter::JsonObjectToUStruct(Object.ToSharedRef(), &Parsed))
		{
			OutProblem = TEXT("save does not match the save format");
			return false;
		}
		// Migrations, oldest first.
		if (Version == 1)
		{
			// v1 held one cell's state in flat fields: they become that cell's record.
			FGLSavedCell Only;
			Only.Cell = Parsed.Cell;
			Only.Glitches = MoveTemp(Parsed.Glitches);
			Only.SalvagedPlacements = MoveTemp(Parsed.SalvagedPlacements);
			Only.DefeatedCreatures = MoveTemp(Parsed.DefeatedCreatures);
			Only.BuildPieces = MoveTemp(Parsed.BuildPieces);
			Only.TerrainIndices = MoveTemp(Parsed.TerrainIndices);
			Only.TerrainDeltaCm = MoveTemp(Parsed.TerrainDeltaCm);
			if (!Only.IsEmpty())
			{
				Parsed.Cells.Add(MoveTemp(Only));
			}
			Parsed.Glitches.Reset();
			Parsed.SalvagedPlacements.Reset();
			Parsed.DefeatedCreatures.Reset();
			Parsed.BuildPieces.Reset();
			Parsed.TerrainIndices.Reset();
			Parsed.TerrainDeltaCm.Reset();
			Version = 2;
		}
		for (FGLSavedCell& Cell : Parsed.Cells)
		{
			for (FGLSavedGlitch& Glitch : Cell.Glitches)
			{
				Glitch.State = FGLGlitchLifecycle::ToPersistedState(Glitch.State);
			}
		}
		Parsed.SchemaVersion = FGLWorldSave::CurrentVersion;
		OutSave = MoveTemp(Parsed);
		return true;
	}
}
