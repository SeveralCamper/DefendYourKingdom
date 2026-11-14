// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKCityGameMode.h"

#include "../DefendYourKingdom.h"
#include "../Core/DYKGameInstance.h"
#include "../Core/Interaction/DYKSelectionPlayerController.h"
#include "../Core/Interaction/DYKStrategyCameraPawn.h"

ADYKCityGameMode::ADYKCityGameMode()
{
	PlayerControllerClass = ADYKSelectionPlayerController::StaticClass();
	DefaultPawnClass = ADYKStrategyCameraPawn::StaticClass();
}

void ADYKCityGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	StartupError = ErrorMessage;
	const UDYKGameInstance* Session = Cast<UDYKGameInstance>(GetGameInstance());
	if (StartupError.IsEmpty() && !Session)
	{
		StartupError = TEXT("The city requires DYKGameInstance.");
	}
	if (StartupError.IsEmpty())
	{
		Session->IsLocalSessionReady(StartupError);
	}
	ErrorMessage = StartupError;
	if (!StartupError.IsEmpty())
	{
		// The local controller otherwise creates a freely moving spectator camera.
		SpectatorClass = nullptr;
		UE_LOG(LogDYK, Error, TEXT("City startup blocked: %s"), *StartupError);
	}
}

void ADYKCityGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!StartupError.IsEmpty())
	{
		return;
	}
	Super::RestartPlayer(NewPlayer);
}

void ADYKCityGameMode::StartPlay()
{
	// UE does not abort world startup on InitGame's ErrorMessage alone.
	if (!StartupError.IsEmpty())
	{
		return;
	}
	Super::StartPlay();
	UE_LOG(LogDYK, Log, TEXT("City started with a ready local session."));
}
