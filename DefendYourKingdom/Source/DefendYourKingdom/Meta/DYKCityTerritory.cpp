#include "DYKCityTerritory.h"
#include "DYKCityEconomy.h"
#include "DYKMissionRewards.h"
#include "../Saving/DYKProfileService.h"

namespace
{
	bool InRange(FIntPoint P, int32 Side) { return P.X >= 0 && P.Y >= 0 && P.X < Side && P.Y < Side; }
	TArray<FIntPoint> Rectangle(int32 X, int32 Y)
	{
		TArray<FIntPoint> Result;
		for (int32 V = 0; V < Y; ++V) for (int32 U = 0; U < X; ++U) Result.Add({U, V});
		return Result;
	}
	bool ValidParcels(const FDYKPlayerProfileSaveData& P)
	{
		if (P.Parcels.Num() != 25) return false;
		TSet<FIntPoint> Seen;
		for (const auto& Parcel : P.Parcels)
		{
			if (!InRange(Parcel.Coordinate, 5) || Seen.Contains(Parcel.Coordinate)) return false;
			Seen.Add(Parcel.Coordinate);
		}
		return true;
	}
}

const FDYKCityTerritoryConfig& DYKCityTerritory::DEV_PLACEHOLDER()
{
	static const auto Config = []
	{
		FDYKCityTerritoryConfig C;
		for (int32 Y = 1; Y <= 2; ++Y) for (int32 X = 1; X <= 3; ++X) C.InitialParcels.Add({X,Y});
		for (const auto& Def : DYKCityCatalog::Buildings())
			C.Footprints.Add(Def.Id.GetName(), Rectangle(Def.Footprint.X,Def.Footprint.Y));
		C.StarterOrigins = {{TEXT("CITY-CASTLE"),{10,10}}, {TEXT("CITY-SAWMILL"),{22,12}},
			{TEXT("CITY-QUARRY"),{32,12}}, {TEXT("CITY-MILL"),{12,22}}, {TEXT("CITY-HOUSE"),{22,22}}};
		for (int32 Y = 0; Y < 5; ++Y) for (int32 X = 0; X < 5; ++X)
		{
			FDYKCityParcelRule Rule;
			Rule.Costs.Add({{EDYKContentKind::Resource, TEXT("RES-WOOD")}, 5 + (X + Y) * 2});
			if (Y == 4) Rule.Conditions.Add(TEXT("PolygonCompleted"));
			C.ParcelRules.Add({X,Y}, MoveTemp(Rule));
		}
		C.Conditions.Add(TEXT("PolygonCompleted"), [](const FDYKPlayerProfileSaveData& P)
		{
			const auto& Mission = DYKMissionRewards::PolygonDEV_PLACEHOLDER();
			return P.MissionCompletions.ContainsByPredicate([&Mission](const auto& M)
				{ return M.MissionId == Mission.Mission.ToContentId() && M.CampaignId == Mission.Campaign.ToContentId(); });
		});
		C.ConditionLabels.Add(TEXT("PolygonCompleted"),TEXT("Победа на полигоне"));
		return C;
	}();
	return Config;
}

bool DYKCityTerritory::IsUnlocked(const FDYKPlayerProfileSaveData& P, FIntPoint Parcel)
{
	if (!InRange(Parcel, 5)) return false;
	const auto* Row = P.Parcels.FindByPredicate([Parcel](const auto& R) { return R.Coordinate == Parcel; });
	return Row && Row->bIsUnlocked;
}

bool DYKCityTerritory::OccupiedCells(const FDYKBuildingSaveData& B, const FDYKCityTerritoryConfig& C, TArray<FIntPoint>& Cells)
{
	Cells.Reset();
	const auto* Shape = C.Footprints.Find(B.BuildingId.Name);
	if (B.BuildingId.Kind != EDYKContentKind::Building || !Shape || Shape->IsEmpty() || Shape->Num() > 2500
		|| B.RotationQuarterTurns < 0 || B.RotationQuarterTurns > 3 || !InRange(B.CellOrigin, 50)) return false;
	int32 Width = 0, Height = 0;
	TSet<FIntPoint> Seen;
	for (auto Cell : *Shape)
	{
		if (!InRange(Cell, 50) || Seen.Contains(Cell)) return false;
		Seen.Add(Cell); Width = FMath::Max(Width, Cell.X + 1); Height = FMath::Max(Height, Cell.Y + 1);
	}
	for (auto Cell : *Shape)
	{
		switch (B.RotationQuarterTurns)
		{
		case 1: Cell = {Height - 1 - Cell.Y, Cell.X}; break;
		case 2: Cell = {Width - 1 - Cell.X, Height - 1 - Cell.Y}; break;
		case 3: Cell = {Cell.Y, Width - 1 - Cell.X}; break;
		default: break;
		}
		Cell += B.CellOrigin;
		if (!InRange(Cell, 50)) { Cells.Reset(); return false; }
		Cells.Add(Cell);
	}
	return true;
}

bool DYKCityTerritory::Validate(const FDYKPlayerProfileSaveData& P, const FDYKCityTerritoryConfig& C)
{
	if (P.CityEconomy.TerritoryVersion != 1 || !ValidParcels(P)) return false;
	TSet<FIntPoint> Occupancy;
	TArray<FIntPoint> Cells;
	int32 Castles = 0;
	for (const auto& B : P.Buildings)
	{
		if (!OccupiedCells(B, C, Cells)) return false;
		if (B.BuildingId.Name == TEXT("CITY-CASTLE"))
		{
			++Castles;
			// Configuration cannot weaken the approved 10x10 castle footprint.
			if (Cells.Num() != 100) return false;
			for (int32 Y = 0; Y < 10; ++Y) for (int32 X = 0; X < 10; ++X)
				if (!Cells.Contains(B.CellOrigin + FIntPoint(X,Y))) return false;
		}
		for (auto Cell : Cells)
		{
			if (!IsUnlocked(P, {Cell.X / 10, Cell.Y / 10}) || Occupancy.Contains(Cell)) return false;
			Occupancy.Add(Cell);
		}
	}
	return Castles == 1;
}

bool DYKCityTerritory::Initialize(FDYKPlayerProfileSaveData& P, const FDYKCityTerritoryConfig& C)
{
	if (P.CityEconomy.TerritoryVersion != 0) return Validate(P,C);
	if (!DYKCityEconomy::Validate(P) || C.InitialParcels.Num() != 6) return false;
	for (auto Parcel : C.InitialParcels) if (!InRange(Parcel,5)) return false;
	auto Candidate = P;
	TSet<FIntPoint> LegacyUnlocked;
	TSet<FIntPoint> LegacySeen;
	for (const auto& Parcel : P.Parcels)
	{
		if (!InRange(Parcel.Coordinate,5) || LegacySeen.Contains(Parcel.Coordinate)) return false;
		LegacySeen.Add(Parcel.Coordinate);
		if (Parcel.bIsUnlocked) LegacyUnlocked.Add(Parcel.Coordinate);
	}
	Candidate.Parcels.Reset();
	for (int32 Y = 0; Y < 5; ++Y) for (int32 X = 0; X < 5; ++X)
		Candidate.Parcels.Add({{X,Y}, C.InitialParcels.Contains({X,Y}) || LegacyUnlocked.Contains({X,Y})});
	// Adapt pre-grid placement once. Fail atomically if legacy ownership cannot fit.
	Candidate.CityEconomy.TerritoryVersion = 1;
	TSet<FIntPoint> Used;
	TArray<int32> Order;
	for (int32 I = 0; I < Candidate.Buildings.Num(); ++I)
		if (Candidate.Buildings[I].BuildingId.Name == TEXT("CITY-CASTLE")) Order.Insert(I,0); else Order.Add(I);
	for (int32 I : Order)
	{
		auto& B = Candidate.Buildings[I];
		if (B.RotationQuarterTurns < 0 || B.RotationQuarterTurns > 3) return false;
		bool Found = false;
		auto TryOrigin = [&](FIntPoint Origin)
		{
			B.CellOrigin = Origin; TArray<FIntPoint> Cells;
			if (!OccupiedCells(B,C,Cells)) return false;
			for (auto Cell : Cells) if (Used.Contains(Cell) || !IsUnlocked(Candidate,{Cell.X/10,Cell.Y/10})) return false;
			Used.Append(Cells); return true;
		};
		if (const auto* Preferred = C.StarterOrigins.Find(B.BuildingId.Name)) Found = TryOrigin(*Preferred);
		for (int32 Y = 0; Y < 50 && !Found; ++Y) for (int32 X = 0; X < 50 && !Found; ++X)
		{
			Found = TryOrigin({X,Y});
		}
		if (!Found) return false;
	}
	if (!Validate(Candidate,C)) return false;
	P = MoveTemp(Candidate);
	return true;
}

bool DYKCityTerritory::CanUnlock(const FDYKPlayerProfileSaveData& P, FIntPoint Parcel, const FDYKCityTerritoryConfig& C)
{
	if (!InRange(Parcel,5) || !DYKCityEconomy::Validate(P) || !Validate(P,C) || IsUnlocked(P,Parcel)) return false;
	bool Adjacent = false;
	for (auto Delta : {FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1)})
		Adjacent |= IsUnlocked(P,Parcel+Delta);
	const auto* Rule = C.ParcelRules.Find(Parcel);
	if (!Adjacent || !Rule) return false;
	for (FName Key : Rule->Conditions)
	{
		const auto* Check = C.Conditions.Find(Key);
		if (!Check || !*Check || !(*Check)(P)) return false;
	}
	TSet<FName> Resources;
	for (const auto& Cost : Rule->Costs)
	{
		const auto Id = FDYKResourceId::FromContentId(Cost.ResourceId);
		if (!Id || Cost.Amount <= 0 || Resources.Contains(Cost.ResourceId.Name)
			|| DYKCityCatalog::ResourceCapacity(Id.GetValue()) <= 0 || DYKCityEconomy::Balance(P,Id.GetValue()) < Cost.Amount) return false;
		Resources.Add(Cost.ResourceId.Name);
	}
	return true;
}

bool DYKCityTerritory::Unlock(IDYKProfileService& Service, FGuid Command, FIntPoint Parcel, const FDYKCityTerritoryConfig& C)
{
	return Service.ApplyProfileCommand(Command,[&C,Parcel](auto& P)
	{
		if (!CanUnlock(P,Parcel,C)) return false;
		for (const auto& Cost : C.ParcelRules[Parcel].Costs)
			for (auto& R : P.Resources) if (R.ResourceId == Cost.ResourceId) R.Amount -= Cost.Amount;
		for (auto& Row : P.Parcels) if (Row.Coordinate == Parcel) Row.bIsUnlocked=true;
		return Validate(P,C);
	});
}

bool DYKCityTerritory::Move(IDYKProfileService& Service, FGuid Command, FGuid Instance, FIntPoint Origin, int32 Turns, const FDYKCityTerritoryConfig& C)
{
	return Service.ApplyProfileCommand(Command,[&C,Instance,Origin,Turns](auto& P)
	{
		if (!DYKCityEconomy::Validate(P) || !Validate(P,C)) return false;
		const auto* State = P.CityEconomy.Instances.FindByPredicate([Instance](const auto& S) { return S.InstanceId == Instance; });
		if (!State || !P.Buildings.IsValidIndex(State->BuildingIndex)) return false;
		auto& B = P.Buildings[State->BuildingIndex];
		B.CellOrigin=Origin; B.RotationQuarterTurns=Turns;
		return Validate(P,C);
	});
}

FVector DYKCityTerritory::CellCorner(FIntPoint Cell) { return FVector((Cell.X-15)*CellSize,(Cell.Y-15)*CellSize,0.); }
FVector DYKCityTerritory::BuildingLocation(const FDYKBuildingSaveData& B, const FDYKCityTerritoryConfig& C)
{
	TArray<FIntPoint> Cells;
	if (!OccupiedCells(B,C,Cells)) return FVector::ZeroVector;
	FIntPoint Max=B.CellOrigin;
	for (auto Cell : Cells) { Max.X=FMath::Max(Max.X,Cell.X); Max.Y=FMath::Max(Max.Y,Cell.Y); }
	return (CellCorner(B.CellOrigin)+CellCorner(Max+FIntPoint(1,1)))*.5;
}
