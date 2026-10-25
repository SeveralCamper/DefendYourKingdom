// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKCityGameMode.h"

#include "../Core/Interaction/DYKSelectionPlayerController.h"
#include "../Core/Interaction/DYKStrategyCameraPawn.h"
#include "../Saving/DYKSaveSubsystem.h"

ADYKCityGameMode::ADYKCityGameMode()
{
	PlayerControllerClass = ADYKSelectionPlayerController::StaticClass();
	DefaultPawnClass = ADYKStrategyCameraPawn::StaticClass();
}

void ADYKCityGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("Citadel started without a game instance; local profile is unavailable."));
		return;
	}

	const UDYKSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UDYKSaveSubsystem>();
	if (!SaveSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("Citadel started without the local profile subsystem."));
		return;
	}

	switch (SaveSubsystem->GetLoadStatus())
	{
	case EDYKProfileLoadStatus::Loaded:
		UE_LOG(LogTemp, Log, TEXT("Citadel started with the existing local profile."));
		break;
	case EDYKProfileLoadStatus::Created:
		UE_LOG(LogTemp, Log, TEXT("Citadel started with a newly created local profile."));
		break;
	default:
		UE_LOG(LogTemp, Error, TEXT("Citadel started without a usable local profile: %s"), *SaveSubsystem->GetLoadError());
		break;
	}
}
