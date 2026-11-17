#include "DYKMissionRewards.h"
#include "DYKCityEconomy.h"
#include "../Battle/DYKBattleSession.h"
#include "../Saving/DYKProfileService.h"

namespace
{
	bool ValidResources(const TArray<FDYKResourceSaveData>& Resources)
	{
		TSet<FDYKContentId> Seen;
		for (const auto& Row : Resources)
		{
			if (!FDYKResourceId::FromContentId(Row.ResourceId) || Row.Amount <= 0 || Seen.Contains(Row.ResourceId)) return false;
			Seen.Add(Row.ResourceId);
		}
		return true;
	}

	bool ValidProfile(const FDYKPlayerProfileSaveData& P)
	{
		if (!DYKCityEconomy::Validate(P)) return false;
		TSet<FDYKContentId> Resources;
		for (const auto& Row : P.Resources)
		{
			if (!FDYKResourceId::FromContentId(Row.ResourceId) || Row.Amount < 0 || Resources.Contains(Row.ResourceId)) return false;
			Resources.Add(Row.ResourceId);
		}
		TSet<FName> Rewards;
		for (const auto& Row : P.MissionRewards)
		{
			if (Row.RewardId.IsNone() || Rewards.Contains(Row.RewardId) ||
				!FDYKCampaignId::FromContentId(Row.CampaignId) || !ValidResources(Row.Remaining)) return false;
			Rewards.Add(Row.RewardId);
		}
		return true;
	}

	int64 Room(const FDYKPlayerProfileSaveData& P, const FDYKContentId& Resource)
	{
		const FDYKResourceId Id = FDYKResourceId::FromContentId(Resource).GetValue();
		const int64 Configured = DYKCityEconomy::Capacity(P,Id);
		// Arbitrary resource entities are allowed. Only warehouse resources have a configured cap.
		return (Configured > 0 ? Configured : MAX_int64) - DYKCityEconomy::Balance(P, Id);
	}

	bool Deliver(FDYKPlayerProfileSaveData& P, FDYKMissionRewardSaveData& Receipt)
	{
		bool Changed = false;
		for (auto& Row : Receipt.Remaining)
		{
			const int64 Amount = FMath::Min(Row.Amount, Room(P, Row.ResourceId));
			if (Amount <= 0) continue;
			auto* Balance = P.Resources.FindByPredicate([&](const auto& R) { return R.ResourceId == Row.ResourceId; });
			if (!Balance) Balance = &P.Resources.Add_GetRef({Row.ResourceId, 0});
			Balance->Amount += Amount;
			Row.Amount -= Amount;
			Changed = true;
		}
		Receipt.Remaining.RemoveAll([](const auto& R) { return R.Amount == 0; });
		return Changed;
	}
}

const FDYKMissionRewardDefinition& DYKMissionRewards::PolygonDEV_PLACEHOLDER()
{
	// Replaceable test content, not a final campaign mission or approved balance.
	static const FDYKMissionRewardDefinition Definition = {
		FDYKCampaignId(TEXT("DEV-POLYGON-CAMPAIGN")), FDYKMissionId(TEXT("DEV-POLYGON-MISSION")),
		TEXT("DEV-POLYGON-FIRST-VICTORY"), {{FDYKResourceId(TEXT("RES-WOOD")).ToContentId(), 25},
		{FDYKResourceId(TEXT("RES-GOLD")).ToContentId(), 10}}};
	return Definition;
}

bool DYKMissionRewards::Settle(IDYKProfileService& Service, const FDYKBattleResult& Result,
	const FDYKMissionRewardDefinition& Def)
{
	if (Result.Outcome != EDYKBattlePhase::Victory || !Result.AttemptId.IsValid() ||
		!Def.Campaign.IsValid() || !Def.Mission.IsValid() || Def.RewardId.IsNone() || !ValidResources(Def.Resources)) return false;
	return Service.ApplyProfileCommand(Result.AttemptId, [&Def](FDYKPlayerProfileSaveData& P)
	{
		// Direct entry to the technical polygon can precede the first city load.
		if (P.CityEconomy.Version == 0 && !DYKCityEconomy::Initialize(P, FDateTime::UtcNow().ToUnixTimestamp())) return false;
		if (!ValidProfile(P)) return false;
		const auto* Issued = P.MissionRewards.FindByPredicate([&](const auto& R) { return R.RewardId == Def.RewardId; });
		if (Issued && Issued->CampaignId != Def.Campaign.ToContentId()) return false;
		if (!P.MissionCompletions.ContainsByPredicate([&](const auto& R)
			{ return R.CampaignId == Def.Campaign.ToContentId() && R.MissionId == Def.Mission.ToContentId(); }))
			P.MissionCompletions.Add({Def.Campaign.ToContentId(), Def.Mission.ToContentId()});
		if (!Issued)
		{
			auto& Receipt = P.MissionRewards.Add_GetRef({Def.RewardId, Def.Campaign.ToContentId(), Def.Resources});
			Deliver(P, Receipt);
		}
		return true;
	});
}

bool DYKMissionRewards::HasCollectable(const FDYKPlayerProfileSaveData& P, FDYKCampaignId Campaign)
{
	if (!Campaign.IsValid() || !ValidProfile(P)) return false;
	for (const auto& Receipt : P.MissionRewards)
		if (Receipt.CampaignId == Campaign.ToContentId())
			for (const auto& Row : Receipt.Remaining) if (Room(P, Row.ResourceId) > 0) return true;
	return false;
}

bool DYKMissionRewards::Collect(IDYKProfileService& Service, FGuid Command, FDYKCampaignId Campaign)
{
	return Service.ApplyProfileCommand(Command, [Campaign](FDYKPlayerProfileSaveData& P)
	{
		if (!HasCollectable(P, Campaign)) return false;
		for (auto& Receipt : P.MissionRewards)
			if (Receipt.CampaignId == Campaign.ToContentId()) Deliver(P, Receipt);
		return true;
	});
}
