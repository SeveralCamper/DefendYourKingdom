#pragma once

#include "../Saving/DYKProfileSaveGame.h"

class IDYKProfileService;

struct FDYKCityParcelRule
{
	TArray<FDYKResourceSaveData> Costs;
	// Extensible keys resolved by the trusted configuration, never by UI input.
	TArray<FName> Conditions;
};

struct FDYKCityTerritoryConfig
{
	TSet<FIntPoint> InitialParcels;
	TMap<FName, TArray<FIntPoint>> Footprints;
	TMap<FName, FIntPoint> StarterOrigins;
	TMap<FIntPoint, FDYKCityParcelRule> ParcelRules;
	TMap<FName, TFunction<bool(const FDYKPlayerProfileSaveData&)>> Conditions;
	TMap<FName, FString> ConditionLabels;
};

namespace DYKCityTerritory
{
	inline constexpr int32 ParcelsPerSide = 5;
	inline constexpr int32 CellsPerParcel = 10;
	inline constexpr int32 CellsPerSide = 50;
	inline constexpr int32 CurrentVersion = 2;
	// Visual scale and all unspecified layout/cost data are DEV_PLACEHOLDER.
	inline constexpr double CellSize = 200.;
	DEFENDYOURKINGDOM_API const FDYKCityTerritoryConfig& DEV_PLACEHOLDER();
	DEFENDYOURKINGDOM_API bool IsUnlocked(const FDYKPlayerProfileSaveData& Profile, FIntPoint Parcel);
	DEFENDYOURKINGDOM_API bool OccupiedCells(const FDYKBuildingSaveData& Building,
		const FDYKCityTerritoryConfig& Config, TArray<FIntPoint>& Cells);
	// Body stays the authored footprint. The mandatory one-cell ring must fit in full.
	DEFENDYOURKINGDOM_API bool PlacementCells(const FDYKBuildingSaveData& Building,
		const FDYKCityTerritoryConfig& Config, TArray<FIntPoint>& Body, TArray<FIntPoint>& Perimeter);
	// Shared roads may overlap; a body may never overlap any other body or perimeter.
	// Also used for partial editor drafts (including an empty temporary layout).
	DEFENDYOURKINGDOM_API bool ValidateLayout(const FDYKPlayerProfileSaveData& Profile,
		TConstArrayView<FDYKBuildingSaveData> Buildings, const FDYKCityTerritoryConfig& Config);
	DEFENDYOURKINGDOM_API bool Validate(const FDYKPlayerProfileSaveData& Profile, const FDYKCityTerritoryConfig& Config);
	// Atomic adaptation of pre-grid/V1 geometry; preserves ownership, GUIDs, economy and open land.
	DEFENDYOURKINGDOM_API bool Initialize(FDYKPlayerProfileSaveData& Profile, const FDYKCityTerritoryConfig& Config);
	DEFENDYOURKINGDOM_API bool CanUnlock(const FDYKPlayerProfileSaveData& Profile, FIntPoint Parcel,
		const FDYKCityTerritoryConfig& Config);
	DEFENDYOURKINGDOM_API bool Unlock(IDYKProfileService& Service, FGuid Command, FIntPoint Parcel,
		const FDYKCityTerritoryConfig& Config);
	// Geometry contract for M2.03; moving existing ownership never constructs or charges.
	DEFENDYOURKINGDOM_API bool Move(IDYKProfileService& Service, FGuid Command, FGuid Instance,
		FIntPoint Origin, int32 QuarterTurns, const FDYKCityTerritoryConfig& Config);
	DEFENDYOURKINGDOM_API FVector BuildingLocation(const FDYKBuildingSaveData& Building,
		const FDYKCityTerritoryConfig& Config);
	DEFENDYOURKINGDOM_API FVector CellCorner(FIntPoint Cell);
}
