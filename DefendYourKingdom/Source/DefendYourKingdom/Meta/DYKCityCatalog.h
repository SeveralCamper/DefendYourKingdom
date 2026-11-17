#pragma once

#include "../Core/Content/DYKTypedContentId.h"
#include "Containers/ArrayView.h"

enum class EDYKCityBuildingCategory : uint8 { Castle, Unique, Production, House, Decoration };

struct FDYKCityBuildingDefinition
{
	FDYKBuildingId Id;
	const TCHAR* DisplayName;
	FDYKResourceId Resource;
	int32 UnitsPerCycle;
	int32 CycleSeconds;
	int32 StorageCycles;
	FIntPoint StarterCell;
	EDYKCityBuildingCategory Category;
	bool bInitiallyOwned;
	// Per type, never per level. Unspecified sizes are DEV_PLACEHOLDER.
	FIntPoint Footprint;
};

struct FDYKCityProductionLevel
{
	int32 UnitsPerCycle = 0;
	int32 CycleSeconds = 0;
	int64 Capacity = 0;
};

namespace DYKCityCatalog
{
	// All rates, house duration, warehouse capacities and starter positions below
	// are replaceable DEV_PLACEHOLDER data, not approved balance.
	DEFENDYOURKINGDOM_API TConstArrayView<FDYKCityBuildingDefinition> Buildings();
	DEFENDYOURKINGDOM_API const FDYKCityBuildingDefinition* Find(const FDYKContentId& Id);
	DEFENDYOURKINGDOM_API TConstArrayView<FDYKResourceId> Resources();
	DEFENDYOURKINGDOM_API int64 ResourceCapacity(FDYKResourceId Id);
	DEFENDYOURKINGDOM_API FDYKCityProductionLevel Production(FDYKBuildingId Id, int32 Level);
	DEFENDYOURKINGDOM_API int64 WarehouseCapacityDEV_PLACEHOLDER(FDYKResourceId Id, int32 Level);
	DEFENDYOURKINGDOM_API const TCHAR* ResourceName(FDYKResourceId Id);
}
