#include "DYKCityOrders.h"
#include "DYKCityResearch.h"
#include "../Saving/DYKProfileService.h"

TConstArrayView<FDYKCityOrderRecipe> DYKCityOrders::DEV_PLACEHOLDER()
{
	static const FDYKCityOrderRecipe Recipes[]={{15,600},{70,3600},{360,21600},{650,43200}};
	return Recipes;
}

bool DYKCityOrders::Validate(const FDYKPlayerProfileSaveData& P)
{
	if(!DYKCityEconomy::Validate(P)) return false;
	for(const auto& S:P.CityEconomy.Instances)
	{
		const auto* D=DYKCityCatalog::Find(P.Buildings[S.BuildingIndex].BuildingId);
		if(S.OrdersVersion<0 || S.OrdersVersion>1 || S.OrdersUpdatedAtUnix<0 || S.Orders.Num()>QueueCapacity) return false;
		if(S.OrdersVersion==0 && (!S.Orders.IsEmpty() || S.OrdersUpdatedAtUnix!=0)) return false;
		if(D->Category!=EDYKCityBuildingCategory::Production && S.OrdersVersion!=0) return false;
		TSet<FGuid> Ids;
		for(int32 I=0;I<S.Orders.Num();++I)
		{
			const auto& O=S.Orders[I];
			if(!O.OrderId.IsValid() || Ids.Contains(O.OrderId) || O.Recipe<0 || O.Recipe>=DEV_PLACEHOLDER().Num()
				|| O.EnqueuedAtUnix<=0 || O.StartedAtUnix<0 || O.CompletesAtUnix<0 || O.RemainingOutput<0) return false;
			Ids.Add(O.OrderId);
			if(O.StartedAtUnix>0)
			{
				if(I!=0 || O.StartedAtUnix<O.EnqueuedAtUnix || O.CompletesAtUnix<=O.StartedAtUnix
					|| O.RemainingOutput>DEV_PLACEHOLDER()[O.Recipe].Amount) return false;
			}
			else if(O.CompletesAtUnix!=0 || O.RemainingOutput!=0) return false;
		}
	}
	return true;
}

bool DYKCityOrders::Migrate(FDYKPlayerProfileSaveData& P,int64 Now)
{
	if(Now<=0 || !Validate(P)) return false;
	for(auto& S:P.CityEconomy.Instances)
	{
		const auto* D=DYKCityCatalog::Find(P.Buildings[S.BuildingIndex].BuildingId);
		if(D->Category!=EDYKCityBuildingCategory::Production || S.OrdersVersion==1) continue;
		// Settle the preceding automatic model once. Preserve every already-produced item.
		S.Stored=DYKCityEconomy::View(P,S.InstanceId,Now).Stored;
		S.UpdatedAtUnix=FMath::Max(Now,S.UpdatedAtUnix); S.RemainderSeconds=0;
		S.OrdersVersion=1; S.OrdersUpdatedAtUnix=S.UpdatedAtUnix;
	}
	return Validate(P);
}

bool DYKCityOrders::Advance(FDYKPlayerProfileSaveData& P,int64 Now)
{
	if(Now<=0 || !Validate(P)) return false;
	for(auto& S:P.CityEconomy.Instances)
	{
		if(S.OrdersVersion!=1) continue;
		const auto& B=P.Buildings[S.BuildingIndex];
		const auto* D=DYKCityCatalog::Find(B.BuildingId);
		const int64 Time=FMath::Max(Now,S.OrdersUpdatedAtUnix);
		const int64 Limit=DYKCityCatalog::Production(D->Id,B.Level).Capacity;
		int64 StartAt=Time;
		while(!B.bUnderConstruction && !S.Orders.IsEmpty())
		{
			auto& O=S.Orders[0];
			if(O.StartedAtUnix==0)
			{
				if(S.Stored>=Limit) break;
				const auto Recipe=DEV_PLACEHOLDER()[O.Recipe];
				const int64 Begin=FMath::Max(StartAt,O.EnqueuedAtUnix);
				if(Recipe.Seconds>MAX_int64-Begin) return false;
				O.StartedAtUnix=Begin; O.CompletesAtUnix=Begin+Recipe.Seconds; O.RemainingOutput=Recipe.Amount;
			}
			if(O.CompletesAtUnix>Time) break;
			const int64 Part=FMath::Min(O.RemainingOutput,FMath::Max<int64>(0,Limit-S.Stored));
			S.Stored+=Part; O.RemainingOutput-=Part;
			if(Part>0) S.CollectionStartedAtUnix=FMath::Min(S.CollectionStartedAtUnix,O.StartedAtUnix);
			if(O.RemainingOutput>0) break;
			// Work released by a completion can continue during the same offline interval.
			StartAt=FMath::Max(O.CompletesAtUnix,S.OrdersUpdatedAtUnix);
			S.Orders.RemoveAt(0);
		}
		S.OrdersUpdatedAtUnix=Time; S.UpdatedAtUnix=FMath::Max(Time,S.UpdatedAtUnix);
	}
	return Validate(P);
}

bool DYKCityOrders::Enqueue(IDYKProfileService& Service,FGuid Command,FGuid Instance,int32 Recipe,int64 Now)
{
	return Service.ApplyProfileCommand(Command,[=](auto& P)
	{
		if(Recipe<0 || Recipe>=DEV_PLACEHOLDER().Num() || !Migrate(P,Now) || !DYKCityResearch::Advance(P,Now)) return false;
		auto* S=P.CityEconomy.Instances.FindByPredicate([Instance](const auto& Row){return Row.InstanceId==Instance;});
		if(!S || S->OrdersVersion!=1 || P.Buildings[S->BuildingIndex].bUnderConstruction || S->Orders.Num()>=QueueCapacity) return false;
		FDYKCityOrderSaveData O; O.OrderId=Command; O.Recipe=Recipe; O.EnqueuedAtUnix=FMath::Max(Now,S->OrdersUpdatedAtUnix);
		S->Orders.Add(O);
		return Advance(P,Now);
	});
}

bool DYKCityOrders::CancelWaiting(IDYKProfileService& Service,FGuid Command,FGuid Instance,FGuid Order,int64 Now)
{
	return Service.ApplyProfileCommand(Command,[=](auto& P)
	{
		if(!DYKCityResearch::Advance(P,Now)) return false;
		auto* S=P.CityEconomy.Instances.FindByPredicate([Instance](const auto& Row){return Row.InstanceId==Instance;});
		if(!S) return false;
		const int32 I=S->Orders.IndexOfByPredicate([Order](const auto& O){return O.OrderId==Order;});
		if(I==INDEX_NONE || S->Orders[I].StartedAtUnix!=0) return false;
		S->Orders.RemoveAt(I); return Advance(P,Now);
	});
}

bool DYKCityOrders::CancelCurrent(IDYKProfileService& Service,FGuid Command,FGuid Instance,FGuid Order,int64 Now)
{
 return Service.ApplyProfileCommand(Command,[=](auto& P)
 {
  if(!DYKCityResearch::Advance(P,Now)) return false;
  auto* S=P.CityEconomy.Instances.FindByPredicate([Instance](const auto& Row){return Row.InstanceId==Instance;});
  if(!S || S->Orders.IsEmpty() || S->Orders[0].OrderId!=Order) return false;
  // Advance settles completed output first; a stale click cannot cancel its successor.
  S->Orders.RemoveAt(0);
  return Advance(P,Now);
 });
}
