// Copyright Epic Games, Inc. All Rights Reserved.

#include "DYKOfflineClockSubsystem.h"

#include "DYKProfileSaveGame.h"
#include "DYKSaveSubsystem.h"

void UDYKOfflineClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDYKSaveSubsystem>();

	UDYKSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	const FDYKPlayerProfileSaveData* Profile = SaveSubsystem->GetProfileSnapshot();
	if (!Profile || SaveSubsystem->GetLoadStatus() == EDYKProfileLoadStatus::CreationFailed)
	{
		LastError = SaveSubsystem->GetLoadError().IsEmpty()
			? TEXT("No writable local profile is available for offline progress.")
			: SaveSubsystem->GetLoadError();
		return;
	}

	const int64 NowUnix = FDateTime::UtcNow().ToUnixTimestamp();
	const int64 LastCalculationUnix = Profile->LastOfflineCalculationUnix;
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
	const FDYKPlayerProfileSaveData* Profile = SaveSubsystem->GetProfileSnapshot();
	if (!Profile)
	{
		LastError = TEXT("The local profile became unavailable before offline progress could be saved.");
		return false;
	}

	if (!SaveSubsystem->ApplyProfileCommand(FGuid::NewGuid(), [this](FDYKPlayerProfileSaveData& Candidate)
	{
		Candidate.LastOfflineCalculationUnix = FMath::Max(Candidate.LastOfflineCalculationUnix, CalculationTimestampUnix);
		return true;
	}))
	{
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
	const FDYKPlayerProfileSaveData* Profile = SaveSubsystem->GetProfileSnapshot();
	return Profile && GetCurrentLocalDateKey() > Profile->LastDailyRewardDateKey;
}

bool UDYKOfflineClockSubsystem::TryRecordDailyRewardClaim()
{
	UDYKSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	const FDYKPlayerProfileSaveData* Profile = SaveSubsystem->GetProfileSnapshot();
	const int32 TodayDateKey = GetCurrentLocalDateKey();
	if (!Profile || TodayDateKey <= Profile->LastDailyRewardDateKey)
	{
		return false;
	}

	if (!SaveSubsystem->ApplyProfileCommand(FGuid::NewGuid(), [TodayDateKey](FDYKPlayerProfileSaveData& Candidate)
	{
		Candidate.LastDailyRewardDateKey = TodayDateKey;
		return true;
	}))
	{
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
