#include "DYKCityHUDTestProfile.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "../Meta/DYKCityLayoutEditor.h"

struct FDYKCityHUDTestAccess
{
	static void Open(UDYKSaveSubsystem& S,const FString& Prefix) {S.SlotPrefix=Prefix; S.LoadOrCreateProfile();}
};
constexpr auto CityOrdersTestFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKOrderMigrationTest,"DYK.CityHUD.Orders.MigrationAndNoUnrequestedIncome",CityOrdersTestFlags)
bool FDYKOrderMigrationTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P; TestTrue(TEXT("Legacy seed"),DYKCityEconomy::Initialize(P,DYKCityHUDTest::Time));
	const auto Id=DYKCityHUDTest::Instance(P,TEXT("CITY-SAWMILL"));
	const int64 Before=DYKCityEconomy::View(P,Id,DYKCityHUDTest::Time+3600).Stored;
	TestTrue(TEXT("Settle old model once"),DYKCityOrders::Migrate(P,DYKCityHUDTest::Time+3600));
	TestEqual(TEXT("Existing goods conserved"),DYKCityEconomy::View(P,Id,DYKCityHUDTest::Time+3600).Stored,Before);
	TestTrue(TEXT("Advance thirty hours with no orders"),DYKCityResearch::Advance(P,DYKCityHUDTest::Time+30*3600));
	TestEqual(TEXT("No passive producer output"),DYKCityEconomy::View(P,Id,DYKCityHUDTest::Time+30*3600).Stored,Before);
	TestTrue(TEXT("Migration idempotent"),DYKCityOrders::Migrate(P,DYKCityHUDTest::Time+30*3600));
	TestEqual(TEXT("No duplicated historical accrual"),DYKCityEconomy::View(P,Id,MAX_int64).Stored,Before);
	const auto House=DYKCityHUDTest::Instance(P,TEXT("CITY-HOUSE"));
	TestEqual(TEXT("House remains one gold cycle"),DYKCityEconomy::View(P,House,DYKCityHUDTest::Time+30*3600).Stored,int64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKOrdersQueueTest,"DYK.CityHUD.Orders.PerInstanceQueueAndOfflineBoundaries",CityOrdersTestFlags)
bool FDYKOrdersQueueTest::RunTest(const FString& Parameters)
{
	DYKCityHUDTest::Service S; const auto T=DYKCityHUDTest::Time;
	const auto Saw=DYKCityHUDTest::Instance(S.State,TEXT("CITY-SAWMILL"));
	const auto Quarry=DYKCityHUDTest::Instance(S.State,TEXT("CITY-QUARRY"));
	const auto First=FGuid::NewGuid();
	TestTrue(TEXT("Start first order"),DYKCityOrders::Enqueue(S,First,Saw,0,T));
	TestTrue(TEXT("Second queued"),DYKCityOrders::Enqueue(S,FGuid::NewGuid(),Saw,1,T));
	TestTrue(TEXT("Third queued"),DYKCityOrders::Enqueue(S,FGuid::NewGuid(),Saw,3,T));
	TestFalse(TEXT("Fourth rejected"),DYKCityOrders::Enqueue(S,FGuid::NewGuid(),Saw,0,T));
	TestTrue(TEXT("Repeated command acknowledged"),DYKCityOrders::Enqueue(S,First,Saw,0,T));
	TestTrue(TEXT("Other building has independent slots"),DYKCityOrders::Enqueue(S,FGuid::NewGuid(),Quarry,0,T));
	TestTrue(TEXT("Offline first two complete"),DYKCityWork::Update(S,T+4200));
	const auto* State=S.State.CityEconomy.Instances.FindByPredicate([Saw](const auto& R){return R.InstanceId==Saw;});
	if(!TestEqual(TEXT("One remaining order"),State->Orders.Num(),1)) return false;
	TestEqual(TEXT("Actual offline start of third"),State->Orders[0].StartedAtUnix,T+4200);
	TestEqual(TEXT("First two outputs"),State->Stored,int64(85));
	TestTrue(TEXT("Long offline completes all"),DYKCityWork::Update(S,T+30*3600));
	State=S.State.CityEconomy.Instances.FindByPredicate([Saw](const auto& R){return R.InstanceId==Saw;});
	TestEqual(TEXT("Only explicitly ordered output"),State->Stored,int64(735));
	TestTrue(TEXT("No automatically recreated work"),State->Orders.IsEmpty());
	TestEqual(TEXT("No automatic collection to wallet"),DYKCityEconomy::Balance(S.State,FDYKResourceId(TEXT("RES-WOOD"))),int64(70));
	TestTrue(TEXT("Negative clock cannot undo progress"),DYKCityWork::Update(S,T-1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKOrdersCapacityTest,"DYK.CityHUD.Orders.FullStorePartialCollectAndDelayedRelease",CityOrdersTestFlags)
bool FDYKOrdersCapacityTest::RunTest(const FString& Parameters)
{
	DYKCityHUDTest::Service S; const auto T=DYKCityHUDTest::Time;
	const auto Id=DYKCityHUDTest::Instance(S.State,TEXT("CITY-SAWMILL"));
	auto* State=S.State.CityEconomy.Instances.FindByPredicate([Id](const auto& R){return R.InstanceId==Id;}); State->Stored=1435;
	TestTrue(TEXT("Start with five storage places"),DYKCityOrders::Enqueue(S,FGuid::NewGuid(),Id,0,T));
	TestTrue(TEXT("Second waits"),DYKCityOrders::Enqueue(S,FGuid::NewGuid(),Id,0,T));
	TestTrue(TEXT("First completed at store limit"),DYKCityWork::Update(S,T+600));
	State=S.State.CityEconomy.Instances.FindByPredicate([Id](const auto& R){return R.InstanceId==Id;});
	TestEqual(TEXT("Stored exactly capacity"),State->Stored,int64(1440));
	TestEqual(TEXT("Unplaced part retained inside order"),State->Orders[0].RemainingOutput,int64(10));
	TestEqual(TEXT("Next remains unstarted"),State->Orders[1].StartedAtUnix,int64(0));
	const auto Collect=FGuid::NewGuid();
	TestTrue(TEXT("Collect only thirty free warehouse places"),DYKCityEconomy::Collect(S,Collect,Id,T+601));
	TestEqual(TEXT("Warehouse exact bound"),DYKCityEconomy::Balance(S.State,FDYKResourceId(TEXT("RES-WOOD"))),int64(100));
	TestTrue(TEXT("Transfer retained output and release next"),DYKCityWork::Update(S,T+601));
	State=S.State.CityEconomy.Instances.FindByPredicate([Id](const auto& R){return R.InstanceId==Id;});
	TestEqual(TEXT("No lost output"),State->Stored,int64(1420));
	TestEqual(TEXT("Release is not backdated to blocked completion"),State->Orders[0].StartedAtUnix,T+601);
	TestTrue(TEXT("Collection replay cannot pay twice"),DYKCityEconomy::Collect(S,Collect,Id,T+900));
	TestEqual(TEXT("Wallet unchanged on replay"),DYKCityEconomy::Balance(S.State,FDYKResourceId(TEXT("RES-WOOD"))),int64(100));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKOrdersSaveTest,"DYK.CityHUD.Orders.RealProfileFilesAndReplay",CityOrdersTestFlags)
bool FDYKOrdersSaveTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> Owner(NewObject<UGameInstance>());
	const FString Prefix=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("CityHUD/file-tests"),FGuid::NewGuid().ToString(EGuidFormats::Digits),TEXT("profile")));
	auto* Save=NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKCityHUDTestAccess::Open(*Save,Prefix);
	TestTrue(TEXT("Seed isolated persistent state"),Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& P){return DYKCityHUDTest::Seed(P);}));
	const auto Id=DYKCityHUDTest::Instance(*Save->GetProfileSnapshot(),TEXT("CITY-SAWMILL"));
	const auto First=FGuid::NewGuid();
	TestTrue(TEXT("Persist active order"),DYKCityOrders::Enqueue(*Save,First,Id,0,DYKCityHUDTest::Time));
	TestTrue(TEXT("Persist pending order"),DYKCityOrders::Enqueue(*Save,FGuid::NewGuid(),Id,1,DYKCityHUDTest::Time));
	const auto Expected=*Save->GetProfileSnapshot();
	auto* Reload=NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKCityHUDTestAccess::Open(*Reload,Prefix);
	if(!TestNotNull(TEXT("Read actual profile files"),Reload->GetProfileSnapshot())) return false;
	TestTrue(TEXT("Exact queue/timer/remainder restoration"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Expected,Reload->GetProfileSnapshot(),0));
	TestTrue(TEXT("Offline completion after reload"),DYKCityWork::Update(*Reload,DYKCityHUDTest::Time+5000));
	TestTrue(TEXT("Old order command cannot append again"),DYKCityOrders::Enqueue(*Reload,First,Id,0,DYKCityHUDTest::Time+5000));
	const auto* State=Reload->GetProfileSnapshot()->CityEconomy.Instances.FindByPredicate([Id](const auto& R){return R.InstanceId==Id;});
	TestEqual(TEXT("Output counted precisely once"),State->Stored,int64(85));
	TestTrue(TEXT("Queue stays empty"),State->Orders.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKOrdersFailureTest,"DYK.CityHUD.Orders.InvalidStateCancelAndFailedWrite",CityOrdersTestFlags)
bool FDYKOrdersFailureTest::RunTest(const FString& Parameters)
{
	DYKCityHUDTest::Service S; const auto T=DYKCityHUDTest::Time; const auto Id=DYKCityHUDTest::Instance(S.State,TEXT("CITY-SAWMILL"));
	const auto Active=FGuid::NewGuid(),Pending=FGuid::NewGuid();
	TestFalse(TEXT("Unknown recipe"),DYKCityOrders::Enqueue(S,FGuid::NewGuid(),Id,9,T));
	S.Fail=true; TestFalse(TEXT("Write failure atomic"),DYKCityOrders::Enqueue(S,Active,Id,0,T));
	TestTrue(TEXT("Failed write leaves no orders"),S.State.CityEconomy.Instances[1].Orders.IsEmpty());
	S.Fail=false; TestTrue(TEXT("Retry succeeds once"),DYKCityOrders::Enqueue(S,Active,Id,0,T));
	TestTrue(TEXT("Pending"),DYKCityOrders::Enqueue(S,Pending,Id,1,T));
	TestFalse(TEXT("Active order cannot be canceled as waiting"),DYKCityOrders::CancelWaiting(S,FGuid::NewGuid(),Id,Active,T+1));
	TestTrue(TEXT("Cancel only pending work"),DYKCityOrders::CancelWaiting(S,FGuid::NewGuid(),Id,Pending,T+1));
	TestFalse(TEXT("No second cancellation"),DYKCityOrders::CancelWaiting(S,FGuid::NewGuid(),Id,Pending,T+1));
	auto Bad=S.State; Bad.CityEconomy.Instances[1].Orders[0].Recipe=-1;
	TestFalse(TEXT("Corrupt recipe rejected"),DYKCityOrders::Advance(Bad,T+3600));
	Bad=S.State; Bad.CityEconomy.Instances[1].Orders[0].RemainingOutput=MAX_int64;
	TestFalse(TEXT("Corrupt output rejected"),DYKCityOrders::Validate(Bad));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKOrdersUpgradeTest,"DYK.CityHUD.Orders.UpgradeAndLayoutPreserveActiveOrder",CityOrdersTestFlags)
bool FDYKOrdersUpgradeTest::RunTest(const FString& Parameters)
{
	DYKCityHUDTest::Service S; const auto T=DYKCityHUDTest::Time;
	const auto Id=DYKCityHUDTest::Instance(S.State,TEXT("CITY-SAWMILL"));
	TestTrue(TEXT("Start ten minute order"),DYKCityOrders::Enqueue(S,FGuid::NewGuid(),Id,0,T));
	FDYKCityBuildingWorkRequest Upgrade; Upgrade.CommandId=FGuid::NewGuid(); Upgrade.InstanceId=Id;
	Upgrade.BuildingId=FDYKBuildingId(TEXT("CITY-SAWMILL")); Upgrade.Kind=EDYKCityBuildingWorkKind::Upgrade; Upgrade.ExpectedLevel=1;
	TestTrue(TEXT("Start independent building upgrade"),DYKCityWork::Enqueue(S,Upgrade,T+10));
	FDYKCityLayoutEditor Editor; TestTrue(TEXT("Editor starts during active order and upgrade"),Editor.Begin(S.State));
	const auto Original=Editor.Entries().FindByPredicate([Id](const auto& E){return E.InstanceId==Id;})->Origin;
	TestTrue(TEXT("Temporary storage"),Editor.Store(Id));
	TestTrue(TEXT("Offline completes upgrade, not yet order"),DYKCityWork::Update(S,T+50));
	const auto* Producer=S.State.CityEconomy.Instances.FindByPredicate([Id](const auto& R){return R.InstanceId==Id;});
	TestEqual(TEXT("Started order retains exact completion time"),Producer->Orders[0].CompletesAtUnix,T+600);
	TestTrue(TEXT("Restore editor placement"),Editor.Place(Id,Original,0));
	TestTrue(TEXT("Editor commit preserves latest economy"),Editor.Commit(S));
	TestTrue(TEXT("Finish original order"),DYKCityWork::Update(S,T+600));
	Producer=S.State.CityEconomy.Instances.FindByPredicate([Id](const auto& R){return R.InstanceId==Id;});
	TestEqual(TEXT("Output conserved"),Producer->Stored,int64(15));
	TestEqual(TEXT("Upgrade retained"),S.State.Buildings[Producer->BuildingIndex].Level,2);
	TestFalse(TEXT("Overflow timestamp rejected atomically"),DYKCityOrders::Enqueue(S,FGuid::NewGuid(),Id,0,MAX_int64));
	TestEqual(TEXT("Failed timestamp cannot create work"),S.State.CityEconomy.Instances.FindByPredicate([Id](const auto& R){return R.InstanceId==Id;})->Orders.Num(),0);
	return true;
}
#endif
