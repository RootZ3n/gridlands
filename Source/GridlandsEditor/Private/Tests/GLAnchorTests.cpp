#include "Anchors/GLAnchorExport.h"
#include "Content/GLContentDefinitions.h"
#include "Content/GLContentRegistry.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "World/GLAnchorComponent.h"
#include "World/GLGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLAnchorTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	FString RepoRoot() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); }
}

using namespace GLAnchorTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLAnchorsInSync, "Gridlands.Editor.Anchors.ExportMatchesCommitted", Flags)
bool FGLAnchorsInSync::RunTest(const FString& Parameters)
{
	FGLContentRegistry Registry;
	Registry.LoadRepository(RepoRoot());
	int32 Cells = 0;
	Registry.ForEachEntry([&](const FGLContentEntry& Entry)
	{
		const FGLCellDef* Cell = Entry.Definition.GetPtr<FGLCellDef>();
		if (Entry.Kind != TEXT("cell") || !Cell || Cell->Level.IsEmpty())
		{
			return;
		}
		++Cells;
		UWorld* World = GLAnchorExport::LoadMap(Cell->Level);
		if (!TestNotNull(FString::Printf(TEXT("%s level %s loads"), *Entry.Id.ToString(), *Cell->Level), World))
		{
			return;
		}
		TArray<FString> Problems;
		const FString Fresh = GLAnchorExport::Render(World, Entry.Id.ToString(), Problems);
		for (const FString& Problem : Problems)
		{
			AddError(Problem);
		}
		FString Committed;
		FFileHelper::LoadFileToString(Committed, *FPaths::Combine(RepoRoot(), GLAnchorExport::FilePathForCell(Entry.Id.ToString())));
		TestTrue(FString::Printf(TEXT("%s is up to date (run Tools/export-anchors.sh)"), *GLAnchorExport::FilePathForCell(Entry.Id.ToString())), Fresh == Committed);
	});
	TestTrue(TEXT("at least one cell has a level"), Cells >= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLOriginBrief, "Gridlands.Editor.Blockout.OriginMatchesBrief", Flags)
bool FGLOriginBrief::RunTest(const FString& Parameters)
{
	UWorld* World = GLAnchorExport::LoadMap(TEXT("/Game/Gridlands/Maps/L_Origin"));
	if (!TestNotNull(TEXT("L_Origin loads"), World))
	{
		return false;
	}
	TMap<FString, const AActor*> Anchored;
	int32 PlayerStarts = 0;
	for (const AActor* Actor : World->PersistentLevel->Actors)
	{
		if (!Actor)
		{
			continue;
		}
		PlayerStarts += Actor->IsA<APlayerStart>() ? 1 : 0;
		TestFalse(TEXT("no static ground box: the runtime heightfield is the ground (ADR-0022)"), Actor->GetActorLabel() == TEXT("Ground"));
		if (const UGLAnchorComponent* Anchor = Actor->FindComponentByClass<UGLAnchorComponent>())
		{
			Anchored.Add(Anchor->AnchorId.ToString(), Actor);
		}
	}
	// E8: dominant modern-day suburbia, one conspicuous 1950s fragment, one smaller Roman fragment, one storm drain.
	int32 Houses = 0;
	for (const auto& Pair : Anchored)
	{
		Houses += Pair.Key.StartsWith(TEXT("anchor.origin.house_")) ? 1 : 0;
	}
	TestTrue(TEXT("modern houses dominate (at least 8)"), Houses >= 8);
	TestTrue(TEXT("one 1950s fragment"), Anchored.Contains(TEXT("anchor.origin.fragment_fifties_diner")));
	TestTrue(TEXT("one Roman fragment"), Anchored.Contains(TEXT("anchor.origin.fragment_roman_columns")));
	TestTrue(TEXT("one storm-drain entrance"), Anchored.Contains(TEXT("anchor.origin.storm_drain_entrance")));
	TestEqual(TEXT("exactly one player start"), PlayerStarts, 1);
	TestTrue(TEXT("the world uses the Gridlands game mode"), World->GetWorldSettings()->DefaultGameMode == AGLGameMode::StaticClass());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
