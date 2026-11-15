#pragma once

#include "Containers/ArrayView.h"
#include "DYKUnitDefinition.h"

namespace DYKUnitIds
{
	// Stable keys: never derive them from display names or reuse them for another type.
	DEFENDYOURKINGDOM_API const FDYKUnitTypeId& Peasant();
	DEFENDYOURKINGDOM_API const FDYKUnitTypeId& Archer();
	DEFENDYOURKINGDOM_API const FDYKUnitTypeId& Goblin();
	DEFENDYOURKINGDOM_API const FDYKUnitTypeId& GoblinAssassin();
}

namespace DYKUnitCatalog
{
	// No registration/update API. Returned addresses remain valid for the process lifetime.
	DEFENDYOURKINGDOM_API TConstArrayView<FDYKUnitDefinition> GetAll();
	DEFENDYOURKINGDOM_API const FDYKUnitDefinition* Find(FDYKUnitTypeId Id);
	// Checked adapter for persistence/asset boundaries; rejects another category.
	DEFENDYOURKINGDOM_API const FDYKUnitDefinition* Find(const FDYKContentId& Id);
}
