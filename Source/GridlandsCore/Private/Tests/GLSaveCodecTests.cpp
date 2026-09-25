#include "Misc/AutomationTest.h"
#include "Save/GLWorldSave.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLSaveCodec, "Gridlands.Core.Save.CodecRoundTripAndVersions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGLSaveCodec::RunTest(const FString& Parameters)
{
	FGLWorldSave Save;
	Save.Cell = TEXT("cell.home.origin");
	Save.SettingsPreset = TEXT("settings.preset.relaxed");
	Save.Glitches.Add({ TEXT("placement.origin.a"), EGLGlitchState::Repaired, 5.0, false });
	Save.Glitches.Add({ TEXT("placement.origin.b"), EGLGlitchState::Repairing, 2.5, true });
	Save.SalvagedPlacements.Add(TEXT("placement.origin.junk_pile_01"));
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
	TestEqual(TEXT("glitch count"), Back.Glitches.Num(), 2);
	TestEqual(TEXT("repaired stays repaired"), Back.Glitches[0].State, EGLGlitchState::Repaired);
	TestEqual(TEXT("repairing comes back as interrupted"), Back.Glitches[1].State, EGLGlitchState::Interrupted);
	TestEqual(TEXT("progress kept"), Back.Glitches[1].ProgressSeconds, 2.5);
	TestTrue(TEXT("delivered flag kept"), Back.Glitches[1].ItemsDelivered);
	TestEqual(TEXT("capability"), Back.PehlichiCapabilities[0].Count, 2);
	TestEqual(TEXT("dialogue history"), Back.EventCounts[0].Count, 3);
	TestTrue(TEXT("transform"), Back.Zenny.Location.Equals(FVector(1, 2, 3)) && Back.Zenny.Yaw == 90.0);
	TestEqual(TEXT("encoding is stable"), GLSaveCodec::ToJson(Back), Json);

	TestFalse(TEXT("a newer save is refused"), GLSaveCodec::FromJson(Json.Replace(TEXT("\"schemaVersion\": 1"), TEXT("\"schemaVersion\": 99")), Back, Problem));
	TestTrue(TEXT("and says why"), Problem.Contains(TEXT("newer")));
	TestFalse(TEXT("a save without a version is refused"), GLSaveCodec::FromJson(TEXT("{\"cell\":\"cell.home.origin\"}"), Back, Problem));
	TestFalse(TEXT("garbage is refused"), GLSaveCodec::FromJson(TEXT("{not json"), Back, Problem));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
