// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DYKSaveSubsystem.generated.h"

class UDYKProfileSaveGame;

UENUM()
enum class EDYKProfileLoadStatus : uint8
{
	NotLoaded,
	Loaded,
	Created,
	Corrupt,
	UnsupportedVersion,
	CreationFailed
};

UCLASS()
class DEFENDYOURKINGDOM_API UDYKSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool SaveProfile();
	TObjectPtr<UDYKProfileSaveGame> GetActiveProfile() const;
	EDYKProfileLoadStatus GetLoadStatus() const;
	const FString& GetLoadError() const;

private:
	void LoadOrCreateProfile();

	UPROPERTY()
	TObjectPtr<UDYKProfileSaveGame> ActiveProfile;

	EDYKProfileLoadStatus LoadStatus = EDYKProfileLoadStatus::NotLoaded;
	FString LoadError;
};
