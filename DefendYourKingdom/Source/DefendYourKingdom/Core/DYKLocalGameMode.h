#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DYKLocalGameMode.generated.h"

// Common startup guard for independently loaded local worlds.
UCLASS(Abstract)
class DEFENDYOURKINGDOM_API ADYKLocalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADYKLocalGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void StartPlay() override;

private:
	FString StartupError;
};
