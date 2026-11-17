#include "DYKCityWork.h"
#include "DYKCityEconomy.h"
#include "../Saving/DYKProfileService.h"

namespace
{
	FDYKCityProductionSaveData* WorkInstance(FDYKPlayerProfileSaveData& P,FGuid Id)
	{
		return P.CityEconomy.Instances.FindByPredicate([Id](const auto& S){return S.InstanceId==Id;});
	}
	void RemoveWorkBuilding(FDYKPlayerProfileSaveData& P,FGuid Id)
	{
		const int32 Index=WorkInstance(P,Id)->BuildingIndex;
		P.CityEconomy.Instances.RemoveAll([Id](const auto& S){return S.InstanceId==Id;});
		P.Buildings.RemoveAt(Index);
		for(auto& S:P.CityEconomy.Instances) if(S.BuildingIndex>Index) --S.BuildingIndex;
	}
	void ClaimWorkRefunds(FDYKPlayerProfileSaveData& P)
	{
		for(auto& Refund:P.CityEconomy.PendingRefunds)
		{
			for(auto& R:P.Resources) if(R.ResourceId.Name==Refund.Resource)
			{
				const int64 Room=FMath::Max<int64>(0,DYKCityEconomy::Capacity(P,FDYKResourceId(Refund.Resource))-R.Amount);
				const int64 Part=FMath::Min(Room,Refund.Amount);
				R.Amount+=Part; Refund.Amount-=Part;
			}
		}
		P.CityEconomy.PendingRefunds.RemoveAll([](const auto& R){return R.Amount==0;});
	}
	FDYKCityBuildingWorkRequest WorkRequest(const FDYKPlayerProfileSaveData& P,const FDYKCityWorkSaveData& W)
	{
		const auto* S=P.CityEconomy.Instances.FindByPredicate([&W](const auto& Row){return Row.InstanceId==W.InstanceId;});
		FDYKCityBuildingWorkRequest R;
		if(!S) return R;
		const auto& B=P.Buildings[S->BuildingIndex];
		R.CommandId=W.WorkId; R.InstanceId=W.InstanceId; R.ExpectedLevel=W.ExpectedLevel;
		R.Kind=W.bUpgrade ? EDYKCityBuildingWorkKind::Upgrade : EDYKCityBuildingWorkKind::Build;
		R.BuildingId=FDYKBuildingId(B.BuildingId.Name); R.Origin=B.CellOrigin; R.QuarterTurns=B.RotationQuarterTurns;
		return R;
	}
}

int32 DYKCityWork::Builders(const FDYKPlayerProfileSaveData& P)
{
	// Granted by campaign progression, never by one arbitrary mission victory.
	const bool Campaign=P.UnlockedContent.Contains(FDYKContentId{EDYKContentKind::Campaign,TEXT("DEV-CAMPAIGN-COMPLETED")});
	return Campaign ? 2+int32(P.CityEconomy.bThirdBuilderEntitlement) : 1;
}

bool DYKCityWork::Validate(const FDYKPlayerProfileSaveData& P)
{
	if(!DYKCityEconomy::Validate(P) || P.CityEconomy.WorkUpdatedAtUnix<0) return false;
	TSet<FGuid> WorkIds,Instances;
	int32 Active=0;
	for(const auto& W:P.CityEconomy.Work)
	{
		const auto* S=P.CityEconomy.Instances.FindByPredicate([&W](const auto& Row){return Row.InstanceId==W.InstanceId;});
		if(!W.WorkId.IsValid() || !S || WorkIds.Contains(W.WorkId) || Instances.Contains(W.InstanceId)
			|| W.EnqueuedAtUnix<=0 || W.StartedAtUnix<0 || W.CompletesAtUnix<0) return false;
		WorkIds.Add(W.WorkId); Instances.Add(W.InstanceId);
		const auto& B=P.Buildings[S->BuildingIndex];
		if(W.bUpgrade ? (B.bUnderConstruction || B.Level!=W.ExpectedLevel) : (!B.bUnderConstruction || W.ExpectedLevel!=0 || B.Level!=1)) return false;
		if(W.StartedAtUnix>0)
		{
			++Active;
			if(W.StartedAtUnix<W.EnqueuedAtUnix || W.CompletesAtUnix<=W.StartedAtUnix
				|| B.ConstructionStartedAtUnix!=W.StartedAtUnix || B.ConstructionCompletesAtUnix!=W.CompletesAtUnix) return false;
		}
		else if(W.CompletesAtUnix!=0 || !W.Paid.IsEmpty() || B.ConstructionStartedAtUnix!=0 || B.ConstructionCompletesAtUnix!=0) return false;
		TSet<FName> Costs;
		for(const auto& C:W.Paid)
		{
			if(C.Amount<=0 || DYKCityCatalog::ResourceCapacity(FDYKResourceId(C.Resource))<=0 || Costs.Contains(C.Resource)) return false;
			Costs.Add(C.Resource);
		}
	}
	for(const auto& S:P.CityEconomy.Instances)
	{
		const auto& B=P.Buildings[S.BuildingIndex];
		if((B.bUnderConstruction || B.ConstructionStartedAtUnix!=0 || B.ConstructionCompletesAtUnix!=0) && !Instances.Contains(S.InstanceId)) return false;
	}
	TSet<FName> RefundIds;
	for(const auto& C:P.CityEconomy.PendingRefunds)
	{
		if(C.Amount<=0 || DYKCityCatalog::ResourceCapacity(FDYKResourceId(C.Resource))<=0 || RefundIds.Contains(C.Resource)) return false;
		RefundIds.Add(C.Resource);
	}
	return Active<=Builders(P);
}

bool DYKCityWork::Pay(FDYKPlayerProfileSaveData& P,TConstArrayView<FDYKResourceSaveData> Costs,TArray<FDYKCityWorkCost>& Paid)
{
	TSet<FName> Ids;
	for(const auto& C:Costs)
	{
		const auto Id=FDYKResourceId::FromContentId(C.ResourceId);
		if(!Id || C.Amount<=0 || Ids.Contains(C.ResourceId.Name) || DYKCityEconomy::Balance(P,Id.GetValue())<C.Amount) return false;
		Ids.Add(C.ResourceId.Name);
	}
	Paid.Reset();
	for(const auto& C:Costs) for(auto& R:P.Resources) if(R.ResourceId==C.ResourceId)
	{
		R.Amount-=C.Amount; Paid.Add({C.ResourceId.Name,C.Amount});
	}
	return true;
}

bool DYKCityWork::Refund(FDYKPlayerProfileSaveData& P,TConstArrayView<FDYKCityWorkCost> Paid)
{
	for(const auto& C:Paid)
	{
		auto* Existing=P.CityEconomy.PendingRefunds.FindByPredicate([&C](const auto& R){return R.Resource==C.Resource;});
		if(C.Amount<=0 || (Existing && Existing->Amount>MAX_int64-C.Amount)) return false;
		if(Existing) Existing->Amount+=C.Amount;
		else P.CityEconomy.PendingRefunds.Add(C);
	}
	ClaimWorkRefunds(P);
	return true;
}

bool DYKCityWork::Advance(FDYKPlayerProfileSaveData& P,int64 Now)
{
	if(Now<=0 || !Validate(P)) return false;
	Now=FMath::Max(Now,P.CityEconomy.WorkUpdatedAtUnix);
	int64 Cursor=P.CityEconomy.WorkUpdatedAtUnix>0 ? P.CityEconomy.WorkUpdatedAtUnix : Now;
	bool InitialPass=true;
	for(;;)
	{
		for(int32 I=P.CityEconomy.Work.Num()-1;I>=0;--I)
		{
			const auto W=P.CityEconomy.Work[I];
			if(W.StartedAtUnix==0 || W.CompletesAtUnix>Cursor) continue;
			auto* S=WorkInstance(P,W.InstanceId);
			auto& B=P.Buildings[S->BuildingIndex];
			if(W.bUpgrade)
			{
				if(!DYKCityEconomy::CompleteUpgrade(P,W.InstanceId,W.ExpectedLevel+1,W.CompletesAtUnix)) return false;
			}
			else
			{
				B.bUnderConstruction=false;
				S->UpdatedAtUnix=S->CollectionStartedAtUnix=W.CompletesAtUnix;
			}
			for(const auto& C:W.Paid)
			{
				auto* Receipt=B.PaidCosts.FindByPredicate([&C](const auto& R){return R.ResourceId.Name==C.Resource;});
				if(Receipt) { if(Receipt->Amount>MAX_int64-C.Amount) return false; Receipt->Amount+=C.Amount; }
				else B.PaidCosts.Add({FDYKResourceId(C.Resource).ToContentId(),C.Amount});
			}
			B.ConstructionStartedAtUnix=B.ConstructionCompletesAtUnix=0;
			P.CityEconomy.Work.RemoveAt(I);
		}
		ClaimWorkRefunds(P);
		int32 Active=0; for(const auto& W:P.CityEconomy.Work) Active+=W.StartedAtUnix>0;
		for(auto& W:P.CityEconomy.Work)
		{
			if(Active>=Builders(P)) break;
			if(W.StartedAtUnix>0) continue;
			auto R=WorkRequest(P,W);
			auto Check=P;
			if(!W.bUpgrade) RemoveWorkBuilding(Check,W.InstanceId);
			FDYKCityBuildingWorkPlan Plan;
			if(!DYKCityBuildingRules::CanStart(Check,R,DYKCityTerritory::DEV_PLACEHOLDER(),DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan)) continue;
			// An idle job which became affordable since the last observation starts now.
			// Jobs released by a known completion below start at that offline event.
			const int64 Start=FMath::Max(InitialPass ? Now : Cursor,W.EnqueuedAtUnix);
			if(Plan.DurationSeconds>MAX_int64-Start || !Pay(P,Plan.Costs,W.Paid)) return false;
			W.StartedAtUnix=Start; W.CompletesAtUnix=Start+Plan.DurationSeconds;
			auto& B=P.Buildings[WorkInstance(P,W.InstanceId)->BuildingIndex];
			B.ConstructionStartedAtUnix=Start; B.ConstructionCompletesAtUnix=W.CompletesAtUnix;
			++Active;
		}
		int64 Next=MAX_int64;
		for(const auto& W:P.CityEconomy.Work) if(W.StartedAtUnix>0) Next=FMath::Min(Next,W.CompletesAtUnix);
		if(Next>Now || Next==MAX_int64) break;
		Cursor=Next;
		InitialPass=false;
	}
	P.CityEconomy.WorkUpdatedAtUnix=Now;
	return Validate(P);
}

bool DYKCityWork::Update(IDYKProfileService& Service,int64 Now)
{
	const auto* P=Service.GetProfileSnapshot();
	if(!P) return false;
	if(P->CityEconomy.Work.IsEmpty() && P->CityEconomy.PendingRefunds.IsEmpty()) return Validate(*P);
	auto Candidate=*P;
	if(!Advance(Candidate,Now)) return false;
	// Avoid writing the profile each second just to advance a clock.
	Candidate.CityEconomy.WorkUpdatedAtUnix=P->CityEconomy.WorkUpdatedAtUnix;
	if(FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Candidate,P,0)) return true;
	return Service.ApplyProfileCommand(FGuid::NewGuid(),[Now](auto& State){return Advance(State,Now);});
}

bool DYKCityWork::Enqueue(IDYKProfileService& Service,const FDYKCityBuildingWorkRequest& R,int64 Now)
{
	return Service.ApplyProfileCommand(R.CommandId,[&R,Now](auto& P)
	{
		if(!Advance(P,Now) || P.CityEconomy.Work.ContainsByPredicate([&R](const auto& W){return W.InstanceId==R.InstanceId;})) return false;
		FDYKCityBuildingWorkPlan Plan;
		if(!DYKCityBuildingRules::Prepare(P,R,DYKCityTerritory::DEV_PLACEHOLDER(),DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan)) return false;
		if(R.Kind==EDYKCityBuildingWorkKind::Build)
		{
			FDYKBuildingSaveData B; B.BuildingId=R.BuildingId.ToContentId(); B.CellOrigin=R.Origin;
			B.RotationQuarterTurns=R.QuarterTurns; B.bUnderConstruction=true;
			FDYKCityProductionSaveData S; S.InstanceId=R.InstanceId; S.BuildingIndex=P.Buildings.Add(B);
			S.UpdatedAtUnix=S.CollectionStartedAtUnix=FMath::Max(Now,P.CityEconomy.WorkUpdatedAtUnix);
			P.CityEconomy.Instances.Add(S);
		}
		FDYKCityWorkSaveData W; W.WorkId=R.CommandId; W.InstanceId=R.InstanceId;
		W.bUpgrade=R.Kind==EDYKCityBuildingWorkKind::Upgrade; W.ExpectedLevel=R.ExpectedLevel;
		W.EnqueuedAtUnix=FMath::Max(Now,P.CityEconomy.WorkUpdatedAtUnix); P.CityEconomy.Work.Add(W);
		return Advance(P,Now);
	});
}

bool DYKCityWork::Cancel(IDYKProfileService& Service,FGuid Command,FGuid Work,int64 Now)
{
	return Service.ApplyProfileCommand(Command,[Work,Now](auto& P)
	{
		if(!Advance(P,Now)) return false;
		const int32 Index=P.CityEconomy.Work.IndexOfByPredicate([Work](const auto& W){return W.WorkId==Work;});
		if(Index==INDEX_NONE) return false;
		const auto W=P.CityEconomy.Work[Index];
		if(!Refund(P,W.Paid)) return false;
		if(W.bUpgrade)
		{
			auto& B=P.Buildings[WorkInstance(P,W.InstanceId)->BuildingIndex];
			B.ConstructionStartedAtUnix=B.ConstructionCompletesAtUnix=0;
		}
		else RemoveWorkBuilding(P,W.InstanceId);
		P.CityEconomy.Work.RemoveAt(Index);
		return Advance(P,Now);
	});
}
