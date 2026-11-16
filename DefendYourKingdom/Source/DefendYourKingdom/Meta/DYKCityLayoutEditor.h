#pragma once

#include "DYKCityTerritory.h"

struct FDYKCityLayoutEntry
{
	FGuid InstanceId;
	FIntPoint Origin = FIntPoint::ZeroValue;
	int32 QuarterTurns = 0;
	bool bPlaced = true;
};

// A transient layout transaction. Ownership and economic state never live in the buffer.
class DEFENDYOURKINGDOM_API FDYKCityLayoutEditor
{
public:
	bool Begin(const FDYKPlayerProfileSaveData& Profile);
	bool IsActive() const { return bActive; }
	const TArray<FDYKCityLayoutEntry>& Entries() const { return Draft; }
	bool Store(FGuid Instance);
	bool Place(FGuid Instance, FIntPoint Origin, int32 QuarterTurns);
	bool CanPlace(FGuid Instance, FIntPoint Origin, int32 QuarterTurns) const;
	bool Commit(IDYKProfileService& Service);
	void Cancel();

private:
	bool ValidateDraft(const TArray<FDYKCityLayoutEntry>& Layout, bool RequireAll) const;
	bool MatchesOriginal(const FDYKPlayerProfileSaveData& Profile) const;
	FDYKPlayerProfileSaveData Original;
	TArray<FDYKCityLayoutEntry> Draft;
	FGuid CommandId;
	bool bActive = false;
};
