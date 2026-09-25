#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GLGameMode.generated.h"

UCLASS()
class GRIDLANDSGAME_API AGLGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGLGameMode();
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
