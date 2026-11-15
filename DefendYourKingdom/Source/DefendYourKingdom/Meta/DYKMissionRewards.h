#pragma once

#include "../Core/Content/DYKTypedContentId.h"
#include "../Saving/DYKProfileSaveGame.h"

class IDYKProfileService;
struct FDYKBattleResult;

struct FDYKMissionRewardDefinition
{
	FDYKCampaignId Campaign;
	FDYKMissionId Mission;
	// Stable globally unique source, independent of the per-attempt command ID.
	FName RewardId;
	TArray<FDYKResourceSaveData> Resources;
};

namespace DYKMissionRewards
{
	DEFENDYOURKINGDOM_API const FDYKMissionRewardDefinition& PolygonDEV_PLACEHOLDER();
	// Saves completion, issued receipt, delivered resources and overflow together.
	DEFENDYOURKINGDOM_API bool Settle(IDYKProfileService& Service, const FDYKBattleResult& Result,
		const FDYKMissionRewardDefinition& Definition);
	DEFENDYOURKINGDOM_API bool Collect(IDYKProfileService& Service, FGuid Command, FDYKCampaignId Campaign);
	DEFENDYOURKINGDOM_API bool HasCollectable(const FDYKPlayerProfileSaveData& Profile, FDYKCampaignId Campaign);
}
