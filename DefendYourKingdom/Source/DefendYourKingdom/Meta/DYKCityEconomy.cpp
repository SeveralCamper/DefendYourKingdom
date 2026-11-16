#include "DYKCityEconomy.h"
#include "DYKCityBuildingRules.h"
#include "../Saving/DYKProfileService.h"

namespace
{
	const FDYKCityProductionSaveData* Find(const FDYKPlayerProfileSaveData& P, FGuid Id)
	{
		return P.CityEconomy.Instances.FindByPredicate([Id](const auto& Item) { return Item.InstanceId == Id; });
	}
	FDYKCityProductionSaveData Accrue(FDYKCityProductionSaveData State, const FDYKCityBuildingDefinition& Def, int64 Now)
	{
		if (Def.CycleSeconds <= 0) { return State; }
		const int64 Capacity = int64(Def.UnitsPerCycle) * Def.StorageCycles;
		const int64 Time = FMath::Max(Now, State.UpdatedAtUnix);
		// Bound before adding/multiplying, including arbitrary long local offline time.
		const int64 Seconds = FMath::Min(Time - State.UpdatedAtUnix, int64(Def.CycleSeconds) * Def.StorageCycles)
			+ State.RemainderSeconds;
		State.Stored = FMath::Min(Capacity, State.Stored + Seconds / Def.CycleSeconds * Def.UnitsPerCycle);
		State.RemainderSeconds = State.Stored == Capacity ? 0 : int32(Seconds % Def.CycleSeconds);
		State.UpdatedAtUnix = Time;
		return State;
	}
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
	for (const auto& Id : DYKCityCatalog::Resources())
	{
		int32 Count = 0;
		for (const auto& Row : P.Resources)
		{
			if (Row.ResourceId == Id.ToContentId())
			{
				++Count;
				if (Row.Amount < 0 || Row.Amount > DYKCityCatalog::ResourceCapacity(Id)) { return false; }
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
		if (Def->CycleSeconds > 0)
		{
			if (State.Stored > int64(Def->UnitsPerCycle) * Def->StorageCycles
				|| State.RemainderSeconds >= Def->CycleSeconds) { return false; }
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
	if (!Def || !Def->Resource.IsValid()) { return Result; }
	Result.Capacity = int64(Def->UnitsPerCycle) * Def->StorageCycles;
	Result.Stored = Accrue(*State, *Def, Now).Stored;
	if (Now >= State->CollectionStartedAtUnix && Now - State->CollectionStartedAtUnix >= 60)
	{
		Result.Collectable = FMath::Min(Result.Stored, DYKCityCatalog::ResourceCapacity(Def->Resource) - Balance(P, Def->Resource));
	}
	return Result;
}

bool DYKCityEconomy::Collect(IDYKProfileService& Service, FGuid Command, FGuid Instance, int64 Now)
{
	return Service.ApplyProfileCommand(Command, [Instance, Now](FDYKPlayerProfileSaveData& P)
	{
		const auto V = View(P, Instance, Now);
		if (V.Collectable <= 0) { return false; }
		auto* State = P.CityEconomy.Instances.FindByPredicate([Instance](const auto& S) { return S.InstanceId == Instance; });
		const auto& Def = *DYKCityCatalog::Find(P.Buildings[State->BuildingIndex].BuildingId);
		*State = Accrue(*State, Def, Now);
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
