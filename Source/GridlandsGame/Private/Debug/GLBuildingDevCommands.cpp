// P12 (ADR-0040) DEV ONLY: the operator's daily-driver session tests BUILDING, not gathering. Never in a shipping build.

#include "Content/GLContent.h"
#include "Content/GLContentDefinitions.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GridlandsGame.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GLInventoryComponent.h"
#include "Knowledge/GLKnowledgeSubsystem.h"

#if !UE_BUILD_SHIPPING

namespace GLBuildingDevCommands
{
	/** The building knowledge every buildable piece and finish needs, and a kit of construction materials. */
	int32 GiveStarterKit(UWorld* World, AActor* Zenny, FString& OutReport)
	{
		UGLKnowledgeSubsystem* Knowledge = World ? World->GetSubsystem<UGLKnowledgeSubsystem>() : nullptr;
		UGLInventoryComponent* Inventory = Zenny ? Zenny->FindComponentByClass<UGLInventoryComponent>() : nullptr;
		if (!Knowledge || !Inventory)
		{
			OutReport = TEXT("no player to equip");
			return 0;
		}
		TSet<FName> Needed;
		GLContent::Get().ForEachEntry([&Needed](const FGLContentEntry& Entry)
		{
			if (const FGLBuildPieceDef* Piece = Entry.Definition.GetPtr<FGLBuildPieceDef>(); Piece && Piece->Buildable)
			{
				Needed.Append(Piece->UnlockedBy);
			}
			if (const FGLFinishDef* Finish = Entry.Definition.GetPtr<FGLFinishDef>())
			{
				Needed.Append(Finish->UnlockedBy);
			}
		});
		int32 Learned = 0;
		for (const FName& Id : Needed)
		{
			Learned += Knowledge->Learn(Id) ? 1 : 0;
		}
		const TPair<const TCHAR*, int32> Kit[] = { { TEXT("item.component.stud"), 300 }, { TEXT("item.material.timber_plank"), 300 },
			{ TEXT("item.material.timber_log"), 30 }, { TEXT("item.material.cut_stone"), 30 } };
		TArray<FString> Given;
		int32 Items = 0;
		for (const TPair<const TCHAR*, int32>& Entry : Kit)
		{
			const int32 Added = Inventory->AddItem(Entry.Key, Entry.Value);
			Items += Added;
			Given.Add(FString::Printf(TEXT("%d %s"), Added, Entry.Key));
		}
		OutReport = FString::Printf(TEXT("learned %d (of %d needed), gave %s"), Learned, Needed.Num(), *FString::Join(Given, TEXT(", ")));
		return Items;
	}

	FAutoConsoleCommandWithWorldAndArgs StarterKitCommand(
		TEXT("gl.Dev.BuildingStarterKit"),
		TEXT("DEV ONLY (P12): building knowledge and a kit of construction materials for the daily-driver session."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>&, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			FString Report;
			GiveStarterKit(World, PC ? PC->GetPawn() : nullptr, Report);
			UE_LOG(LogGridlands, Display, TEXT("gl.Dev.BuildingStarterKit: %s"), *Report);
		}));
}

#endif // !UE_BUILD_SHIPPING
