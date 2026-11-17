#include "../Meta/DYKCityWork.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Meta/DYKCityLayoutEditor.h"
#include "../Saving/DYKProfileService.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr auto WorkTestFlags=EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int64 WorkTestTime=1794700000;
	struct FWorkTestService : IDYKProfileService
	{
		FDYKPlayerProfileSaveData State;
		TArray<FGuid> Commands;
		bool bFail=false;
		FWorkTestService()
		{
			DYKCityEconomy::Initialize(State,WorkTestTime);
			DYKCityTerritory::Initialize(State,DYKCityTerritory::DEV_PLACEHOLDER());
			for(auto& R:State.Resources) R.Amount=100;
		}
		virtual const FDYKPlayerProfileSaveData* GetProfileSnapshot() const override {return &State;}
		virtual int64 GetRevision() const override {return Commands.Num();}
		virtual bool ApplyProfileCommand(const FGuid& Id,TFunctionRef<bool(FDYKPlayerProfileSaveData&)> Command) override
		{
			if(!Id.IsValid()) return false;
			if(Commands.Contains(Id)) return true;
			auto Candidate=State;
			if(!Command(Candidate) || bFail) return false;
			State=MoveTemp(Candidate); Commands.Add(Id); return true;
		}
	};
	FDYKCityBuildingWorkRequest WorkTestUpgrade(const FDYKPlayerProfileSaveData& P,const TCHAR* Name)
	{
		FDYKCityBuildingWorkRequest R; R.CommandId=FGuid::NewGuid(); R.Kind=EDYKCityBuildingWorkKind::Upgrade;
		for(const auto& S:P.CityEconomy.Instances)
		{
			const auto& B=P.Buildings[S.BuildingIndex]; if(B.BuildingId.Name!=FName(Name)) continue;
			R.InstanceId=S.InstanceId; R.BuildingId=FDYKBuildingId(Name); R.ExpectedLevel=B.Level;
		}
		return R;
	}
	FDYKCityBuildingWorkRequest WorkTestBuild(const FDYKPlayerProfileSaveData& P,const TCHAR* Name)
	{
		FDYKCityBuildingWorkRequest R; R.CommandId=FGuid::NewGuid(); R.InstanceId=FGuid::NewGuid(); R.BuildingId=FDYKBuildingId(Name);
		FDYKCityBuildingWorkPlan Plan;
		for(int32 Y=0;Y<50;++Y) for(int32 X=0;X<50;++X)
		{
			R.Origin={X,Y};
			if(DYKCityBuildingRules::Prepare(P,R,DYKCityTerritory::DEV_PLACEHOLDER(),DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan)) return R;
		}
		return R;
	}
	int64 WorkTestBalance(const FWorkTestService& S) {return DYKCityEconomy::Balance(S.State,FDYKResourceId(TEXT("RES-WOOD")));}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKWorkQueueTest,"DYK.M206.BuildersQueueAndOfflineTransitions",WorkTestFlags)
bool FDYKWorkQueueTest::RunTest(const FString& Parameters)
{
	FWorkTestService S;
	TestEqual(TEXT("One starting builder"),DYKCityWork::Builders(S.State),1);
	const auto First=WorkTestUpgrade(S.State,TEXT("CITY-CASTLE"));
	TestTrue(TEXT("First job starts"),DYKCityWork::Enqueue(S,First,WorkTestTime));
	TestEqual(TEXT("Cost paid on start once"),WorkTestBalance(S),int64(90));
	const auto Second=WorkTestUpgrade(S.State,TEXT("CITY-SAWMILL"));
	TestFalse(TEXT("Level 2 sawmill cannot bypass castle condition"),DYKCityWork::Enqueue(S,Second,WorkTestTime));
	const auto House=WorkTestBuild(S.State,TEXT("CITY-HOUSE"));
	TestTrue(TEXT("Second job reserves site unpaid"),DYKCityWork::Enqueue(S,House,WorkTestTime+1));
	TestEqual(TEXT("Waiting job has no payment"),WorkTestBalance(S),int64(90));
	TestEqual(TEXT("Only first active"),S.State.CityEconomy.Work[1].StartedAtUnix,int64(0));
	TestEqual(TEXT("Unfinished house cannot produce"),DYKCityEconomy::View(S.State,House.InstanceId,WorkTestTime+3600).Stored,int64(0));
	TestTrue(TEXT("Retry enqueue idempotent"),DYKCityWork::Enqueue(S,House,WorkTestTime+1));
	TestEqual(TEXT("No extra site or work"),S.State.CityEconomy.Work.Num(),2);
	TestTrue(TEXT("Offline replay finishes and starts next at exact boundary"),DYKCityWork::Update(S,WorkTestTime+31));
	TestEqual(TEXT("Only house remains"),S.State.CityEconomy.Work.Num(),1);
	TestEqual(TEXT("Queued job starts at first completion, not login"),S.State.CityEconomy.Work[0].StartedAtUnix,WorkTestTime+30);
	TestEqual(TEXT("House cost now paid"),WorkTestBalance(S),int64(85));
	TestTrue(TEXT("Long offline completes remaining work"),DYKCityWork::Update(S,WorkTestTime+30*3600));
	TestTrue(TEXT("No outstanding work"),S.State.CityEconomy.Work.IsEmpty());
	TestEqual(TEXT("Production begins at real completion"),DYKCityEconomy::View(S.State,House.InstanceId,WorkTestTime+30*3600).Stored,int64(1));
	TestTrue(TEXT("Valid completed state"),DYKCityWork::Validate(S.State));
	const auto Before=S.State;
	TestTrue(TEXT("Repeated offline update"),DYKCityWork::Update(S,WorkTestTime+30*3600));
	TestTrue(TEXT("No repeated receipts or payment"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Before,&S.State,0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKWorkCancellationTest,"DYK.M206.CancellationRefundsAndFailures",WorkTestFlags)
bool FDYKWorkCancellationTest::RunTest(const FString& Parameters)
{
	FWorkTestService S;
	const auto First=WorkTestUpgrade(S.State,TEXT("CITY-CASTLE"));
	TestTrue(TEXT("Start upgrade"),DYKCityWork::Enqueue(S,First,WorkTestTime));
	const auto Pending=WorkTestBuild(S.State,TEXT("CITY-HOUSE"));
	TestTrue(TEXT("Queue house"),DYKCityWork::Enqueue(S,Pending,WorkTestTime));
	const auto CancelPending=FGuid::NewGuid();
	TestTrue(TEXT("Cancel unpaid site"),DYKCityWork::Cancel(S,CancelPending,Pending.CommandId,WorkTestTime+1));
	TestEqual(TEXT("No fabricated unpaid refund"),WorkTestBalance(S),int64(90));
	TestEqual(TEXT("Site removed, indices remain valid"),S.State.Buildings.Num(),5);
	TestTrue(TEXT("Validate after removing reservation"),DYKCityWork::Validate(S.State));
	for(auto& R:S.State.Resources) R.Amount=100;
	S.bFail=true;
	TestFalse(TEXT("Failed durable write rejects cancellation"),DYKCityWork::Cancel(S,FGuid::NewGuid(),First.CommandId,WorkTestTime+2));
	TestEqual(TEXT("Failure preserves active work"),S.State.CityEconomy.Work.Num(),1);
	S.bFail=false;
	const auto CancelPaid=FGuid::NewGuid();
	TestTrue(TEXT("Cancel paid upgrade"),DYKCityWork::Cancel(S,CancelPaid,First.CommandId,WorkTestTime+2));
	TestEqual(TEXT("Filled warehouse never exceeded"),WorkTestBalance(S),int64(100));
	TestEqual(TEXT("Full wood refund retained"),S.State.CityEconomy.PendingRefunds[0].Amount,int64(10));
	TestTrue(TEXT("Duplicate cancel acknowledged"),DYKCityWork::Cancel(S,CancelPaid,First.CommandId,WorkTestTime+2));
	TestFalse(TEXT("Different command cannot cancel twice"),DYKCityWork::Cancel(S,FGuid::NewGuid(),First.CommandId,WorkTestTime+2));
	TestTrue(TEXT("Spend room"),DYKCityEconomy::Spend(S,FGuid::NewGuid(),FDYKResourceId(TEXT("RES-WOOD")),20));
	TestTrue(TEXT("Claim retained refund"),DYKCityWork::Update(S,WorkTestTime+3));
	TestEqual(TEXT("Full exact refund once"),WorkTestBalance(S),int64(90));
	TestTrue(TEXT("Refund no longer pending for wood"),!S.State.CityEconomy.PendingRefunds.ContainsByPredicate([](const auto& R){return R.Resource==FName(TEXT("RES-WOOD"));}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKWorkPersistenceTest,"DYK.M206.SaveBoundaryAndEditor",WorkTestFlags)
bool FDYKWorkPersistenceTest::RunTest(const FString& Parameters)
{
	FWorkTestService S;
	const auto R=WorkTestUpgrade(S.State,TEXT("CITY-CASTLE"));
	TestTrue(TEXT("Start"),DYKCityWork::Enqueue(S,R,WorkTestTime));
	auto* Save=NewObject<UDYKProfileSaveGame>(); Save->Profile=S.State; Save->AppliedCommandIds=S.Commands;
	TArray<uint8> Bytes; TestTrue(TEXT("Serialize active work"),UGameplayStatics::SaveGameToMemory(Save,Bytes));
	auto* Loaded=Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if(!TestNotNull(TEXT("Load active work"),Loaded)) return false;
	S.State=Loaded->Profile; S.Commands=Loaded->AppliedCommandIds;
	FDYKCityLayoutEditor Editor;
	TestTrue(TEXT("Editor can start during upgrade"),Editor.Begin(S.State));
	TestTrue(TEXT("Offline finish at exact boundary"),DYKCityWork::Update(S,WorkTestTime+30));
	TestFalse(TEXT("Cancel at boundary cannot refund completed work"),DYKCityWork::Cancel(S,FGuid::NewGuid(),R.CommandId,WorkTestTime+30));
	TestTrue(TEXT("Editor commits without resetting finished level or cost"),Editor.Commit(S));
	TestEqual(TEXT("Castle reached level 2"),S.State.Buildings[0].Level,2);
	TestEqual(TEXT("No double payment"),WorkTestBalance(S),int64(90));
	TestTrue(TEXT("Backwards update cannot regress"),DYKCityWork::Update(S,WorkTestTime-1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKWorkTwoBuildersTest,"DYK.M206.TwoBuildersReservationsAndInvalidState",WorkTestFlags)
bool FDYKWorkTwoBuildersTest::RunTest(const FString& Parameters)
{
	FWorkTestService S;
	S.State.UnlockedContent.Add({EDYKContentKind::Campaign,TEXT("DEV-CAMPAIGN-COMPLETED")});
	TestEqual(TEXT("Campaign unlocks only second builder"),DYKCityWork::Builders(S.State),2);
	const auto First=WorkTestUpgrade(S.State,TEXT("CITY-CASTLE"));
	TestTrue(TEXT("First"),DYKCityWork::Enqueue(S,First,WorkTestTime));
	const auto Second=WorkTestBuild(S.State,TEXT("CITY-HOUSE"));
	TestTrue(TEXT("Second"),DYKCityWork::Enqueue(S,Second,WorkTestTime));
	TestTrue(TEXT("Two concurrent jobs"),S.State.CityEconomy.Work[1].StartedAtUnix>0);
	const auto Third=WorkTestBuild(S.State,TEXT("CITY-HOUSE"));
	TestTrue(TEXT("Third waits"),DYKCityWork::Enqueue(S,Third,WorkTestTime));
	TestEqual(TEXT("Unpaid third"),S.State.CityEconomy.Work[2].StartedAtUnix,int64(0));
	const auto Fourth=WorkTestBuild(S.State,TEXT("CITY-HOUSE"));
	TestFalse(TEXT("Reserved sites count towards house limit"),DYKCityWork::Enqueue(S,Fourth,WorkTestTime));
	TestTrue(TEXT("Offline completes three jobs"),DYKCityWork::Update(S,WorkTestTime+30));
	TestTrue(TEXT("Queue empty"),S.State.CityEconomy.Work.IsEmpty());
	TestTrue(TEXT("Valid layout"),DYKCityTerritory::Validate(S.State,DYKCityTerritory::DEV_PLACEHOLDER()));
	auto Invalid=S.State; Invalid.Buildings[0].bUnderConstruction=true;
	TestFalse(TEXT("Orphaned construction rejected"),DYKCityWork::Validate(Invalid));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKWorkProductionAndWaitTest,"DYK.M206.ProductionBoundaryAndResourceWait",WorkTestFlags)
bool FDYKWorkProductionAndWaitTest::RunTest(const FString& Parameters)
{
	FWorkTestService S;
	S.State.Buildings[0].Level=2;
	const auto Saw=WorkTestUpgrade(S.State,TEXT("CITY-SAWMILL"));
	TestTrue(TEXT("Start sawmill upgrade"),DYKCityWork::Enqueue(S,Saw,WorkTestTime));
	TestEqual(TEXT("Before completion old production capacity"),DYKCityEconomy::View(S.State,Saw.InstanceId,WorkTestTime+29).Capacity,int64(1440));
	TestTrue(TEXT("Complete at 30 seconds"),DYKCityWork::Update(S,WorkTestTime+30));
	TestEqual(TEXT("Thirty seconds of old work preserved into new cycle"),DYKCityEconomy::View(S.State,Saw.InstanceId,WorkTestTime+60).Stored,int64(4));
	TestTrue(TEXT("Free wood space"),DYKCityEconomy::Spend(S,FGuid::NewGuid(),FDYKResourceId(TEXT("RES-WOOD")),20));
	TestTrue(TEXT("Collect settles finished upgrades first"),DYKCityEconomy::Collect(S,FGuid::NewGuid(),Saw.InstanceId,WorkTestTime+60));
	TestTrue(TEXT("Valid after collection"),DYKCityWork::Validate(S.State));
	for(auto& R:S.State.Resources) R.Amount=0;
	const auto House=WorkTestBuild(S.State,TEXT("CITY-HOUSE"));
	TestTrue(TEXT("Queue without money"),DYKCityWork::Enqueue(S,House,WorkTestTime+60));
	TestEqual(TEXT("Unpaid despite idle builder"),S.State.CityEconomy.Work[0].StartedAtUnix,int64(0));
	TestTrue(TEXT("Long offline cannot build without payment"),DYKCityWork::Update(S,WorkTestTime+3600));
	for(auto& R:S.State.Resources) R.Amount=20;
	TestTrue(TEXT("Funds arrive now"),DYKCityWork::Update(S,WorkTestTime+7200));
	TestEqual(TEXT("Resource wait cannot backdate newly funded work"),S.State.CityEconomy.Work[0].StartedAtUnix,WorkTestTime+7200);
	return true;
}
#endif
