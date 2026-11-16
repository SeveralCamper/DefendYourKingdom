#pragma once

#include "CoreMinimal.h"
#include "DYKCitySaveData.generated.h"

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKCityProductionSaveData
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FGuid InstanceId;
	// Index into Profile.Buildings; layout operations must preserve this association.
	UPROPERTY(SaveGame) int32 BuildingIndex = INDEX_NONE;
	UPROPERTY(SaveGame) int64 Stored = 0;
	UPROPERTY(SaveGame) int64 UpdatedAtUnix = 0;
	UPROPERTY(SaveGame) int64 CollectionStartedAtUnix = 0;
	UPROPERTY(SaveGame) int32 RemainderSeconds = 0;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKCitySaveData
{
	GENERATED_BODY()
	// Zero is the pre-M1.05 profile, migrated once without replacing old progress.
	UPROPERTY(SaveGame) int32 Version = 0;
	UPROPERTY(SaveGame) int32 TerritoryVersion = 0;
	UPROPERTY(SaveGame) TArray<FDYKCityProductionSaveData> Instances;
};
