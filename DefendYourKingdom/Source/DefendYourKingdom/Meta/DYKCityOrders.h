#pragma once
#include "DYKCityEconomy.h"

struct FDYKCityOrderRecipe { int64 Amount; int64 Seconds; };
namespace DYKCityOrders
{
	inline constexpr int32 QueueCapacity=3;
	// Author-requested four order choices; unapproved balance remains replaceable.
	DEFENDYOURKINGDOM_API TConstArrayView<FDYKCityOrderRecipe> DEV_PLACEHOLDER();
	DEFENDYOURKINGDOM_API bool Migrate(FDYKPlayerProfileSaveData& Profile,int64 Now);
	DEFENDYOURKINGDOM_API bool Validate(const FDYKPlayerProfileSaveData& Profile);
	DEFENDYOURKINGDOM_API bool Advance(FDYKPlayerProfileSaveData& Profile,int64 Now);
	DEFENDYOURKINGDOM_API bool Enqueue(IDYKProfileService& Service,FGuid Command,FGuid Instance,int32 Recipe,int64 Now);
	DEFENDYOURKINGDOM_API bool CancelWaiting(IDYKProfileService& Service,FGuid Command,FGuid Instance,FGuid Order,int64 Now);
}
