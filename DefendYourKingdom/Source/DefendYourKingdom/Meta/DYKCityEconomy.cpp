#include "DYKCityEconomy.h"
#include "DYKCityBuildingRules.h"
#include "DYKCityWork.h"
#include "DYKCityResearch.h"
#include "../Saving/DYKProfileService.h"

namespace
{
	const FDYKCityProductionSaveData* Find(const FDYKPlayerProfileSaveData& P, FGuid Id)
	{
		return P.CityEconomy.Instances.FindByPredicate([Id](const auto& Item) { return Item.InstanceId == Id; });
	}
	FDYKCityProductionSaveData Accrue(FDYKCityProductionSaveData State, const FDYKCityProductionLevel& Def, int64 Now)
	{
		if (State.OrdersVersion==1) return State;
		if (Def.CycleSeconds <= 0) { return State; }
		const int64 Capacity = Def.Capacity;
		const int64 Time = FMath::Max(Now, State.UpdatedAtUnix);
		if (State.Stored >= Capacity)
		{
			State.UpdatedAtUnix = Time;
			State.RemainderSeconds = 0;
			return State;
		}
		// Bound before adding/multiplying, including arbitrary long local offline time.
		const int64 Seconds = FMath::Min(Time - State.UpdatedAtUnix, (Capacity / Def.UnitsPerCycle + 1) * Def.CycleSeconds)
			+ State.RemainderSeconds;
		State.Stored = FMath::Min(Capacity, State.Stored + Seconds / Def.CycleSeconds * Def.UnitsPerCycle);
		State.RemainderSeconds = State.Stored == Capacity ? 0 : int32(Seconds % Def.CycleSeconds);
		State.UpdatedAtUnix = Time;
		return State;
	}
}

int64 DYKCityEconomy::Capacity(const FDYKPlayerProfileSaveData& P, FDYKResourceId Id)
{
	int32 Level = 0;
	for (const auto& B : P.Buildings)
		if (B.BuildingId.Name == TEXT("CITY-WAREHOUSE") && !B.bUnderConstruction) Level = B.Level;
	return DYKCityCatalog::WarehouseCapacityDEV_PLACEHOLDER(Id,Level);
}

int64 DYKCityEconomy::Balance(const FDYKPlayerProfileSaveData& P, FDYKResourceId Id)
{
	const auto* R = P.Resources.FindByPredicate([Id](const auto& Row) { return Row.ResourceId == Id.ToContentId(); });
	return R ? R->Amount : 0;
}

bool DYKCityEconomy::Validate(const FDYKPlayerProfileSaveData& P)
{
	return Validate(P,DYKCityBuildingRules::DEV_PLACEHOLDER());
}

bool DYKCityEconomy::Validate(const FDYKPlayerProfileSaveData& P, const FDYKCityBuildingRulesConfig& Config)
{
	if (P.CityEconomy.Version != 1) { return false; }
	TSet<FDYKContentId> SeenResourceIds;
	for (const auto& Row:P.Resources)
	{
		if (!FDYKResourceId::FromContentId(Row.ResourceId) || SeenResourceIds.Contains(Row.ResourceId)
			|| Row.Amount<0 || Row.Amount>MAX_uint32) return false;
		SeenResourceIds.Add(Row.ResourceId);
	}
	for (const auto& Id : DYKCityCatalog::Resources())
	{
		int32 Count = 0;
		for (const auto& Row : P.Resources)
		{
			if (Row.ResourceId == Id.ToContentId())
			{
				++Count;
				if (Row.Amount < 0 || Row.Amount > Capacity(P,Id)) { return false; }
			}
		}
		if (Count != 1) { return false; }
	}
	TSet<FGuid> Ids;
	TSet<int32> Indices;
	for (const auto& State : P.CityEconomy.Instances)
	{
		if (!State.InstanceId.IsValid() || Ids.Contains(State.InstanceId) || Indices.Contains(State.BuildingIndex)
			|| !P.Buildings.IsValidIndex(State.BuildingIndex)) { return false; }
		Ids.Add(State.InstanceId);
		Indices.Add(State.BuildingIndex);
		const auto& Building = P.Buildings[State.BuildingIndex];
		const auto* Def = DYKCityCatalog::Find(Building.BuildingId);
		if (!Def || State.Stored < 0 || State.UpdatedAtUnix <= 0
			|| State.CollectionStartedAtUnix <= 0 || State.CollectionStartedAtUnix > State.UpdatedAtUnix
			|| State.RemainderSeconds < 0) { return false; }
		const auto Production = DYKCityCatalog::Production(Def->Id,Building.Level);
		if (Production.CycleSeconds > 0)
		{
			// Retain existing goods even if a later content configuration lowers capacity.
			if (State.Stored > MAX_uint32 || State.RemainderSeconds >= Production.CycleSeconds) { return false; }
		}
		else if (State.Stored != 0 || State.RemainderSeconds != 0) { return false; }
	}
	for (int32 Index = 0; Index < P.Buildings.Num(); ++Index)
	{
		if (DYKCityCatalog::Find(P.Buildings[Index].BuildingId) && !Indices.Contains(Index)) { return false; }
	}
	return DYKCityBuildingRules::ValidateOwnership(P,Config);
}

bool DYKCityEconomy::Initialize(FDYKPlayerProfileSaveData& P, int64 Now)
{
	if (P.CityEconomy.Version != 0) { return Validate(P); }
	if (Now <= 0 || !P.CityEconomy.Instances.IsEmpty()) { return false; }
	for (const auto& Def : DYKCityCatalog::Buildings())
	{
		if (!Def.bInitiallyOwned) continue;
		if (!P.Buildings.ContainsByPredicate([&Def](const auto& B) { return B.BuildingId == Def.Id.ToContentId(); }))
		{
			FDYKBuildingSaveData B;
			B.BuildingId = Def.Id.ToContentId();
			B.CellOrigin = Def.StarterCell;
			P.Buildings.Add(B);
		}
	}
	for (int32 Index = 0; Index < P.Buildings.Num(); ++Index)
	{
		if (DYKCityCatalog::Find(P.Buildings[Index].BuildingId))
		{
			FDYKCityProductionSaveData State;
			State.InstanceId = FGuid::NewGuid();
			State.BuildingIndex = Index;
			State.UpdatedAtUnix = State.CollectionStartedAtUnix = Now;
			P.CityEconomy.Instances.Add(State);
		}
	}
	for (const auto& Id : DYKCityCatalog::Resources())
	{
		if (!P.Resources.ContainsByPredicate([Id](const auto& R) { return R.ResourceId == Id.ToContentId(); }))
		{
			P.Resources.Add({Id.ToContentId(), 0});
		}
	}
	P.CityEconomy.Version = 1;
	return Validate(P);
}

FDYKCityCollectionView DYKCityEconomy::View(const FDYKPlayerProfileSaveData& P, FGuid Instance, int64 Now)
{
	FDYKCityCollectionView Result;
	const auto* State = Find(P, Instance);
	if (!State || !Validate(P)) { return Result; }
	const auto* Def = DYKCityCatalog::Find(P.Buildings[State->BuildingIndex].BuildingId);
	if (!Def || !Def->Resource.IsValid() || P.Buildings[State->BuildingIndex].bUnderConstruction) { return Result; }
	const auto Production = DYKCityCatalog::Production(Def->Id,P.Buildings[State->BuildingIndex].Level);
	Result.Capacity = Production.Capacity;
	Result.Stored = Accrue(*State, Production, Now).Stored;
	if (Now >= State->CollectionStartedAtUnix && Now - State->CollectionStartedAtUnix >= 60)
	{
		Result.Collectable = FMath::Min(Result.Stored, Capacity(P,Def->Resource) - Balance(P, Def->Resource));
	}
	return Result;
}

bool DYKCityEconomy::Collect(IDYKProfileService& Service, FGuid Command, FGuid Instance, int64 Now)
{
	return Service.ApplyProfileCommand(Command, [Instance, Now](FDYKPlayerProfileSaveData& P)
	{
		if (!DYKCityResearch::Advance(P,Now)) return false;
		const auto V = View(P, Instance, Now);
		if (V.Collectable <= 0) { return false; }
		auto* State = P.CityEconomy.Instances.FindByPredicate([Instance](const auto& S) { return S.InstanceId == Instance; });
		const auto& Def = *DYKCityCatalog::Find(P.Buildings[State->BuildingIndex].BuildingId);
		*State = Accrue(*State, DYKCityCatalog::Production(Def.Id,P.Buildings[State->BuildingIndex].Level), Now);
		State->Stored -= V.Collectable;
		if (State->Stored == 0)
		{
			State->CollectionStartedAtUnix = State->UpdatedAtUnix;
			// Keep work already spent on the next incomplete production cycle.
		}
		for (auto& R : P.Resources) { if (R.ResourceId == Def.Resource.ToContentId()) { R.Amount += V.Collectable; } }
		return true;
	});
}

bool DYKCityEconomy::CompleteUpgrade(FDYKPlayerProfileSaveData& P, FGuid Instance, int32 TargetLevel, int64 Now)
{
	if (!Validate(P) || Now <= 0) return false;
	auto* State = P.CityEconomy.Instances.FindByPredicate([Instance](const auto& S) { return S.InstanceId == Instance; });
	if (!State || Now < State->UpdatedAtUnix) return false;
	auto& B = P.Buildings[State->BuildingIndex];
	const auto* Rule = DYKCityBuildingRules::DEV_PLACEHOLDER().Buildings.Find(B.BuildingId.Name);
	if (!Rule || TargetLevel != B.Level+1 || TargetLevel > Rule->Levels.Num()) return false;
	const auto Id = FDYKBuildingId::FromContentId(B.BuildingId).GetValue();
	const auto Old = DYKCityCatalog::Production(Id,B.Level);
	const auto Next = DYKCityCatalog::Production(Id,TargetLevel);
	*State = Accrue(*State,Old,Now);
	if (Old.CycleSeconds > 0 && Next.CycleSeconds > 0)
		State->RemainderSeconds = int32(int64(State->RemainderSeconds) * Next.CycleSeconds / Old.CycleSeconds);
	B.Level = TargetLevel;
	return true;
}

bool DYKCityEconomy::Spend(IDYKProfileService& Service, FGuid Command, FDYKResourceId Resource, int64 Amount)
{
	return Service.ApplyProfileCommand(Command, [Resource, Amount](FDYKPlayerProfileSaveData& P)
	{
		if (Amount <= 0 || DYKCityCatalog::ResourceCapacity(Resource) <= 0 || !Validate(P)) { return false; }
		for (auto& Row : P.Resources)
		{
			if (Row.ResourceId == Resource.ToContentId())
			{
				if (Row.Amount < Amount) { return false; }
				Row.Amount -= Amount;
				return true;
			}
		}
		return false;
	});
}
