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
		C.StarterOrigins = {{TEXT("CITY-CASTLE"),{11,11}}, {TEXT("CITY-SAWMILL"),{22,12}},
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

bool DYKCityTerritory::PlacementCells(const FDYKBuildingSaveData& B, const FDYKCityTerritoryConfig& C,
	TArray<FIntPoint>& Body,TArray<FIntPoint>& Perimeter)
{
	Body.Reset(); Perimeter.Reset();
	auto Fail=[&]() { Body.Reset(); Perimeter.Reset(); return false; };
	if (!OccupiedCells(B,C,Body)) return Fail();
	if (B.BuildingId.Name==TEXT("CITY-CASTLE"))
	{
		// Road clearance never changes the approved 10x10 body.
		if (Body.Num()!=100) return Fail();
		for (int32 Y=0;Y<10;++Y) for (int32 X=0;X<10;++X)
			if (!Body.Contains(B.CellOrigin+FIntPoint(X,Y))) return Fail();
	}
	TSet<FIntPoint> Taken,Ring; Taken.Append(Body);
	for (const auto Cell:Body) for (int32 Y=-1;Y<=1;++Y) for (int32 X=-1;X<=1;++X)
	{
		const auto Neighbor=Cell+FIntPoint(X,Y);
		if (!InRange(Neighbor,CellsPerSide)) return Fail();
		if (!Taken.Contains(Neighbor)) Ring.Add(Neighbor);
	}
	Perimeter=Ring.Array();
	Perimeter.Sort([](FIntPoint A,FIntPoint B){ return A.Y==B.Y ? A.X<B.X : A.Y<B.Y; });
	return true;
}

bool DYKCityTerritory::ValidateLayout(const FDYKPlayerProfileSaveData& P,
	TConstArrayView<FDYKBuildingSaveData> Buildings,const FDYKCityTerritoryConfig& C)
{
	if (!ValidParcels(P)) return false;
	TSet<FIntPoint> Bodies,Roads;
	for (const auto& B:Buildings)
	{
		TArray<FIntPoint> Body,Perimeter;
		if (!PlacementCells(B,C,Body,Perimeter)) return false;
		for (const auto Cell:Body)
		{
			if (!IsUnlocked(P,{Cell.X/10,Cell.Y/10}) || Bodies.Contains(Cell) || Roads.Contains(Cell)) return false;
		}
		for (const auto Cell:Perimeter)
			if (!IsUnlocked(P,{Cell.X/10,Cell.Y/10}) || Bodies.Contains(Cell)) return false;
		Bodies.Append(Body); Roads.Append(Perimeter);
	}
	return true;
}

bool DYKCityTerritory::Validate(const FDYKPlayerProfileSaveData& P, const FDYKCityTerritoryConfig& C)
{
	return P.CityEconomy.TerritoryVersion==CurrentVersion
		&& P.Buildings.FilterByPredicate([](const auto& B){return B.BuildingId.Name==TEXT("CITY-CASTLE");}).Num()==1
		&& ValidateLayout(P,P.Buildings,C);
}

bool DYKCityTerritory::Initialize(FDYKPlayerProfileSaveData& P, const FDYKCityTerritoryConfig& C)
{
	const int32 Version=P.CityEconomy.TerritoryVersion;
	if (Version==CurrentVersion) return Validate(P,C);
	if (Version<0 || Version>CurrentVersion || !DYKCityEconomy::Validate(P)) return false;
	auto Candidate=P;
	if (Version==0)
	{
		if (C.InitialParcels.Num()!=6) return false;
		for (auto Parcel:C.InitialParcels) if (!InRange(Parcel,5)) return false;
		TSet<FIntPoint> LegacyUnlocked,LegacySeen;
		for (const auto& Parcel:P.Parcels)
		{
			if (!InRange(Parcel.Coordinate,5) || LegacySeen.Contains(Parcel.Coordinate)) return false;
			LegacySeen.Add(Parcel.Coordinate);
			if (Parcel.bIsUnlocked) LegacyUnlocked.Add(Parcel.Coordinate);
		}
		Candidate.Parcels.Reset();
		for (int32 Y=0;Y<5;++Y) for (int32 X=0;X<5;++X)
			Candidate.Parcels.Add({{X,Y},C.InitialParcels.Contains({X,Y}) || LegacyUnlocked.Contains({X,Y})});
	}
	else
	{
		// Verify V1's original body-only contract before attempting clearance repair.
		if (!ValidParcels(P)) return false;
		TSet<FIntPoint> Used;
		for (const auto& B:P.Buildings)
		{
			TArray<FIntPoint> Body;
			if (!OccupiedCells(B,C,Body)) return false;
			for (const auto Cell:Body)
			{
				if (!IsUnlocked(P,{Cell.X/10,Cell.Y/10}) || Used.Contains(Cell)) return false;
				Used.Add(Cell);
			}
		}
	}
	Candidate.CityEconomy.TerritoryVersion=CurrentVersion;
	if (Version==1 && Validate(Candidate,C)) { P=MoveTemp(Candidate); return true; }
	// Largest bodies first; original locations win, then nearest deterministic legal locations.
	TArray<int32> Order;
	for (int32 I=0;I<Candidate.Buildings.Num();++I) Order.Add(I);
	Order.Sort([&](int32 A,int32 B)
	{
		const auto* SA=C.Footprints.Find(Candidate.Buildings[A].BuildingId.Name);
		const auto* SB=C.Footprints.Find(Candidate.Buildings[B].BuildingId.Name);
		const int32 NA=SA ? SA->Num() : 0,NB=SB ? SB->Num() : 0;
		return NA==NB ? A<B : NA>NB;
	});
	TArray<FDYKBuildingSaveData> Placed;
	bool Repaired=true;
	for (const int32 I:Order)
	{
		auto B=Candidate.Buildings[I];
		if (B.RotationQuarterTurns<0 || B.RotationQuarterTurns>3) return false;
		FIntPoint Preferred=B.CellOrigin;
		if (Version==0) if (const auto* Origin=C.StarterOrigins.Find(B.BuildingId.Name)) Preferred=*Origin;
		TArray<FIntPoint> Origins;
		for (int32 Y=0;Y<50;++Y) for (int32 X=0;X<50;++X) Origins.Add({X,Y});
		Origins.Sort([Preferred](FIntPoint A,FIntPoint Other)
		{
			const int64 DA=FMath::Abs(int64(A.X)-Preferred.X)+FMath::Abs(int64(A.Y)-Preferred.Y);
			const int64 DB=FMath::Abs(int64(Other.X)-Preferred.X)+FMath::Abs(int64(Other.Y)-Preferred.Y);
			return DA==DB ? (A.Y==Other.Y ? A.X<Other.X : A.Y<Other.Y) : DA<DB;
		});
		bool Found=false;
		for (const auto Origin:Origins)
		{
			B.CellOrigin=Origin; Placed.Add(B);
			if (ValidateLayout(Candidate,Placed,C)) { Candidate.Buildings[I]=B; Found=true; break; }
			Placed.Pop(EAllowShrinking::No);
		}
		if (!Found) { Repaired=false; break; }
	}
	if (!Repaired || !Validate(Candidate,C))
	{
		// Existing game saves own the five starter types. Their canonical ring-safe layout
		// is a bounded fallback if greedy nearest-position repair runs out of room.
		if (Candidate.Buildings.Num()!=C.StarterOrigins.Num()) return false;
		TSet<FName> Types;
		for (auto& B:Candidate.Buildings)
		{
			const auto* Origin=C.StarterOrigins.Find(B.BuildingId.Name);
			if (!Origin || Types.Contains(B.BuildingId.Name)) return false;
			Types.Add(B.BuildingId.Name); B.CellOrigin=*Origin;
		}
	}
	if (!Validate(Candidate,C)) return false;
	P=MoveTemp(Candidate);
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
