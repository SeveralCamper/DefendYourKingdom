// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKOfflineClockSubsystem.h"

#include "DYKProfileSaveGame.h"
#include "DYKSaveSubsystem.h"

void UDYKOfflineClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDYKSaveSubsystem>();

	UDYKSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	UDYKProfileSaveGame* Profile = SaveSubsystem->GetActiveProfile().Get();
	if (!Profile || SaveSubsystem->GetLoadStatus() == EDYKProfileLoadStatus::CreationFailed)
	{
		LastError = SaveSubsystem->GetLoadError().IsEmpty()
			? TEXT("No writable local profile is available for offline progress.")
			: SaveSubsystem->GetLoadError();
		return;
	}

	const int64 NowUnix = FDateTime::UtcNow().ToUnixTimestamp();
	const int64 LastCalculationUnix = Profile->Profile.LastOfflineCalculationUnix;
	CalculationTimestampUnix = FMath::Max(NowUnix, LastCalculationUnix);
	ElapsedSeconds = LastCalculationUnix > 0
		? FMath::Max<int64>(0, NowUnix - LastCalculationUnix)
		: 0;

	bHasPendingOfflineCalculation = true;
	bIsReady = true;
}

bool UDYKOfflineClockSubsystem::IsReady() const
{
	return bIsReady;
}

bool UDYKOfflineClockSubsystem::HasPendingOfflineCalculation() const
{
	return bIsReady && bHasPendingOfflineCalculation;
}

int64 UDYKOfflineClockSubsystem::GetElapsedSeconds() const
{
	return HasPendingOfflineCalculation() ? ElapsedSeconds : 0;
}

int64 UDYKOfflineClockSubsystem::GetCalculationTimestampUnix() const
{
	return CalculationTimestampUnix;
}

bool UDYKOfflineClockSubsystem::CommitOfflineCalculation()
{
	if (!HasPendingOfflineCalculation())
	{
		return false;
	}

	UDYKSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	UDYKProfileSaveGame* Profile = SaveSubsystem->GetActiveProfile().Get();
	if (!Profile)
	{
		LastError = TEXT("The local profile became unavailable before offline progress could be saved.");
		return false;
	}

	const int64 PreviousTimestamp = Profile->Profile.LastOfflineCalculationUnix;
	Profile->Profile.LastOfflineCalculationUnix = FMath::Max(PreviousTimestamp, CalculationTimestampUnix);
	if (!SaveSubsystem->SaveProfile())
	{
		Profile->Profile.LastOfflineCalculationUnix = PreviousTimestamp;
		LastError = SaveSubsystem->GetLoadError();
		return false;
	}

	bHasPendingOfflineCalculation = false;
	LastError.Reset();
	return true;
}

bool UDYKOfflineClockSubsystem::IsDailyRewardAvailable() const
{
	UDYKSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	const UDYKProfileSaveGame* Profile = SaveSubsystem->GetActiveProfile().Get();
	return Profile && GetCurrentLocalDateKey() > Profile->Profile.LastDailyRewardDateKey;
}

bool UDYKOfflineClockSubsystem::TryRecordDailyRewardClaim()
{
	UDYKSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	UDYKProfileSaveGame* Profile = SaveSubsystem->GetActiveProfile().Get();
	const int32 TodayDateKey = GetCurrentLocalDateKey();
	if (!Profile || TodayDateKey <= Profile->Profile.LastDailyRewardDateKey)
	{
		return false;
	}

	const int32 PreviousDateKey = Profile->Profile.LastDailyRewardDateKey;
	Profile->Profile.LastDailyRewardDateKey = TodayDateKey;
	if (!SaveSubsystem->SaveProfile())
	{
		Profile->Profile.LastDailyRewardDateKey = PreviousDateKey;
		LastError = SaveSubsystem->GetLoadError();
		return false;
	}

	LastError.Reset();
	return true;
}

const FString& UDYKOfflineClockSubsystem::GetLastError() const
{
	return LastError;
}

int32 UDYKOfflineClockSubsystem::GetCurrentLocalDateKey() const
{
	const FDateTime LocalDate = FDateTime::Now();
	return LocalDate.GetYear() * 10000 + LocalDate.GetMonth() * 100 + LocalDate.GetDay();
}
