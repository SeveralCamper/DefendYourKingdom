#pragma once

#include "CoreMinimal.h"
#include "DYKCitySaveData.generated.h"

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKCityOrderSaveData
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FGuid OrderId;
	UPROPERTY(SaveGame) int32 Recipe = 0;
	UPROPERTY(SaveGame) int64 EnqueuedAtUnix = 0;
	UPROPERTY(SaveGame) int64 StartedAtUnix = 0;
	UPROPERTY(SaveGame) int64 CompletesAtUnix = 0;
	// Frozen at start. Completed output waits here if the building store is full.
	UPROPERTY(SaveGame) int64 RemainingOutput = 0;
};

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
	UPROPERTY(SaveGame) int32 OrdersVersion = 0;
	UPROPERTY(SaveGame) int64 OrdersUpdatedAtUnix = 0;
	UPROPERTY(SaveGame) TArray<FDYKCityOrderSaveData> Orders;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKCityWorkCost
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FName Resource;
	UPROPERTY(SaveGame) int64 Amount = 0;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKCityWorkSaveData
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FGuid WorkId;
	UPROPERTY(SaveGame) FGuid InstanceId;
	UPROPERTY(SaveGame) bool bUpgrade = false;
	UPROPERTY(SaveGame) int32 ExpectedLevel = 0;
	UPROPERTY(SaveGame) int64 EnqueuedAtUnix = 0;
	UPROPERTY(SaveGame) int64 StartedAtUnix = 0;
	UPROPERTY(SaveGame) int64 CompletesAtUnix = 0;
	UPROPERTY(SaveGame) TArray<FDYKCityWorkCost> Paid;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKCityResearchWorkSaveData
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FGuid WorkId;
	UPROPERTY(SaveGame) FName ResearchId;
	UPROPERTY(SaveGame) int64 EnqueuedAtUnix = 0;
	UPROPERTY(SaveGame) int64 StartedAtUnix = 0;
	UPROPERTY(SaveGame) int64 CompletesAtUnix = 0;
	UPROPERTY(SaveGame) TArray<FDYKCityWorkCost> Paid;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKCitySaveData
{
	GENERATED_BODY()
	// Zero is the pre-M1.05 profile, migrated once without replacing old progress.
	UPROPERTY(SaveGame) int32 Version = 0;
	UPROPERTY(SaveGame) int32 TerritoryVersion = 0;
	UPROPERTY(SaveGame) TArray<FDYKCityProductionSaveData> Instances;
	UPROPERTY(SaveGame) int64 WorkUpdatedAtUnix = 0;
	UPROPERTY(SaveGame) bool bThirdBuilderEntitlement = false;
	UPROPERTY(SaveGame) TArray<FDYKCityWorkSaveData> Work;
	// Full cancellation refunds stay claimable if the warehouse was refilled meanwhile.
	UPROPERTY(SaveGame) TArray<FDYKCityWorkCost> PendingRefunds;
	UPROPERTY(SaveGame) int64 ResearchUpdatedAtUnix = 0;
	UPROPERTY(SaveGame) TArray<FName> CompletedResearch;
	UPROPERTY(SaveGame) TArray<FDYKCityResearchWorkSaveData> ResearchQueue;
	// Stable card keys, independent of page, building instance and research completion.
	UPROPERTY(SaveGame) TArray<FName> VisitedCards;
};
