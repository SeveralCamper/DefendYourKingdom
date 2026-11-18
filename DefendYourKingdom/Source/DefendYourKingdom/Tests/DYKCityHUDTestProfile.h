#pragma once
#include "../Meta/DYKCityOrders.h"
#include "../Meta/DYKCityResearch.h"
#include "../Saving/DYKProfileService.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "../Saving/DYKSaveSubsystem.h"
struct FDYKCityHUDTestAccess
{
	static void Open(UDYKSaveSubsystem& S,const FString& Prefix) {S.SlotPrefix=Prefix; S.LoadOrCreateProfile();}
};
#endif

namespace DYKCityHUDTest
{
	inline constexpr int64 Time=1794700000;
	inline FGuid Instance(const FDYKPlayerProfileSaveData& P,const TCHAR* Type)
	{
		for(const auto& S:P.CityEconomy.Instances) if(P.Buildings[S.BuildingIndex].BuildingId.Name==FName(Type)) return S.InstanceId;
		return {};
	}
	inline bool AddOwned(FDYKPlayerProfileSaveData& P,const TCHAR* Type,int64 Now)
	{
		FDYKBuildingSaveData B; B.BuildingId=FDYKBuildingId(Type).ToContentId();
		bool Found=false;
		for(int32 Y=0;Y<50 && !Found;++Y) for(int32 X=0;X<50 && !Found;++X)
		{
			B.CellOrigin={X,Y}; auto Layout=P.Buildings; Layout.Add(B);
			Found=DYKCityTerritory::ValidateLayout(P,Layout,DYKCityTerritory::DEV_PLACEHOLDER());
		}
		if(!Found) return false;
		FDYKCityProductionSaveData S; S.InstanceId=FGuid::NewGuid(); S.BuildingIndex=P.Buildings.Add(B);
		S.UpdatedAtUnix=S.CollectionStartedAtUnix=Now; P.CityEconomy.Instances.Add(S); return true;
	}
	inline bool Seed(FDYKPlayerProfileSaveData& P,int64 Now=Time,bool Owners=true)
	{
		P={};
		if(!DYKCityEconomy::Initialize(P,Now) || !DYKCityTerritory::Initialize(P,DYKCityTerritory::DEV_PLACEHOLDER())) return false;
		P.Buildings[0].Level=4;
		for(auto& Parcel:P.Parcels) Parcel.bIsUnlocked=true;
		if(Owners) for(const auto Type:{TEXT("CITY-WORKSHOP"),TEXT("CITY-BARRACKS"),TEXT("CITY-LIBRARY"),TEXT("CITY-TAVERN"),TEXT("CITY-WIZARD-TOWER")})
			if(!AddOwned(P,Type,Now)) return false;
		for(auto& R:P.Resources) R.Amount=70;
		return DYKCityOrders::Migrate(P,Now) && DYKCityResearch::Advance(P,Now);
	}
	struct Service : IDYKProfileService
	{
		FDYKPlayerProfileSaveData State;
		TArray<FGuid> Applied;
		bool Fail=false;
		Service() {Seed(State);}
		const FDYKPlayerProfileSaveData* GetProfileSnapshot() const override {return &State;}
		int64 GetRevision() const override {return Applied.Num();}
		bool ApplyProfileCommand(const FGuid& Id,TFunctionRef<bool(FDYKPlayerProfileSaveData&)> Command) override
		{
			if(!Id.IsValid()) return false; if(Applied.Contains(Id)) return true;
			auto Candidate=State; if(!Command(Candidate) || Fail) return false;
			State=MoveTemp(Candidate); Applied.Add(Id); return true;
		}
	};
}
