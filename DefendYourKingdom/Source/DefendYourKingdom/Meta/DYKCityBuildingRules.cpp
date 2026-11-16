#include "DYKCityBuildingRules.h"
#include "DYKCityEconomy.h"

namespace
{
	bool ValidCosts(TConstArrayView<FDYKResourceSaveData> Costs)
	{
		TSet<FName> Seen;
		for (const auto& Cost : Costs)
		{
			const auto Id = FDYKResourceId::FromContentId(Cost.ResourceId);
			if (!Id || Cost.Amount <= 0 || Seen.Contains(Cost.ResourceId.Name)
				|| DYKCityCatalog::ResourceCapacity(Id.GetValue()) <= 0) return false;
			Seen.Add(Cost.ResourceId.Name);
		}
		return true;
	}

	const FDYKBuildingSaveData* FindInstance(const FDYKPlayerProfileSaveData& P, FGuid Id)
	{
		if (!Id.IsValid()) return nullptr;
		const auto* State = P.CityEconomy.Instances.FindByPredicate([Id](const auto& S) { return S.InstanceId == Id; });
		return State && P.Buildings.IsValidIndex(State->BuildingIndex) ? &P.Buildings[State->BuildingIndex] : nullptr;
	}

	int32 CastleLevel(const FDYKPlayerProfileSaveData& P)
	{
		int32 Level = 0;
		for (const auto& B : P.Buildings)
		{
			if (B.BuildingId == FDYKBuildingId(TEXT("CITY-CASTLE")).ToContentId())
			{
				if (Level != 0 || B.Level <= 0) return 0;
				Level = B.Level;
			}
		}
		return Level;
	}

	int32 Limit(const FDYKCityBuildingDefinition& Def, const FDYKCityBuildingProgression& Rule, int32 Castle)
	{
		if (Def.Category == EDYKCityBuildingCategory::Castle || Def.Category == EDYKCityBuildingCategory::Unique) return 1;
		if (!Rule.LimitsByCastleLevel.IsValidIndex(Castle - 1)) return 0;
		const int32 Count = Rule.LimitsByCastleLevel[Castle - 1];
		// Author-approved house limit must not be replaced by placeholder configuration.
		if (Def.Category == EDYKCityBuildingCategory::House && Castle == 1 && Count != 3) return 0;
		return FMath::Max(0, Count);
	}
}

const FDYKCityBuildingRulesConfig& DYKCityBuildingRules::DEV_PLACEHOLDER()
{
	static const auto Config = []
	{
		FDYKCityBuildingRulesConfig C;
		// All prices, positive durations, level counts, availability and unapproved
		// instance limits here are replaceable DEV_PLACEHOLDER data, not final balance.
		for (const auto& Def : DYKCityCatalog::Buildings())
		{
			FDYKCityBuildingProgression Rule;
			const int32 MaxLevel = Def.Category == EDYKCityBuildingCategory::Castle ? 5
				: Def.Id.GetName() == TEXT("CITY-WAREHOUSE") ? 4
				: Def.Category == EDYKCityBuildingCategory::Decoration ? 1 : 3;
			for (int32 Level = 1; Level <= MaxLevel; ++Level)
			{
				FDYKCityBuildingLevelRule Step;
				Step.RequiredCastleLevel = Def.Category == EDYKCityBuildingCategory::Castle ? FMath::Max(1,Level-1) : Level;
				Step.DurationSeconds = 15 * Level;
				Step.Costs.Add({FDYKResourceId(TEXT("RES-WOOD")).ToContentId(), 5 * Level});
				Step.Costs.Add({FDYKResourceId(TEXT("RES-STONE")).ToContentId(), 3 * Level});
				Rule.Levels.Add(MoveTemp(Step));
			}
			if (Def.Category == EDYKCityBuildingCategory::House) Rule.LimitsByCastleLevel = {3,4,5,6,7};
			if (Def.Category == EDYKCityBuildingCategory::Production) Rule.LimitsByCastleLevel = {2,3,4,5,6};
			if (Def.Category == EDYKCityBuildingCategory::Decoration) Rule.LimitsByCastleLevel = {25,25,25,25,25};
			C.Buildings.Add(Def.Id.GetName(), MoveTemp(Rule));
		}
		return C;
	}();
	return Config;
}

bool DYKCityBuildingRules::ValidateOwnership(const FDYKPlayerProfileSaveData& P, const FDYKCityBuildingRulesConfig& C)
{
	const int32 Castle = CastleLevel(P);
	if (Castle <= 0) return false;
	TMap<FName,int32> Counts;
	for (const auto& B : P.Buildings)
	{
		const auto* Def = DYKCityCatalog::Find(B.BuildingId);
		const auto* Rule = C.Buildings.Find(B.BuildingId.Name);
		if (!Def || !Rule || B.Level <= 0 || B.Level > Rule->Levels.Num() || !ValidCosts(B.PaidCosts)) return false;
		++Counts.FindOrAdd(Def->Id.GetName());
	}
	for (const auto& Def : DYKCityCatalog::Buildings())
	{
		const auto* Rule = C.Buildings.Find(Def.Id.GetName());
		const int32 Count = Counts.FindRef(Def.Id.GetName());
		if (Def.bInitiallyOwned && Count == 0) return false;
		if (Count > 0 && (!Rule || Count > Limit(Def,*Rule,Castle))) return false;
	}
	return true;
}

bool DYKCityBuildingRules::Prepare(const FDYKPlayerProfileSaveData& P, const FDYKCityBuildingWorkRequest& Request,
	const FDYKCityTerritoryConfig& Territory, const FDYKCityBuildingRulesConfig& C, FDYKCityBuildingWorkPlan& Plan)
{
	// Callers may revalidate their existing Plan.Request into the same Plan.
	auto R = Request;
	Plan = {};
	if (!R.CommandId.IsValid() || !R.InstanceId.IsValid() || !DYKCityEconomy::Validate(P,C)
		|| !DYKCityTerritory::Validate(P,Territory)) return false;
	const auto* Def = DYKCityCatalog::Find(R.BuildingId.ToContentId());
	const auto* Rule = C.Buildings.Find(R.BuildingId.GetName());
	if (!Def || !Rule) return false;
	int32 TargetLevel = 1;
	if (R.Kind == EDYKCityBuildingWorkKind::Build)
	{
		if (R.ExpectedLevel != 0 || FindInstance(P,R.InstanceId)) return false;
		int32 Count = 0;
		for (const auto& B : P.Buildings) Count += B.BuildingId == Def->Id.ToContentId() ? 1 : 0;
		if (Count >= Limit(*Def,*Rule,CastleLevel(P))) return false;
		auto Candidate = P;
		FDYKBuildingSaveData B;
		B.BuildingId = Def->Id.ToContentId(); B.CellOrigin = R.Origin; B.RotationQuarterTurns = R.QuarterTurns;
		Candidate.Buildings.Add(B);
		if (!DYKCityTerritory::Validate(Candidate,Territory)) return false;
	}
	else if (R.Kind == EDYKCityBuildingWorkKind::Upgrade)
	{
		const auto* B = FindInstance(P,R.InstanceId);
		if (!B || B->BuildingId != Def->Id.ToContentId() || B->Level != R.ExpectedLevel
			|| B->ConstructionStartedAtUnix != 0 || B->ConstructionCompletesAtUnix != 0
			|| B->Level >= Rule->Levels.Num()) return false;
		R.Origin = B->CellOrigin;
		R.QuarterTurns = B->RotationQuarterTurns;
		TargetLevel = B->Level + 1;
	}
	else return false;
	if (!Rule->Levels.IsValidIndex(TargetLevel-1)) return false;
	const auto& Step = Rule->Levels[TargetLevel-1];
	if (Step.RequiredCastleLevel <= 0 || CastleLevel(P) < Step.RequiredCastleLevel
		|| Step.DurationSeconds <= 0 || !ValidCosts(Step.Costs)) return false;
	for (const FName Key : Step.Conditions)
	{
		const auto* Check = C.Conditions.Find(Key);
		if (!Check || !*Check || !(*Check)(P)) return false;
	}
	Plan.Request = R;
	Plan.TargetLevel = TargetLevel;
	Plan.DurationSeconds = Step.DurationSeconds;
	Plan.Costs = Step.Costs;
	return true;
}

bool DYKCityBuildingRules::CanStart(const FDYKPlayerProfileSaveData& P, const FDYKCityBuildingWorkRequest& R,
	const FDYKCityTerritoryConfig& Territory, const FDYKCityBuildingRulesConfig& C, FDYKCityBuildingWorkPlan& Plan)
{
	if (!Prepare(P,R,Territory,C,Plan)) return false;
	for (const auto& Cost : Plan.Costs)
	{
		if (DYKCityEconomy::Balance(P,FDYKResourceId::FromContentId(Cost.ResourceId).GetValue()) < Cost.Amount)
		{
			Plan = {};
			return false;
		}
	}
	return true;
}

bool DYKCityBuildingRules::CanSell(const FDYKPlayerProfileSaveData& P, FGuid Instance)
{
	if (!DYKCityEconomy::Validate(P)) return false;
	const auto* B = FindInstance(P,Instance);
	const auto* Def = B ? DYKCityCatalog::Find(B->BuildingId) : nullptr;
	return Def && Def->Category == EDYKCityBuildingCategory::Decoration
		&& B->ConstructionStartedAtUnix == 0 && B->ConstructionCompletesAtUnix == 0;
}

bool DYKCityBuildingRules::PrepareSale(const FDYKPlayerProfileSaveData& P, FGuid Instance, TArray<FDYKResourceSaveData>& Refund)
{
	Refund.Reset();
	if (!CanSell(P,Instance)) return false;
	Refund = FindInstance(P,Instance)->PaidCosts;
	return true;
}
