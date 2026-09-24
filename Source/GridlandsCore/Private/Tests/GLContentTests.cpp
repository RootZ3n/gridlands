#include "Content/GLContentDefinitions.h"
#include "Content/GLContentId.h"
#include "Content/GLContentRegistry.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

// ADR-0021: the engine loads Data/ JSON directly. These tests prove the C++ loader agrees with
// the Python validator (Tools/gldata) and loses nothing on the way in.

namespace GLContentTests
{
	constexpr EAutomationTestFlags ContentTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	FString RepoRoot() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); }

	TSharedPtr<FJsonObject> ReadJson(const FString& Path)
	{
		FString Text;
		TSharedPtr<FJsonObject> Object;
		if (FFileHelper::LoadFileToString(Text, *Path))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object);
		}
		return Object;
	}

	/** A throwaway repository root holding a copy of the registry plus the given entity files. */
	FString MakeSandbox(const TMap<FString, FString>& Files)
	{
		const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ContentTestSandbox"), FGuid::NewGuid().ToString());
		IFileManager::Get().Copy(*FPaths::Combine(Root, TEXT("Data/_registry/kinds.json")), *FPaths::Combine(RepoRoot(), TEXT("Data/_registry/kinds.json")));
		for (const TPair<FString, FString>& File : Files)
		{
			FFileHelper::SaveStringToFile(File.Value, *FPaths::Combine(Root, File.Key));
		}
		return Root;
	}

	bool HasRule(const FGLContentRegistry& Registry, const TCHAR* Rule)
	{
		return Registry.GetProblems().ContainsByPredicate([Rule](const FGLContentProblem& P) { return P.Rule == Rule; });
	}

	const TCHAR* FuseJson = TEXT(R"({"schemaVersion":1,"id":"item.part.fuse","displayName":"Fuse","stackSize":10,"weight":0.05,"criticalPath":true,"sources":["Source.Salvage"]})");
}

using namespace GLContentTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLContentIdCorpus, "Gridlands.Core.Content.IdGrammarMatchesCorpus", ContentTestFlags)
bool FGLContentIdCorpus::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Corpus = ReadJson(FPaths::Combine(RepoRoot(), TEXT("Tools/tests/id-corpus.json")));
	if (!TestTrue(TEXT("shared corpus is readable"), Corpus.IsValid()))
	{
		return false;
	}
	int32 Checked = 0;
	for (const TSharedPtr<FJsonValue>& Value : Corpus->GetArrayField(TEXT("valid")))
	{
		FString Problem;
		TestTrue(FString::Printf(TEXT("valid: '%s' (%s)"), *Value->AsString(), *Problem), GLContentId::IsValid(Value->AsString(), &Problem));
		++Checked;
	}
	for (const TSharedPtr<FJsonValue>& Value : Corpus->GetArrayField(TEXT("invalid")))
	{
		TestFalse(FString::Printf(TEXT("invalid: '%s'"), *Value->AsString()), GLContentId::IsValid(Value->AsString()));
		++Checked;
	}
	TestTrue(TEXT("corpus is not empty"), Checked >= 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLContentRepositoryLoads, "Gridlands.Core.Content.RepositoryDataLoadsClean", ContentTestFlags)
bool FGLContentRepositoryLoads::RunTest(const FString& Parameters)
{
	FGLContentRegistry Registry;
	const bool bClean = Registry.LoadRepository(RepoRoot());
	for (const FGLContentProblem& Problem : Registry.GetProblems())
	{
		AddError(Problem.ToString());
	}
	TestTrue(TEXT("no load problems"), bClean);

	// Count entity files independently of the loader, so a silently skipped file fails the test.
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *FPaths::Combine(RepoRoot(), TEXT("Data")), TEXT("*.json"), true, false);
	const int32 EntityFiles = Files.FilterByPredicate([](const FString& F)
	{
		return !F.Contains(TEXT("/Data/_registry/")) && !F.EndsWith(TEXT("/_aliases.json")) && !F.Contains(TEXT("/Data/anchor/"));
	}).Num();
	TestEqual(TEXT("every entity file became an entity"), Registry.Num(), EntityFiles);
	TestTrue(TEXT("the repository has content"), Registry.Num() >= 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLContentRoundTrip, "Gridlands.Core.Content.RoundTripKeepsEveryField", ContentTestFlags)
bool FGLContentRoundTrip::RunTest(const FString& Parameters)
{
	FGLContentRegistry Registry;
	Registry.LoadRepository(RepoRoot());
	int32 Checked = 0;
	Registry.ForEachEntry([this, &Checked](const FGLContentEntry& Entry)
	{
		FString Mismatch;
		const bool bSurvived = FGLContentRegistry::SurvivesRoundTrip(*Entry.Source, Entry.Definition.GetScriptStruct(), Entry.Definition.GetMemory(), Mismatch);
		TestTrue(FString::Printf(TEXT("%s: %s"), *Entry.File, *Mismatch), bSurvived);
		++Checked;
	});
	TestEqual(TEXT("every entity was round-tripped"), Checked, Registry.Num());

	// The check itself must be able to fail: a field the struct cannot hold is reported as dropped.
	const TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetStringField(TEXT("displayName"), TEXT("Fuse"));
	Extra->SetStringField(TEXT("noSuchField"), TEXT("x"));
	FGLItemDef Item;
	Item.DisplayName = TEXT("Fuse");
	FString Mismatch;
	TestFalse(TEXT("an unrepresentable field is detected"), FGLContentRegistry::SurvivesRoundTrip(*Extra, FGLItemDef::StaticStruct(), &Item, Mismatch));
	TestTrue(TEXT("the mismatch names the field"), Mismatch.Contains(TEXT("noSuchField")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLContentSchemaKeys, "Gridlands.Core.Content.SchemaKeysMatchValidator", ContentTestFlags)
bool FGLContentSchemaKeys::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Export = ReadJson(FPaths::Combine(RepoRoot(), TEXT("Data/_registry/schema.generated.json")));
	if (!TestTrue(TEXT("schema export is readable (run Tools/data.sh generate)"), Export.IsValid()))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Kinds = Export->GetObjectField(TEXT("kinds"));
	TSet<FName> PythonKinds;
	for (const auto& Pair : Kinds->Values)
	{
		const FName Kind(*FString(*Pair.Key));
		PythonKinds.Add(Kind);
		const UScriptStruct* Struct = FGLContentRegistry::StructForKind(Kind);
		if (!TestNotNull(FString::Printf(TEXT("C++ definition for kind '%s'"), *Kind.ToString()), Struct))
		{
			continue;
		}
		TSet<FString> PythonPaths;
		for (const TSharedPtr<FJsonValue>& Path : Pair.Value->AsArray())
		{
			PythonPaths.Add(Path->AsString());
		}
		TArray<FString> CppPathList;
		FGLContentRegistry::CollectKeyPaths(Struct, FString(), CppPathList);
		const TSet<FString> CppPaths(CppPathList);
		for (const FString& Path : PythonPaths.Difference(CppPaths).Array())
		{
			AddError(FString::Printf(TEXT("%s: validator allows '%s' but C++ has no field for it"), *Kind.ToString(), *Path));
		}
		for (const FString& Path : CppPaths.Difference(PythonPaths).Array())
		{
			AddError(FString::Printf(TEXT("%s: C++ has '%s' but the validator does not allow it"), *Kind.ToString(), *Path));
		}
	}
	for (const FName Kind : FGLContentRegistry::TypedKinds())
	{
		TestTrue(FString::Printf(TEXT("validator has a schema for C++ kind '%s'"), *Kind.ToString()), PythonKinds.Contains(Kind));
	}
	TestTrue(TEXT("kinds were compared"), PythonKinds.Num() >= 15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLContentRejects, "Gridlands.Core.Content.LoaderRejectsMalformedEntities", ContentTestFlags)
bool FGLContentRejects::RunTest(const FString& Parameters)
{
	// Each rejection below is logged by the loader; declaring them makes the log part of the assertion.
	for (const TCHAR* Rule : { TEXT("Content: CXX-1"), TEXT("Content: ID-4"), TEXT("Content: ID-1"), TEXT("Content: ID-9") })
	{
		AddExpectedMessagePlain(Rule, ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	}
	{
		FGLContentRegistry Registry;
		Registry.LoadRepository(MakeSandbox({ { TEXT("Data/item/part/fuse.json"), FuseJson } }));
		TestEqual(TEXT("a well-formed entity loads"), Registry.Num(), 1);
		TestFalse(TEXT("with no problems"), Registry.GetProblems().Num() > 0);
	}
	{
		FGLContentRegistry Registry;
		FString Json = FuseJson;
		Json.ReplaceInline(TEXT("\"weight\":0.05"), TEXT("\"weight\":0.05,\"damage\":5"));
		Registry.LoadRepository(MakeSandbox({ { TEXT("Data/item/part/fuse.json"), Json } }));
		TestTrue(TEXT("unknown field is rejected (CXX-1)"), HasRule(Registry, TEXT("CXX-1")));
		TestEqual(TEXT("and not loaded"), Registry.Num(), 0);
	}
	{
		FGLContentRegistry Registry;
		Registry.LoadRepository(MakeSandbox({ { TEXT("Data/item/material/fuse.json"), FuseJson } }));
		TestTrue(TEXT("path must mirror id (ID-4)"), HasRule(Registry, TEXT("ID-4")));
	}
	{
		FGLContentRegistry Registry;
		FString Json = FuseJson;
		Json.ReplaceInline(TEXT("item.part.fuse"), TEXT("item.part.Fuse"));
		Registry.LoadRepository(MakeSandbox({ { TEXT("Data/item/part/fuse.json"), Json } }));
		TestTrue(TEXT("bad id grammar (ID-1)"), HasRule(Registry, TEXT("ID-1")));
	}
	{
		FGLContentRegistry Registry;
		Registry.LoadRepository(MakeSandbox({ { TEXT("Data/spell/fire/ball.json"), TEXT(R"({"schemaVersion":1,"id":"spell.fire.ball"})") } }));
		TestTrue(TEXT("unregistered kind (ID-9)"), HasRule(Registry, TEXT("ID-9")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLContentTypedLookup, "Gridlands.Core.Content.TypedLookupReadsAuthoredValues", ContentTestFlags)
bool FGLContentTypedLookup::RunTest(const FString& Parameters)
{
	FGLContentRegistry Registry;
	Registry.LoadRepository(RepoRoot());

	const FGLItemDef* PryBar = Registry.Find<FGLItemDef>(TEXT("item.tool.pry_bar"));
	if (TestNotNull(TEXT("pry bar"), PryBar))
	{
		TestTrue(TEXT("pry bar is a tool"), PryBar->IsTool());
		TestEqual(TEXT("tool class"), PryBar->Tool.ToolClass, FName(TEXT("Tool.Pry")));
		TestEqual(TEXT("tier"), PryBar->Tool.Tier, 1);
	}
	TestNull(TEXT("kind-checked lookup refuses the wrong type"), Registry.Find<FGLRecipeDef>(TEXT("item.tool.pry_bar")));

	const FGLGlitchDef* Lamp = Registry.Find<FGLGlitchDef>(TEXT("glitch.home.flicker_lamp"));
	if (TestNotNull(TEXT("glitch"), Lamp))
	{
		TestEqual(TEXT("repair seconds"), Lamp->Repair.Seconds, 5.0);
		TestEqual(TEXT("interrupt policy enum"), Lamp->Repair.InterruptPolicy, EGLInterruptPolicy::KeepProgress);
		TestEqual(TEXT("one requirement"), Lamp->Requirements.Num(), 1);
	}
	const FGLPlacementDef* Placement = Registry.Find<FGLPlacementDef>(TEXT("placement.origin.glitch_flicker_lamp"));
	if (TestNotNull(TEXT("placement"), Placement))
	{
		const FName* Blocker = Placement->Bindings.Find(TEXT("blocker"));
		TestTrue(TEXT("binding map loaded"), Blocker && *Blocker == FName(TEXT("placement.origin.junk_pile_01")));
	}
	const FGLCellDef* Origin = Registry.Find<FGLCellDef>(TEXT("cell.home.origin"));
	if (TestNotNull(TEXT("origin cell"), Origin))
	{
		TestEqual(TEXT("modern-day dominant era weight"), Origin->EraComposition[0].Weight, 0.85);
	}
	const FGLExchangeDef* Died = Registry.Find<FGLExchangeDef>(TEXT("exchange.player.died_again"));
	if (TestNotNull(TEXT("exchange"), Died))
	{
		TestEqual(TEXT("category enum"), Died->Category, EGLExchangeCategory::Contextual);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
