#include "DYKGameInstance.h"
#include "../Battle/DYKPolygonGameMode.h"
#include "../Meta/DYKCityGameMode.h"

#include "../DefendYourKingdom.h"
#include "../Saving/DYKOfflineClockSubsystem.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

void UDYKGameInstance::Init()
{
	Super::Init();
	LevelLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnLevelLoaded);
	TravelFailedHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::OnTravelFailed);

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

void UDYKGameInstance::Shutdown()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LevelLoadedHandle);
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(TravelFailedHandle);
	}
	bLevelTravelPending = false;
	Super::Shutdown();
}

FName UDYKGameInstance::GetLevelPackage(const EDYKLocalLevel Destination)
{
	switch (Destination)
	{
	case EDYKLocalLevel::City:
		return TEXT("/Game/DefendYourKingdom/Maps/L_Citadel");
	case EDYKLocalLevel::MissionPolygon:
		return TEXT("/Game/DefendYourKingdom/Maps/L_MissionPolygon");
	default:
		return NAME_None;
	}
}

bool UDYKGameInstance::RequestLevelTravel(const EDYKLocalLevel Destination, FString& OutError)
{
	OutError.Reset();
	const FName Package = GetLevelPackage(Destination);
	if (Package.IsNone())
	{
		OutError = TEXT("Unknown local destination; only city and draft mission polygon are available.");
		return false;
	}
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != this)
	{
		OutError = TEXT("Level travel requires an active local game world.");
		return false;
	}
	if (bLevelTravelPending)
	{
		OutError = TEXT("A level load is already pending.");
		return false;
	}
	if (const auto* City = World->GetAuthGameMode<ADYKCityGameMode>(); City && City->GetLayoutEditor().IsActive())
	{
		OutError = TEXT("Завершите редактор города: примените расстановку или отмените изменения.");
		return false;
	}
	if (!IsLocalSessionReady(OutError))
	{
		return false;
	}
	// Remove the PIE prefix before comparing package names.
	const FString CurrentPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	if (CurrentPackage == Package.ToString())
	{
		OutError = TEXT("The requested level is already loaded.");
		return false;
	}
	if (!FPackageName::DoesPackageExist(Package.ToString()))
	{
		OutError = TEXT("The requested local map package is missing.");
		return false;
	}
	// Preserve a terminal victory before any caller unloads its world. Failed IO
	// leaves the result available for retry instead of silently discarding it.
	if (auto* Battle = World->GetAuthGameMode<ADYKPolygonGameMode>(); Battle &&
		Battle->GetBattleSession().GetResult() && !Battle->SettleMissionResult())
	{
		OutError = Battle->GetMissionResultError();
		return false;
	}
	bLevelTravelPending = true;
	UE_LOG(LogDYK, Log, TEXT("Loading local level: %s"), *Package.ToString());
	UGameplayStatics::OpenLevel(World, Package, true);
	return true;
}

void UDYKGameInstance::OnLevelLoaded(UWorld* LoadedWorld)
{
	if (LoadedWorld && LoadedWorld->GetGameInstance() == this)
	{
		bLevelTravelPending = false;
	}
}

void UDYKGameInstance::OnTravelFailed(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (World && World->GetGameInstance() == this)
	{
		bLevelTravelPending = false;
		UE_LOG(LogDYK, Error, TEXT("Local level load failed (%d): %s"), static_cast<int32>(FailureType), *Error);
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
