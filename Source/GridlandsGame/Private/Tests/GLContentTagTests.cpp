#include "Content/GLContentDefinitions.h"
#include "Content/GLContentSubsystem.h"
#include "Engine/Engine.h"
#include "GameplayTagsManager.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GLContentTagTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLContentTagsRegistered, "Gridlands.Game.Content.ConfigTagsAreRegisteredWithEngine", GLContentTagTests::TestFlags)
bool FGLContentTagsRegistered::RunTest(const FString& Parameters)
{
	// Config/Tags/*.ini are the declared vocabulary (TAG-3). Prove Unreal actually registered every tag.
	TArray<FString> Files;
	const FString TagsDir = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Tags"));
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(TagsDir, TEXT("*.ini")), true, false);
	int32 Checked = 0;
	for (const FString& File : Files)
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(TagsDir, File));
		for (const FString& Line : Lines)
		{
			FString Tag;
			if (Line.StartsWith(TEXT("GameplayTagList=(Tag=\"")) && Line.Split(TEXT("Tag=\""), nullptr, &Tag))
			{
				Tag.Split(TEXT("\""), &Tag, nullptr);
				const FGameplayTag Found = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Tag), false);
				TestTrue(FString::Printf(TEXT("%s (%s) is registered"), *Tag, *File), Found.IsValid());
				++Checked;
			}
		}
	}
	TestTrue(TEXT("tag files were read"), Files.Num() >= 2);
	TestTrue(TEXT("tags were checked"), Checked >= 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGLContentSubsystemLoaded, "Gridlands.Game.Content.EngineSubsystemLoadsContent", GLContentTagTests::TestFlags)
bool FGLContentSubsystemLoaded::RunTest(const FString& Parameters)
{
	const UGLContentSubsystem* Content = GEngine ? GEngine->GetEngineSubsystem<UGLContentSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("content subsystem exists"), Content))
	{
		return false;
	}
	TestEqual(TEXT("loaded without problems"), Content->GetRegistry().GetProblems().Num(), 0);
	TestNotNull(TEXT("pry bar reachable through the subsystem"), Content->Find<FGLItemDef>(TEXT("item.tool.pry_bar")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
