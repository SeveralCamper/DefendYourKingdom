#pragma once
#include "DYKCityWork.h"
#include "../Core/Content/DYKUnitDefinition.h"

struct FDYKResearchBonus
{
	int32 HP=0;
	int32 PhysicalArmor=0;
	int32 Damage=0;
	int32 MagicalArmor=0;
};

enum class EDYKResearchScope : uint8 { Line, Type };

struct FDYKResearchDefinition
{
	FName Id;
	FString DisplayName;
	TArray<FName> Prerequisites;
	TArray<FName> Conditions;
	int64 DurationSeconds=20; // DEV_PLACEHOLDER, never instant.
	TArray<FDYKResourceSaveData> Costs;
	FDYKContentId Unlock;
	EDYKUnitLine TargetLine=EDYKUnitLine::None;
	FDYKUnitTypeId TargetType;
	FDYKResearchBonus Bonus;
	// Owning gameplay building, independent of the shared logical research slot.
	FName Building;
};

struct FDYKResearchConfig
{
	TArray<FDYKResearchDefinition> Definitions;
	TMap<FName,TFunction<bool(const FDYKPlayerProfileSaveData&)>> Conditions;
};

namespace DYKCityResearch
{
	DEFENDYOURKINGDOM_API const FDYKResearchConfig& DEV_PLACEHOLDER();
	DEFENDYOURKINGDOM_API const FDYKResearchDefinition* Find(FName Id,const FDYKResearchConfig& Config=DEV_PLACEHOLDER());
	DEFENDYOURKINGDOM_API bool ValidateConfig(const FDYKResearchConfig& Config);
	DEFENDYOURKINGDOM_API bool Validate(const FDYKPlayerProfileSaveData& Profile,const FDYKResearchConfig& Config=DEV_PLACEHOLDER());
	DEFENDYOURKINGDOM_API bool CanEnqueue(const FDYKPlayerProfileSaveData& Profile,FName Research,const FDYKResearchConfig& Config=DEV_PLACEHOLDER());
	// Common chronological boundary for builders + the one independent research slot.
	DEFENDYOURKINGDOM_API bool Advance(FDYKPlayerProfileSaveData& Profile,int64 Now,const FDYKResearchConfig& Config=DEV_PLACEHOLDER());
	DEFENDYOURKINGDOM_API bool Enqueue(IDYKProfileService& Service,FGuid Command,FName Research,int64 Now,const FDYKResearchConfig& Config=DEV_PLACEHOLDER());
	DEFENDYOURKINGDOM_API bool Cancel(IDYKProfileService& Service,FGuid Command,FGuid Work,int64 Now,const FDYKResearchConfig& Config=DEV_PLACEHOLDER());
	DEFENDYOURKINGDOM_API bool IsUnlocked(const FDYKPlayerProfileSaveData& Profile,FDYKUnitTypeId Type);
	// Separate scopes remain queryable for inspection and balance tests.
	DEFENDYOURKINGDOM_API FDYKResearchBonus Bonus(const FDYKPlayerProfileSaveData& Profile,const FDYKUnitDefinition& Unit,
		EDYKResearchScope Scope=EDYKResearchScope::Line,const FDYKResearchConfig& Config=DEV_PLACEHOLDER());
	DEFENDYOURKINGDOM_API FDYKUnitBaseStats EffectiveStats(const FDYKPlayerProfileSaveData& Profile,const FDYKUnitDefinition& Unit);
}
