#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DYKGameInstance.generated.h"

UENUM(BlueprintType)
enum class EDYKLocalLevel : uint8
{
	City,
	MissionPolygon
};

// Session lifetime only. Profile data and time remain owned by their subsystems.
UCLASS()
class DEFENDYOURKINGDOM_API UDYKGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;
	bool IsLocalSessionReady(FString& OutError) const;

	// Restricted to technical maps. No arbitrary URL, final campaign, or seamless travel.
	UFUNCTION(BlueprintCallable, Category="Local Session")
	bool RequestLevelTravel(EDYKLocalLevel Destination, FString& OutError);

	UFUNCTION(BlueprintPure, Category="Local Session")
	bool IsLevelTravelPending() const { return bLevelTravelPending; }

	static FName GetLevelPackage(EDYKLocalLevel Destination);

private:
	void OnLevelLoaded(UWorld* LoadedWorld);
	void OnTravelFailed(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	bool bLevelTravelPending = false;
	FDelegateHandle LevelLoadedHandle;
	FDelegateHandle TravelFailedHandle;
};
