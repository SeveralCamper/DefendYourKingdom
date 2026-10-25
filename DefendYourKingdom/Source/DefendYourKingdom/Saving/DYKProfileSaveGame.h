// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Core/Content/DYKContentId.h"
#include "GameFramework/SaveGame.h"
#include "DYKProfileSaveGame.generated.h"

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKResourceSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FDYKContentId ResourceId;

	UPROPERTY(SaveGame)
	int64 Amount = 0;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKParcelSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	UPROPERTY(SaveGame)
	bool bIsUnlocked = false;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKBuildingSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FDYKContentId BuildingId;

	UPROPERTY(SaveGame)
	FIntPoint CellOrigin = FIntPoint::ZeroValue;

	UPROPERTY(SaveGame)
	int32 RotationQuarterTurns = 0;

	UPROPERTY(SaveGame)
	int32 Level = 1;

	UPROPERTY(SaveGame)
	int64 ConstructionStartedAtUnix = 0;

	UPROPERTY(SaveGame)
	int64 ConstructionCompletesAtUnix = 0;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKUpgradeSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FDYKContentId UpgradeId;

	UPROPERTY(SaveGame)
	int32 Level = 0;

	UPROPERTY(SaveGame)
	int64 StartedAtUnix = 0;

	UPROPERTY(SaveGame)
	int64 CompletesAtUnix = 0;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKCampaignProgressSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FDYKContentId CampaignId;

	UPROPERTY(SaveGame)
	TArray<FDYKContentId> UnlockedMissions;

	UPROPERTY(SaveGame)
	TArray<FDYKContentId> CompletedMissions;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKMissionDeploymentSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FDYKContentId SlotId;

	UPROPERTY(SaveGame)
	FDYKContentId ContentId;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKCurrentMissionSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FDYKContentId MissionId;

	UPROPERTY(SaveGame)
	int32 CurrentWaveIndex = 0;

	UPROPERTY(SaveGame)
	int64 Supplies = 0;

	UPROPERTY(SaveGame)
	int64 Valor = 0;

	UPROPERTY(SaveGame)
	bool bIsActive = false;

	UPROPERTY(SaveGame)
	TArray<FDYKMissionDeploymentSaveData> Deployment;
};

USTRUCT()
struct DEFENDYOURKINGDOM_API FDYKPlayerProfileSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TArray<FDYKParcelSaveData> Parcels;

	UPROPERTY(SaveGame)
	TArray<FDYKBuildingSaveData> Buildings;

	UPROPERTY(SaveGame)
	TArray<FDYKResourceSaveData> Resources;

	UPROPERTY(SaveGame)
	TArray<FDYKContentId> ResearchedContent;

	UPROPERTY(SaveGame)
	TArray<FDYKUpgradeSaveData> Upgrades;

	UPROPERTY(SaveGame)
	TArray<FDYKContentId> UnlockedContent;

	// Local calendar date in YYYYMMDD form; zero means no reward has been claimed.
	UPROPERTY(SaveGame)
	int32 LastDailyRewardDateKey = 0;

	UPROPERTY(SaveGame)
	int64 LastOfflineCalculationUnix = 0;

	UPROPERTY(SaveGame)
	FDYKCampaignProgressSaveData CampaignProgress;

	UPROPERTY(SaveGame)
	FDYKCurrentMissionSaveData CurrentMission;
};

UCLASS()
class DEFENDYOURKINGDOM_API UDYKProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSchemaVersion = 1;

	UPROPERTY(SaveGame)
	int32 SchemaVersion = CurrentSchemaVersion;

	UPROPERTY(SaveGame)
	FDYKPlayerProfileSaveData Profile;
};
