#pragma once
#include "DYKCityBuildingRules.h"

class IDYKProfileService;

namespace DYKCityWork
{
	DEFENDYOURKINGDOM_API int32 Builders(const FDYKPlayerProfileSaveData& Profile);
	DEFENDYOURKINGDOM_API bool Validate(const FDYKPlayerProfileSaveData& Profile);
	// Mutates only a transaction candidate. Replays completions chronologically, including offline starts.
	DEFENDYOURKINGDOM_API bool Advance(FDYKPlayerProfileSaveData& Profile, int64 Now);
	DEFENDYOURKINGDOM_API bool Update(IDYKProfileService& Service, int64 Now);
	DEFENDYOURKINGDOM_API bool Enqueue(IDYKProfileService& Service, const FDYKCityBuildingWorkRequest& Request, int64 Now);
	DEFENDYOURKINGDOM_API bool Cancel(IDYKProfileService& Service, FGuid Command, FGuid Work, int64 Now);
	DEFENDYOURKINGDOM_API bool Pay(FDYKPlayerProfileSaveData& Profile, TConstArrayView<FDYKResourceSaveData> Costs, TArray<FDYKCityWorkCost>& Paid);
	DEFENDYOURKINGDOM_API bool Refund(FDYKPlayerProfileSaveData& Profile, TConstArrayView<FDYKCityWorkCost> Paid);
}
