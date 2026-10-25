// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DYKOfflineClockSubsystem.generated.h"

UCLASS()
class DEFENDYOURKINGDOM_API UDYKOfflineClockSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool IsReady() const;
	bool HasPendingOfflineCalculation() const;
	int64 GetElapsedSeconds() const;
	int64 GetCalculationTimestampUnix() const;

	// Call after offline production and construction have updated the active profile.
	// The profile state and timestamp are written together to avoid losing or replaying progress.
	bool CommitOfflineCalculation();

	bool IsDailyRewardAvailable() const;
	bool TryRecordDailyRewardClaim();
	const FString& GetLastError() const;

private:
	int32 GetCurrentLocalDateKey() const;

	int64 ElapsedSeconds = 0;
	int64 CalculationTimestampUnix = 0;
	bool bIsReady = false;
	bool bHasPendingOfflineCalculation = false;
	FString LastError;
};
