// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKLocalGameMode.h"

#include "../DefendYourKingdom.h"
#include "../Core/DYKGameInstance.h"
#include "../Core/Interaction/DYKSelectionPlayerController.h"
#include "../Core/Interaction/DYKStrategyCameraPawn.h"

ADYKLocalGameMode::ADYKLocalGameMode()
{
	PlayerControllerClass = ADYKSelectionPlayerController::StaticClass();
	DefaultPawnClass = ADYKStrategyCameraPawn::StaticClass();
}

void ADYKLocalGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	StartupError = ErrorMessage;
	const UDYKGameInstance* Session = Cast<UDYKGameInstance>(GetGameInstance());
	if (StartupError.IsEmpty() && !Session)
	{
		StartupError = TEXT("The local world requires DYKGameInstance.");
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
		UE_LOG(LogDYK, Error, TEXT("Local world startup blocked: %s"), *StartupError);
	}
}

void ADYKLocalGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!StartupError.IsEmpty())
	{
		return;
	}
	Super::RestartPlayer(NewPlayer);
}

void ADYKLocalGameMode::StartPlay()
{
	// UE does not abort world startup on InitGame's ErrorMessage alone.
	if (!StartupError.IsEmpty())
	{
		return;
	}
	Super::StartPlay();
	UE_LOG(LogDYK, Log, TEXT("Local world started with a ready local session."));
}
