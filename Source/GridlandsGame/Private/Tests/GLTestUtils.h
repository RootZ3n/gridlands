#pragma once

// Shared helpers for GridlandsGame automation tests.
//
// Unreal's unity build compiles several .cpp files as one translation unit, so test files must
// not each define helpers with the same names. Put shared helpers here; give file-local ones
// unique names.

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace GLTestUtils
{
	inline constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** A throwaway game world with physics and world subsystems, destroyed at scope end. */
	struct FTestWorld
	{
		UWorld* World = nullptr;
		explicit FTestWorld(const TCHAR* Name = TEXT("GLTestWorld"))
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, Name);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			// Free it now: a 1 km world (terrain, structures, scatter) is gigabytes, and the whole
			// suite's worlds must not pile up until the editor exits (the gate hit the OOM killer).
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	};

	inline FGameplayTag Tag(const TCHAR* Name) { return UGameplayTagsManager::Get().RequestGameplayTag(FName(Name)); }
}
