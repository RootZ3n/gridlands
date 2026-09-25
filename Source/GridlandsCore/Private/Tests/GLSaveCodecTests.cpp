#include "Misc/AutomationTest.h"
#include "Save/GLWorldSave.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSaveCodec, "Gridlands.Core.Save.CodecRoundTripAndVersions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGLSaveCodec::RunTest(const FString& Parameters)
{
	FGLWorldSave Save;
	Save.Cell = TEXT("cell.home.origin");
	Save.SettingsPreset = TEXT("settings.preset.relaxed");
	FGLSavedCell& Origin = Save.Cells.AddDefaulted_GetRef();
	Origin.Cell = TEXT("cell.home.origin");
	Origin.Glitches.Add({ TEXT("placement.origin.a"), EGLGlitchState::Repaired, 5.0, false });
	Origin.Glitches.Add({ TEXT("placement.origin.b"), EGLGlitchState::Repairing, 2.5, true });
	Origin.SalvagedPlacements.Add(TEXT("placement.origin.junk_pile_01"));
	FGLSavedCell& Lots = Save.Cells.AddDefaulted_GetRef();
	Lots.Cell = TEXT("cell.outer.diner_lots");
	Lots.TerrainIndices = { 7, 9 };
	Lots.TerrainDeltaCm = { -50, 25 };
	Save.Inventory.Add({ TEXT("item.tool.pry_bar"), 1 });
	Save.Knowledge.Add(TEXT("knowledge.material.copper_wire"));
	Save.PehlichiCapabilities.Add({ TEXT("capability.pehlichi.scan"), 2 });
	Save.ExchangeUses.Add({ TEXT("exchange.story.opening"), 1 });
	Save.EventCounts.Add({ TEXT("Event.Player.Died"), 3 });
	Save.Zenny.Location = FVector(1, 2, 3);
	Save.Zenny.Yaw = 90.0;

	const FString Json = GLSaveCodec::ToJson(Save);
	TestTrue(TEXT("human-readable JSON with a version"), Json.Contains(TEXT("\"schemaVersion\"")) && Json.Contains(TEXT("placement.origin.a")));
	TestFalse(TEXT("Repairing is never written (S-1 / lifecycle)"), Json.Contains(TEXT("\"Repairing\"")));

	FGLWorldSave Back;
	FString Problem;
	TestTrue(TEXT("round trip parses"), GLSaveCodec::FromJson(Json, Back, Problem));
	TestEqual(TEXT("two cell records"), Back.Cells.Num(), 2);
	const FGLSavedCell* O = Back.FindCell(TEXT("cell.home.origin"));
	if (!TestNotNull(TEXT("origin record"), O))
	{
		return false;
	}
	TestEqual(TEXT("glitch count"), O->Glitches.Num(), 2);
	TestEqual(TEXT("repaired stays repaired"), O->Glitches[0].State, EGLGlitchState::Repaired);
	TestEqual(TEXT("repairing comes back as interrupted"), O->Glitches[1].State, EGLGlitchState::Interrupted);
	TestEqual(TEXT("progress kept"), O->Glitches[1].ProgressSeconds, 2.5);
	TestTrue(TEXT("delivered flag kept"), O->Glitches[1].ItemsDelivered);
	TestEqual(TEXT("the other cell's ground kept"), Back.FindCell(TEXT("cell.outer.diner_lots"))->TerrainDeltaCm, TArray<int32>{ -50, 25 });
	TestEqual(TEXT("capability"), Back.PehlichiCapabilities[0].Count, 2);
	TestEqual(TEXT("dialogue history"), Back.EventCounts[0].Count, 3);
	TestTrue(TEXT("transform"), Back.Zenny.Location.Equals(FVector(1, 2, 3)) && Back.Zenny.Yaw == 90.0);
	TestEqual(TEXT("encoding is stable"), GLSaveCodec::ToJson(Back), Json);

	TestFalse(TEXT("a newer save is refused"), GLSaveCodec::FromJson(Json.Replace(TEXT("\"schemaVersion\": 2"), TEXT("\"schemaVersion\": 99")), Back, Problem));
	TestTrue(TEXT("and says why"), Problem.Contains(TEXT("newer")));
	TestFalse(TEXT("a save without a version is refused"), GLSaveCodec::FromJson(TEXT("{\"cell\":\"cell.home.origin\"}"), Back, Problem));
	TestFalse(TEXT("garbage is refused"), GLSaveCodec::FromJson(TEXT("{not json"), Back, Problem));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSaveMigration, "Gridlands.Core.Save.Version1MigratesToCellRecords", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGLSaveMigration::RunTest(const FString& Parameters)
{
	// A v1 world (M9-M11): one cell's state in flat fields.
	const FString V1 = TEXT("{\"schemaVersion\": 1, \"cell\": \"cell.home.origin\", ")
		TEXT("\"glitches\": [{\"placement\": \"placement.origin.glitch_flicker_lamp\", \"state\": \"Repairing\", \"progressSeconds\": 3.0, \"itemsDelivered\": false}], ")
		TEXT("\"salvagedPlacements\": [\"placement.origin.junk_pile_01\"], \"defeatedCreatures\": [\"placement.origin.drain_gremlin_den\"], ")
		TEXT("\"buildPieces\": [{\"id\": 1, \"def\": \"buildpiece.modern.timber_foundation\", \"location\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"yawQuarter\": 0}], ")
		TEXT("\"terrainIndices\": [5], \"terrainDeltaCm\": [-40], \"inventory\": [{\"id\": \"item.tool.pry_bar\", \"count\": 1}]}");
	FGLWorldSave Migrated;
	FString Problem;
	TestTrue(FString::Printf(TEXT("a v1 save still loads (%s)"), *Problem), GLSaveCodec::FromJson(V1, Migrated, Problem));
	TestEqual(TEXT("becomes the current version"), Migrated.SchemaVersion, FGLWorldSave::CurrentVersion);
	TestEqual(TEXT("one cell record"), Migrated.Cells.Num(), 1);
	const FGLSavedCell* Origin = Migrated.FindCell(TEXT("cell.home.origin"));
	if (!TestNotNull(TEXT("for the saved cell"), Origin))
	{
		return false;
	}
	TestEqual(TEXT("its glitch"), Origin->Glitches.Num(), 1);
	TestEqual(TEXT("mid-repair still comes back Interrupted"), Origin->Glitches[0].State, EGLGlitchState::Interrupted);
	TestEqual(TEXT("salvage"), Origin->SalvagedPlacements.Num(), 1);
	TestEqual(TEXT("defeated creature"), Origin->DefeatedCreatures.Num(), 1);
	TestEqual(TEXT("piece"), Origin->BuildPieces.Num(), 1);
	TestEqual(TEXT("ground"), Origin->TerrainDeltaCm, TArray<int32>{ -40 });
	TestEqual(TEXT("flat fields emptied"), Migrated.Glitches.Num() + Migrated.BuildPieces.Num() + Migrated.TerrainIndices.Num(), 0);
	TestEqual(TEXT("global state untouched"), Migrated.Inventory.Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
