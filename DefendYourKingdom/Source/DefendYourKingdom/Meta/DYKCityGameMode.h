// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DYKCityGameMode.generated.h"

UCLASS()
class DEFENDYOURKINGDOM_API ADYKCityGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADYKCityGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void StartPlay() override;

private:
	FString StartupError;
};
