// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Core/DYKLocalGameMode.h"
#include "DYKCityGameMode.generated.h"

UCLASS()
class DEFENDYOURKINGDOM_API ADYKCityGameMode : public ADYKLocalGameMode
{
	GENERATED_BODY()

public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
};
