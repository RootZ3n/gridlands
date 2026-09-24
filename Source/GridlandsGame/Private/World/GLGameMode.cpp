#include "World/GLGameMode.h"

#include "Character/GLCharacter.h"
#include "Events/GLEventSubsystem.h"
#include "GameplayTagsManager.h"

AGLGameMode::AGLGameMode()
{
	DefaultPawnClass = AGLCharacter::StaticClass();
}

void AGLGameMode::StartPlay()
{
	Super::StartPlay();
	FGLGameplayEvent Started;
	Started.Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Event.Game.Started"));
	UGLEventSubsystem::Emit(this, MoveTemp(Started));
}
