// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKSaveSubsystem.h"

#include "DYKProfileSaveGame.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FString ProfileSlotName = TEXT("DYK_LocalProfile");
	constexpr int32 LocalUserIndex = 0;
}

void UDYKSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadOrCreateProfile();
}

bool UDYKSaveSubsystem::SaveProfile()
{
	if (!ActiveProfile || LoadStatus == EDYKProfileLoadStatus::Corrupt
		|| LoadStatus == EDYKProfileLoadStatus::UnsupportedVersion)
	{
		return false;
	}

	if (ActiveProfile->SchemaVersion != UDYKProfileSaveGame::CurrentSchemaVersion)
	{
		LoadStatus = EDYKProfileLoadStatus::UnsupportedVersion;
		LoadError = FString::Printf(
			TEXT("Profile schema version %d cannot be written by this build."),
			ActiveProfile->SchemaVersion);
		return false;
	}

	if (!UGameplayStatics::SaveGameToSlot(ActiveProfile, ProfileSlotName, LocalUserIndex))
	{
		LoadError = TEXT("The local profile could not be written to disk.");
		return false;
	}

	LoadError.Reset();
	return true;
}

TObjectPtr<UDYKProfileSaveGame> UDYKSaveSubsystem::GetActiveProfile() const
{
	return ActiveProfile;
}

EDYKProfileLoadStatus UDYKSaveSubsystem::GetLoadStatus() const
{
	return LoadStatus;
}

const FString& UDYKSaveSubsystem::GetLoadError() const
{
	return LoadError;
}

void UDYKSaveSubsystem::LoadOrCreateProfile()
{
	if (!UGameplayStatics::DoesSaveGameExist(ProfileSlotName, LocalUserIndex))
	{
		ActiveProfile = Cast<UDYKProfileSaveGame>(UGameplayStatics::CreateSaveGameObject(UDYKProfileSaveGame::StaticClass()));
		if (!ActiveProfile)
		{
			LoadStatus = EDYKProfileLoadStatus::CreationFailed;
			LoadError = TEXT("The local profile could not be created in memory.");
			return;
		}

		ActiveProfile->SchemaVersion = UDYKProfileSaveGame::CurrentSchemaVersion;
		LoadStatus = EDYKProfileLoadStatus::Created;
		if (!SaveProfile())
		{
			LoadStatus = EDYKProfileLoadStatus::CreationFailed;
		}
		return;
	}

	USaveGame* LoadedSave = UGameplayStatics::LoadGameFromSlot(ProfileSlotName, LocalUserIndex);
	UDYKProfileSaveGame* LoadedProfile = Cast<UDYKProfileSaveGame>(LoadedSave);
	if (!LoadedProfile)
	{
		LoadStatus = EDYKProfileLoadStatus::Corrupt;
		LoadError = TEXT("The local profile is damaged or has an unexpected save type; the file was left untouched.");
		return;
	}

	if (LoadedProfile->SchemaVersion != UDYKProfileSaveGame::CurrentSchemaVersion)
	{
		LoadStatus = EDYKProfileLoadStatus::UnsupportedVersion;
		LoadError = FString::Printf(
			TEXT("Profile schema version %d is incompatible with supported version %d; the file was left untouched."),
			LoadedProfile->SchemaVersion,
			UDYKProfileSaveGame::CurrentSchemaVersion);
		return;
	}

	ActiveProfile = LoadedProfile;
	LoadStatus = EDYKProfileLoadStatus::Loaded;
	LoadError.Reset();
}
