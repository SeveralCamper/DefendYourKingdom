#pragma once

#include "../Core/Content/DYKTypedContentId.h"
#include "Containers/ArrayView.h"

struct FDYKCityBuildingDefinition
{
	FDYKBuildingId Id;
	const TCHAR* DisplayName;
	FDYKResourceId Resource;
	int32 UnitsPerCycle;
	int32 CycleSeconds;
	int32 StorageCycles;
	FIntPoint StarterCell;
};

namespace DYKCityCatalog
{
	// All rates, house duration, warehouse capacities and starter positions below
	// are replaceable DEV_PLACEHOLDER data, not approved balance.
	DEFENDYOURKINGDOM_API TConstArrayView<FDYKCityBuildingDefinition> Buildings();
	DEFENDYOURKINGDOM_API const FDYKCityBuildingDefinition* Find(const FDYKContentId& Id);
	DEFENDYOURKINGDOM_API TConstArrayView<FDYKResourceId> Resources();
	DEFENDYOURKINGDOM_API int64 ResourceCapacity(FDYKResourceId Id);
	DEFENDYOURKINGDOM_API const TCHAR* ResourceName(FDYKResourceId Id);
}
