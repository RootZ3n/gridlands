#include "Save/GLWorldSave.h"

#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	/**
	 * v2 -> v3 (P11, ADR-0039), on the JSON before conversion: every saved build piece (per cell, and v1's flat list)
	 * becomes a player-origin v3 piece. yawQuarter q -> yawStep 36 q; a v0 piece id gains its data's legacyLayers, so a
	 * finished-looking v0 wall becomes the same wall as FRAME + FINISH. Nothing else in the file changes.
	 */
	void MigratePiecesToV3(const TSharedPtr<FJsonObject>& Holder, const FGLContentRegistry* Content)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pieces = nullptr;
		if (!Holder.IsValid() || !Holder->TryGetArrayField(TEXT("buildPieces"), Pieces))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Pieces)
		{
			const TSharedPtr<FJsonObject> Piece = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Piece.IsValid())
			{
				continue;
			}
			int32 Quarter = 0;
			Piece->TryGetNumberField(TEXT("yawQuarter"), Quarter);
			Piece->RemoveField(TEXT("yawQuarter"));
			Piece->SetNumberField(TEXT("yawStep"), ((Quarter % 4 + 4) % 4) * 36);
			Piece->SetNumberField(TEXT("origin"), 1); // every v2 build piece was player-built
			TArray<TSharedPtr<FJsonValue>> Layers;
			FString Def;
			if (Content && Piece->TryGetStringField(TEXT("def"), Def))
			{
				if (const FGLBuildPieceDef* Form = Content->Find<FGLBuildPieceDef>(FName(*Def)))
				{
					for (const FName& Layer : Form->LegacyLayers)
					{
						Layers.Add(MakeShared<FJsonValueString>(Layer.ToString()));
					}
				}
			}
			Piece->SetArrayField(TEXT("layers"), Layers);
		}
	}
}

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

	bool FromJson(const FString& Text, FGLWorldSave& OutSave, FString& OutProblem, const FGLContentRegistry* Content)
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
		if (Version <= 2)
		{
			MigratePiecesToV3(Object, Content);
			const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
			if (Object->TryGetArrayField(TEXT("cells"), Cells))
			{
				for (const TSharedPtr<FJsonValue>& Cell : *Cells)
				{
					MigratePiecesToV3(Cell.IsValid() ? Cell->AsObject() : nullptr, Content);
				}
			}
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
