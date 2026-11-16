#pragma once

#include "DYKCityCatalog.h"
#include "DYKCityTerritory.h"

struct FDYKCityBuildingLevelRule
{
	int32 RequiredCastleLevel = 1;
	int64 DurationSeconds = 0;
	TArray<FDYKResourceSaveData> Costs;
	TArray<FName> Conditions;
};

struct FDYKCityBuildingProgression
{
	// Entry 0 constructs level 1; subsequent entries upgrade one level at a time.
	TArray<FDYKCityBuildingLevelRule> Levels;
	// Only repeatable types use this table; entry 0 is castle level 1.
	TArray<int32> LimitsByCastleLevel;
};

struct FDYKCityBuildingRulesConfig
{
	TMap<FName, FDYKCityBuildingProgression> Buildings;
	TMap<FName, TFunction<bool(const FDYKPlayerProfileSaveData&)>> Conditions;
};

enum class EDYKCityBuildingWorkKind : uint8 { Build, Upgrade };

struct FDYKCityBuildingWorkRequest
{
	FGuid CommandId;
	EDYKCityBuildingWorkKind Kind = EDYKCityBuildingWorkKind::Build;
	FDYKBuildingId BuildingId;
	// New identity for Build; existing identity for Upgrade. Never a mutable array index.
	FGuid InstanceId;
	// Placement input for Build. Upgrade resolves current placement by InstanceId.
	FIntPoint Origin = FIntPoint::ZeroValue;
	int32 QuarterTurns = 0;
	// Optimistic precondition: 0 for Build, completed current level for Upgrade.
	int32 ExpectedLevel = 0;
};

struct FDYKCityBuildingWorkPlan
{
	FDYKCityBuildingWorkRequest Request;
	int32 TargetLevel = 0;
	int64 DurationSeconds = 0;
	TArray<FDYKResourceSaveData> Costs;
};

namespace DYKCityBuildingRules
{
	DEFENDYOURKINGDOM_API const FDYKCityBuildingRulesConfig& DEV_PLACEHOLDER();
	DEFENDYOURKINGDOM_API bool ValidateOwnership(const FDYKPlayerProfileSaveData& Profile,
		const FDYKCityBuildingRulesConfig& Config);
	// Queue contracts only: no ownership, level, timers, balance or ledger mutations.
	// M2.06 must include queued reservations when enqueueing and revalidate at start.
	DEFENDYOURKINGDOM_API bool Prepare(const FDYKPlayerProfileSaveData& Profile,
		const FDYKCityBuildingWorkRequest& Request, const FDYKCityTerritoryConfig& Territory,
		const FDYKCityBuildingRulesConfig& Config, FDYKCityBuildingWorkPlan& Plan);
	// Payment belongs to actual start, never to Prepare/enqueue. Recompute trusted costs.
	DEFENDYOURKINGDOM_API bool CanStart(const FDYKPlayerProfileSaveData& Profile,
		const FDYKCityBuildingWorkRequest& Request, const FDYKCityTerritoryConfig& Territory,
		const FDYKCityBuildingRulesConfig& Config, FDYKCityBuildingWorkPlan& Plan);
	DEFENDYOURKINGDOM_API bool CanSell(const FDYKPlayerProfileSaveData& Profile, FGuid Instance);
	// Quote from actual persisted expenditure, not today's catalog price. No sale executes.
	DEFENDYOURKINGDOM_API bool PrepareSale(const FDYKPlayerProfileSaveData& Profile, FGuid Instance,
		TArray<FDYKResourceSaveData>& Refund);
}
