// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DYKProfileService.h"
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
	CreationFailed,
	Recovered
};

UCLASS()
class DEFENDYOURKINGDOM_API UDYKSaveSubsystem : public UGameInstanceSubsystem, public IDYKProfileService
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool SaveProfile();
	const UDYKProfileSaveGame* GetActiveProfile() const;
	virtual const FDYKPlayerProfileSaveData* GetProfileSnapshot() const override;
	virtual int64 GetRevision() const override;
	virtual bool ApplyProfileCommand(const FGuid& CommandId,
		TFunctionRef<bool(FDYKPlayerProfileSaveData&)> Command) override;
	EDYKProfileLoadStatus GetLoadStatus() const;
	const FString& GetLoadError() const;

private:
	friend struct FDYKCityHUDTestAccess;
	friend struct FDYKCityResearchTestAccess;
	friend struct FDYKProfileTestAccess;
	friend struct FDYKCityEconomyTestAccess;
	friend struct FDYKCityTerritoryTestAccess;
	friend struct FDYKMissionRewardTestAccess;
	void LoadOrCreateProfile();
	FString GetSavePath(int32 Slot) const;
	bool WriteProfile(UDYKProfileSaveGame* Candidate);

	UPROPERTY()
	TObjectPtr<UDYKProfileSaveGame> ActiveProfile;

	EDYKProfileLoadStatus LoadStatus = EDYKProfileLoadStatus::NotLoaded;
	FString LoadError;
	FString SlotPrefix;
	int64 Revision = 0;
	int32 ActiveSlot = INDEX_NONE;
	bool bWriting = false;
};
