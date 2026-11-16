#pragma once

#include "DYKCityCatalog.h"
#include "../Saving/DYKProfileSaveGame.h"

class IDYKProfileService;
struct FDYKCityBuildingRulesConfig;

struct FDYKCityCollectionView
{
	int64 Stored = 0;
	int64 Collectable = 0;
	int64 Capacity = 0;
};

// Pure domain calculations take explicit local time. No Tick, IO or world references.
namespace DYKCityEconomy
{
	DEFENDYOURKINGDOM_API bool Initialize(FDYKPlayerProfileSaveData& Profile, int64 Now);
	DEFENDYOURKINGDOM_API bool Validate(const FDYKPlayerProfileSaveData& Profile);
	DEFENDYOURKINGDOM_API bool Validate(const FDYKPlayerProfileSaveData& Profile, const FDYKCityBuildingRulesConfig& Config);
	DEFENDYOURKINGDOM_API int64 Balance(const FDYKPlayerProfileSaveData& Profile, FDYKResourceId Id);
	DEFENDYOURKINGDOM_API FDYKCityCollectionView View(const FDYKPlayerProfileSaveData& Profile, FGuid Instance, int64 Now);
	DEFENDYOURKINGDOM_API bool Collect(IDYKProfileService& Service, FGuid Command, FGuid Instance, int64 Now);
	DEFENDYOURKINGDOM_API bool Spend(IDYKProfileService& Service, FGuid Command, FDYKResourceId Resource, int64 Amount);
}
