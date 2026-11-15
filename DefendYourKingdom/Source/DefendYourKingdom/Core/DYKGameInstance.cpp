#include "DYKGameInstance.h"

#include "../DefendYourKingdom.h"
#include "../Saving/DYKOfflineClockSubsystem.h"
#include "../Saving/DYKSaveSubsystem.h"

void UDYKGameInstance::Init()
{
	Super::Init();

	FString Error;
	if (IsLocalSessionReady(Error))
	{
		UE_LOG(LogDYK, Log, TEXT("Local session initialized: profile and offline clock are ready."));
	}
	else
	{
		UE_LOG(LogDYK, Error, TEXT("Local session initialization failed: %s"), *Error);
	}
}

bool UDYKGameInstance::IsLocalSessionReady(FString& OutError) const
{
	OutError.Reset();
	const UDYKSaveSubsystem* Save = GetSubsystem<UDYKSaveSubsystem>();
	if (!Save)
	{
		OutError = TEXT("The local profile subsystem is unavailable.");
		return false;
	}

	if ((Save->GetLoadStatus() != EDYKProfileLoadStatus::Loaded
		&& Save->GetLoadStatus() != EDYKProfileLoadStatus::Created
		&& Save->GetLoadStatus() != EDYKProfileLoadStatus::Recovered) || !Save->GetProfileSnapshot())
	{
		OutError = Save->GetLoadError().IsEmpty()
			? TEXT("No usable local profile is available.") : Save->GetLoadError();
		return false;
	}

	const UDYKOfflineClockSubsystem* Clock = GetSubsystem<UDYKOfflineClockSubsystem>();
	if (!Clock || !Clock->IsReady())
	{
		OutError = Clock && !Clock->GetLastError().IsEmpty()
			? Clock->GetLastError() : TEXT("The offline clock is unavailable.");
		return false;
	}
	return true;
}
