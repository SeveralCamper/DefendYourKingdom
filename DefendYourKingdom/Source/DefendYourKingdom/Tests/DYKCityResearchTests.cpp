#include "../Meta/DYKCityResearch.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Saving/DYKProfileService.h"
#include "../Core/Content/DYKUnitCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "../Battle/DYKDefenderActor.h"
#include "../Battle/DYKPolygonGameMode.h"
#include "../Meta/DYKMissionRewards.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

struct FDYKCityResearchTestAccess
{
	static void Open(UDYKSaveSubsystem& Service,const FString& Prefix)
	{Service.SlotPrefix=Prefix; Service.LoadOrCreateProfile();}
};

namespace
{
	constexpr auto ResearchTestFlags=EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int64 ResearchTestTime=1794700000;
	struct FResearchTestService : IDYKProfileService
	{
		FDYKPlayerProfileSaveData State;
		TArray<FGuid> Commands;
		bool bFail=false;
		FResearchTestService()
		{
			DYKCityEconomy::Initialize(State,ResearchTestTime);
			DYKCityTerritory::Initialize(State,DYKCityTerritory::DEV_PLACEHOLDER());
			State.Buildings[0].Level=4;
			FDYKBuildingSaveData Warehouse; Warehouse.BuildingId=FDYKBuildingId(TEXT("CITY-WAREHOUSE")).ToContentId();
			Warehouse.Level=4; Warehouse.CellOrigin={27,25};
			FDYKCityProductionSaveData S; S.InstanceId=FGuid::NewGuid(); S.BuildingIndex=State.Buildings.Add(Warehouse);
			S.UpdatedAtUnix=S.CollectionStartedAtUnix=ResearchTestTime; State.CityEconomy.Instances.Add(S);
			for(const auto BuildingName:{TEXT("CITY-BARRACKS"),TEXT("CITY-LIBRARY")})
			{
				FDYKBuildingSaveData Owned; Owned.BuildingId=FDYKBuildingId(BuildingName).ToContentId();
				bool Found=false;
				for(int32 Y=0;Y<50 && !Found;++Y) for(int32 X=0;X<50 && !Found;++X)
				{
					Owned.CellOrigin={X,Y}; auto Layout=State.Buildings; Layout.Add(Owned);
					Found=DYKCityTerritory::ValidateLayout(State,Layout,DYKCityTerritory::DEV_PLACEHOLDER());
				}
				FDYKCityProductionSaveData BuildingState; BuildingState.InstanceId=FGuid::NewGuid(); BuildingState.BuildingIndex=State.Buildings.Add(Owned);
				BuildingState.UpdatedAtUnix=BuildingState.CollectionStartedAtUnix=ResearchTestTime; State.CityEconomy.Instances.Add(BuildingState);
			}
			for(auto& R:State.Resources) R.Amount=FMath::Min<int64>(2400,DYKCityEconomy::Capacity(State,FDYKResourceId(R.ResourceId.Name)));
			State.Resources.Add({FDYKResourceId(TEXT("RES-MEDALS")).ToContentId(),100});
			DYKCityResearch::Advance(State,ResearchTestTime);
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
	int64 ResearchTestBalance(const FResearchTestService& S,const TCHAR* Resource)
	{return DYKCityEconomy::Balance(S.State,FDYKResourceId(Resource));}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKResearchRanksTest,"DYK.M207.RankOrderAndIndependentSlot",ResearchTestFlags)
bool FDYKResearchRanksTest::RunTest(const FString& Parameters)
{
	FResearchTestService S;
	TestTrue(TEXT("Fixture and authored graph valid"),DYKCityResearch::Validate(S.State));
	TestTrue(TEXT("Fixture layout valid"),DYKCityTerritory::Validate(S.State,DYKCityTerritory::DEV_PLACEHOLDER()));
	TestFalse(TEXT("T3 cannot skip T2"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),TEXT("RESEARCH-INFANTRY-T3"),ResearchTestTime));
	const FName SecondRank(TEXT("RESEARCH-INFANTRY-T2"));
	auto& Food=S.State.Resources.FindByPredicate([](const auto& R){return R.ResourceId.Name==TEXT("RES-FOOD");})->Amount;
	Food=499;
	TestFalse(TEXT("HUD eligibility rejects research one resource short"),DYKCityResearch::CanEnqueue(S.State,SecondRank));
	TestFalse(TEXT("Direct research command rejects the same insufficient funds"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),SecondRank,ResearchTestTime));
	TestTrue(TEXT("Rejection leaves the research queue empty"),S.State.CityEconomy.ResearchQueue.IsEmpty());
	TestEqual(TEXT("Rejection leaves current balance untouched"),ResearchTestBalance(S,TEXT("RES-FOOD")),int64(499));
	Food=500;
	TestTrue(TEXT("Exact amount is sufficient"),DYKCityResearch::CanEnqueue(S.State,SecondRank));
	Food=2400;
	FDYKCityBuildingWorkRequest Build; Build.CommandId=FGuid::NewGuid(); Build.InstanceId=S.State.CityEconomy.Instances[0].InstanceId;
	Build.Kind=EDYKCityBuildingWorkKind::Upgrade; Build.BuildingId=FDYKBuildingId(TEXT("CITY-CASTLE")); Build.ExpectedLevel=4;
	TestTrue(TEXT("Occupy sole builder"),DYKCityWork::Enqueue(S,Build,ResearchTestTime));
	const auto First=FGuid::NewGuid(),Second=FGuid::NewGuid();
	TestTrue(TEXT("Research starts independently of builder"),DYKCityResearch::Enqueue(S,First,TEXT("RESEARCH-INFANTRY-T2"),ResearchTestTime));
	TestEqual(TEXT("Author food price paid"),ResearchTestBalance(S,TEXT("RES-FOOD")),int64(1900));
	TestTrue(TEXT("Second independent research queues"),DYKCityResearch::Enqueue(S,Second,TEXT("RESEARCH-RANGED-T2"),ResearchTestTime));
	TestEqual(TEXT("Second remains unpaid"),ResearchTestBalance(S,TEXT("RES-FOOD")),int64(1900));
	TestEqual(TEXT("Exactly one research slot active"),S.State.CityEconomy.ResearchQueue[1].StartedAtUnix,int64(0));
	TestTrue(TEXT("Offline replays two researches"),DYKCityWork::Update(S,ResearchTestTime+40));
	TestTrue(TEXT("Both T2 unlocked"),DYKCityResearch::IsUnlocked(S.State,FDYKUnitTypeId(TEXT("DYK-GUARD"))) && DYKCityResearch::IsUnlocked(S.State,FDYKUnitTypeId(TEXT("DYK-VETERAN-ARCHER"))));
	TestEqual(TEXT("Builder still working on 75s task"),S.State.CityEconomy.Work.Num(),1);
	TestTrue(TEXT("Now T3 allowed"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),TEXT("RESEARCH-INFANTRY-T3"),ResearchTestTime+40));
	TestEqual(TEXT("Author medals price paid"),ResearchTestBalance(S,TEXT("RES-MEDALS")),int64(90));
	TestTrue(TEXT("T3 completes"),DYKCityWork::Update(S,ResearchTestTime+60));
	TestTrue(TEXT("T1 and T2 remain available after T3"),DYKCityResearch::IsUnlocked(S.State,DYKUnitIds::Peasant()) && DYKCityResearch::IsUnlocked(S.State,FDYKUnitTypeId(TEXT("DYK-GUARD"))));
	TestTrue(TEXT("T3 unlocked"),DYKCityResearch::IsUnlocked(S.State,FDYKUnitTypeId(TEXT("DYK-ROYAL-GUARD"))));
	TestFalse(TEXT("Completed research cannot be bought twice"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),TEXT("RESEARCH-INFANTRY-T3"),ResearchTestTime+60));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKResearchCancelTest,"DYK.M207.CancellationAndExactCompletionBoundary",ResearchTestFlags)
bool FDYKResearchCancelTest::RunTest(const FString& Parameters)
{
	FResearchTestService S;
	const int64 FoodBefore=ResearchTestBalance(S,TEXT("RES-FOOD"));
	const FGuid Start=FGuid::NewGuid(),Cancel=FGuid::NewGuid();
	TestTrue(TEXT("Paid line HP research starts"),DYKCityResearch::Enqueue(S,Start,TEXT("RESEARCH-INFANTRY-HP"),ResearchTestTime));
	TestEqual(TEXT("Exact authored price"),ResearchTestBalance(S,TEXT("RES-FOOD")),FoodBefore-800);
	const FGuid Pending=FGuid::NewGuid();
	TestTrue(TEXT("Queue an unpaid second research"),DYKCityResearch::Enqueue(S,Pending,TEXT("RESEARCH-RANGED-HP"),ResearchTestTime));
	TestTrue(TEXT("Cancel unpaid research"),DYKCityResearch::Cancel(S,FGuid::NewGuid(),Pending,ResearchTestTime+1));
	TestEqual(TEXT("Unpaid cancellation fabricates no refund"),ResearchTestBalance(S,TEXT("RES-FOOD")),FoodBefore-800);
	S.bFail=true;
	TestFalse(TEXT("Failed save cannot cancel or refund"),DYKCityResearch::Cancel(S,Cancel,Start,ResearchTestTime+19));
	TestEqual(TEXT("Failure leaves paid state"),ResearchTestBalance(S,TEXT("RES-FOOD")),FoodBefore-800);
	S.bFail=false;
	TestTrue(TEXT("Cancellation before boundary"),DYKCityResearch::Cancel(S,Cancel,Start,ResearchTestTime+19));
	TestEqual(TEXT("Full food refund"),ResearchTestBalance(S,TEXT("RES-FOOD")),FoodBefore);
	TestEqual(TEXT("Full medals refund"),ResearchTestBalance(S,TEXT("RES-MEDALS")),int64(100));
	TestTrue(TEXT("Same cancel idempotent"),DYKCityResearch::Cancel(S,Cancel,Start,ResearchTestTime+19));
	TestFalse(TEXT("Different command cannot refund twice"),DYKCityResearch::Cancel(S,FGuid::NewGuid(),Start,ResearchTestTime+19));
	const auto Again=FGuid::NewGuid();
	TestTrue(TEXT("Can research canceled node again"),DYKCityResearch::Enqueue(S,Again,TEXT("RESEARCH-INFANTRY-HP"),ResearchTestTime+20));
	TestFalse(TEXT("At exact completion cancel cannot refund"),DYKCityResearch::Cancel(S,FGuid::NewGuid(),Again,ResearchTestTime+40));
	TestTrue(TEXT("Normal update persists completion"),DYKCityWork::Update(S,ResearchTestTime+40));
	TestTrue(TEXT("Completed permanently"),S.State.CityEconomy.CompletedResearch.Contains(TEXT("RESEARCH-INFANTRY-HP")));
	TestEqual(TEXT("Only one final price paid"),ResearchTestBalance(S,TEXT("RES-FOOD")),FoodBefore-800);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKResearchScopeTest,"DYK.M207.TargetScopesAndSequentialPersonalLevels",ResearchTestFlags)
bool FDYKResearchScopeTest::RunTest(const FString& Parameters)
{
	FResearchTestService S;
	TestFalse(TEXT("Personal research requires unlocked tier"),DYKCityResearch::CanEnqueue(S.State,TEXT("RESEARCH-INFANTRY-T3-HP-1")));
	TestTrue(TEXT("Line damage starts"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),TEXT("RESEARCH-INFANTRY-DAMAGE"),ResearchTestTime));
	TestTrue(TEXT("Line damage completes"),DYKCityWork::Update(S,ResearchTestTime+20));
	const auto* Peasant=DYKUnitCatalog::Find(DYKUnitIds::Peasant());
	const auto* Archer=DYKUnitCatalog::Find(DYKUnitIds::Archer());
	const auto* Enemy=DYKUnitCatalog::Find(DYKUnitIds::Goblin());
	TestEqual(TEXT("Infantry receives authored +2 damage"),DYKCityResearch::Bonus(S.State,*Peasant).Damage,2);
	TestEqual(TEXT("Ranged unaffected"),DYKCityResearch::Bonus(S.State,*Archer).Damage,0);
	TestEqual(TEXT("Enemy unaffected"),DYKCityResearch::Bonus(S.State,*Enemy).Damage,0);
	TestEqual(TEXT("Global catalog remains immutable"),Peasant->Base.Attack.DamagePerHit.Value,4);
	// Completed ownership fixture isolates the personal-level dependency contract.
	S.State.CityEconomy.CompletedResearch.Add(TEXT("RESEARCH-INFANTRY-T2"));
	S.State.CityEconomy.CompletedResearch.Add(TEXT("RESEARCH-INFANTRY-T3"));
	TestFalse(TEXT("Personal level two cannot skip one"),DYKCityResearch::CanEnqueue(S.State,TEXT("RESEARCH-INFANTRY-T3-HP-2")));
	S.State.CityEconomy.CompletedResearch.Add(TEXT("RESEARCH-INFANTRY-T3-HP-1"));
	TestTrue(TEXT("Personal level two follows one"),DYKCityResearch::CanEnqueue(S.State,TEXT("RESEARCH-INFANTRY-T3-HP-2")));
	FDYKUnitDefinition Royal=*Peasant; Royal.Id=FDYKUnitTypeId(TEXT("DYK-ROYAL-GUARD")); Royal.Tier=3;
	TestEqual(TEXT("Personal HP applies to exact target type"),DYKCityResearch::Bonus(S.State,Royal,EDYKResearchScope::Type).HP,10);
	TestEqual(TEXT("Line and personal effects remain separate pending final composition"),DYKCityResearch::Bonus(S.State,Royal).HP,0);
	TestEqual(TEXT("Other ranks do not receive personal HP"),DYKCityResearch::Bonus(S.State,*Peasant).HP,0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKResearchConfigTest,"DYK.M207.CompositeConditionsAndInvalidGraphs",ResearchTestFlags)
bool FDYKResearchConfigTest::RunTest(const FString& Parameters)
{
	FResearchTestService S;
	auto C=DYKCityResearch::DEV_PLACEHOLDER();
	auto* Node=C.Definitions.FindByPredicate([](const auto& D){return D.Id==FName(TEXT("RESEARCH-INFANTRY-T2"));});
	Node->Conditions={TEXT("Castle5"),TEXT("Event")};
	C.Conditions.Add(TEXT("Castle5"),[](const auto& P){return P.Buildings[0].Level>=5;});
	C.Conditions.Add(TEXT("Event"),[](const auto& P){return P.UnlockedContent.Contains(FDYKContentId{EDYKContentKind::Campaign,TEXT("TEST-RESEARCH-EVENT")});});
	TestTrue(TEXT("Composite graph valid"),DYKCityResearch::ValidateConfig(C));
	TestFalse(TEXT("Both conditions required"),DYKCityResearch::CanEnqueue(S.State,Node->Id,C));
	S.State.Buildings[0].Level=5;
	TestFalse(TEXT("One condition insufficient"),DYKCityResearch::CanEnqueue(S.State,Node->Id,C));
	S.State.UnlockedContent.Add({EDYKContentKind::Campaign,TEXT("TEST-RESEARCH-EVENT")});
	TestTrue(TEXT("Both met"),DYKCityResearch::CanEnqueue(S.State,Node->Id,C));
	Node->Prerequisites.Add(Node->Id);
	TestFalse(TEXT("Cycle rejected"),DYKCityResearch::ValidateConfig(C));
	Node->Prerequisites.Pop(); Node->Prerequisites.Add(TEXT("MISSING-NODE"));
	TestFalse(TEXT("Dangling dependency rejected"),DYKCityResearch::ValidateConfig(C));
	Node->Prerequisites.Pop(); Node->DurationSeconds=0;
	TestFalse(TEXT("Instant research forbidden"),DYKCityResearch::ValidateConfig(C));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKResearchPersistenceTest,"DYK.M207.OfflineSaveRoundtripAndChronologicalResources",ResearchTestFlags)
bool FDYKResearchPersistenceTest::RunTest(const FString& Parameters)
{
	FResearchTestService S;
	const auto First=FGuid::NewGuid(),Second=FGuid::NewGuid();
	TestTrue(TEXT("Start first research"),DYKCityResearch::Enqueue(S,First,TEXT("RESEARCH-INFANTRY-T2"),ResearchTestTime));
	TestTrue(TEXT("Queue second research"),DYKCityResearch::Enqueue(S,Second,TEXT("RESEARCH-RANGED-T2"),ResearchTestTime));
	auto* Save=NewObject<UDYKProfileSaveGame>(); Save->Profile=S.State; Save->AppliedCommandIds=S.Commands;
	TArray<uint8> Bytes; if(!TestTrue(TEXT("Serialize pending research"),UGameplayStatics::SaveGameToMemory(Save,Bytes))) return false;
	auto* Loaded=Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if(!TestNotNull(TEXT("Load"),Loaded)) return false;
	S.State=Loaded->Profile; S.Commands=Loaded->AppliedCommandIds;
	TestTrue(TEXT("After restart first finishes at 20, second starts at 20"),DYKCityWork::Update(S,ResearchTestTime+25));
	TestEqual(TEXT("Exactly one pending research"),S.State.CityEconomy.ResearchQueue.Num(),1);
	TestEqual(TEXT("Second started offline at correct boundary"),S.State.CityEconomy.ResearchQueue[0].StartedAtUnix,ResearchTestTime+20);
	TestTrue(TEXT("Replay enqueue does not pay twice"),DYKCityResearch::Enqueue(S,Second,TEXT("RESEARCH-RANGED-T2"),ResearchTestTime+25));
	TestEqual(TEXT("Exactly two research food payments"),ResearchTestBalance(S,TEXT("RES-FOOD")),int64(1400));
	TestTrue(TEXT("Backward clock cannot regress work"),DYKCityWork::Update(S,ResearchTestTime-1));
	TestTrue(TEXT("Very long offline completes"),DYKCityWork::Update(S,ResearchTestTime+30*3600));
	TestTrue(TEXT("Research queue empty"),S.State.CityEconomy.ResearchQueue.IsEmpty());
	TestTrue(TEXT("Completed state valid"),DYKCityResearch::Validate(S.State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKResearchChronologyTest,"DYK.M207.SharedWalletChronologicalOfflineEvents",ResearchTestFlags)
bool FDYKResearchChronologyTest::RunTest(const FString& Parameters)
{
	FResearchTestService S;
	FDYKCityBuildingWorkRequest Castle; Castle.CommandId=FGuid::NewGuid(); Castle.InstanceId=S.State.CityEconomy.Instances[0].InstanceId;
	Castle.Kind=EDYKCityBuildingWorkKind::Upgrade; Castle.BuildingId=FDYKBuildingId(TEXT("CITY-CASTLE")); Castle.ExpectedLevel=4;
	TestTrue(TEXT("Builder occupied until 75s"),DYKCityWork::Enqueue(S,Castle,ResearchTestTime));
	FDYKCityBuildingWorkRequest House; House.CommandId=FGuid::NewGuid(); House.InstanceId=FGuid::NewGuid(); House.BuildingId=FDYKBuildingId(TEXT("CITY-HOUSE"));
	FDYKCityBuildingWorkPlan Plan; bool Placed=false;
	for(int32 Y=0;Y<50 && !Placed;++Y) for(int32 X=0;X<50 && !Placed;++X)
	{
		House.Origin={X,Y};
		if(DYKCityBuildingRules::Prepare(S.State,House,DYKCityTerritory::DEV_PLACEHOLDER(),DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan)) Placed=true;
	}
	TestTrue(TEXT("Reserve queued house"),Placed && DYKCityWork::Enqueue(S,House,ResearchTestTime));
	TestTrue(TEXT("First research starts"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),TEXT("RESEARCH-INFANTRY-T2"),ResearchTestTime));
	TestTrue(TEXT("Second research waits for 20s slot release"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),TEXT("RESEARCH-RANGED-T2"),ResearchTestTime));
	for(auto& R:S.State.Resources) if(R.ResourceId.Name==TEXT("RES-WOOD")) R.Amount=300;
	TestTrue(TEXT("Single offline update orders both queues"),DYKCityWork::Update(S,ResearchTestTime+100));
	TestTrue(TEXT("Earlier 20s research consumes available wood"),S.State.CityEconomy.ResearchQueue.IsEmpty());
	TestEqual(TEXT("Later construction still waits for its unpaid resources"),S.State.CityEconomy.Work.Num(),1);
	TestEqual(TEXT("Later task cannot overtake earlier research payment"),S.State.CityEconomy.Work[0].StartedAtUnix,int64(0));
	TestEqual(TEXT("Shared wallet exact and nonnegative"),ResearchTestBalance(S,TEXT("RES-WOOD")),int64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKResearchDiskTest,"DYK.M207.RealProfileFilesRestartAndReplay",ResearchTestFlags)
bool FDYKResearchDiskTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> Owner(NewObject<UGameInstance>());
	const FString Prefix=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("M207/file-tests"),FGuid::NewGuid().ToString(EGuidFormats::Digits),TEXT("profile")));
	auto* Save=NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKCityResearchTestAccess::Open(*Save,Prefix);
	FResearchTestService Fixture;
	TestTrue(TEXT("Seed actual isolated file profile"),Save->ApplyProfileCommand(FGuid::NewGuid(),[&Fixture](auto& P){P=Fixture.State; return true;}));
	const auto First=FGuid::NewGuid(),Second=FGuid::NewGuid();
	TestTrue(TEXT("Persist first active research"),DYKCityResearch::Enqueue(*Save,First,TEXT("RESEARCH-INFANTRY-T2"),ResearchTestTime));
	TestTrue(TEXT("Persist second queued research"),DYKCityResearch::Enqueue(*Save,Second,TEXT("RESEARCH-RANGED-T2"),ResearchTestTime));
	const auto Expected=*Save->GetProfileSnapshot();
	auto* Reload=NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKCityResearchTestAccess::Open(*Reload,Prefix);
	if(!TestNotNull(TEXT("Actual disk reload"),Reload->GetProfileSnapshot())) return false;
	TestTrue(TEXT("Active work, unpaid work, costs and clocks restored exactly"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Expected,Reload->GetProfileSnapshot(),0));
	TestTrue(TEXT("Complete both after restart"),DYKCityWork::Update(*Reload,ResearchTestTime+40));
	const auto Final=*Reload->GetProfileSnapshot();
	auto* Again=NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKCityResearchTestAccess::Open(*Again,Prefix);
	TestTrue(TEXT("Completed graph survives another disk reload"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Final,Again->GetProfileSnapshot(),0));
	TestTrue(TEXT("Old enqueue acknowledged after reload"),DYKCityResearch::Enqueue(*Again,Second,TEXT("RESEARCH-RANGED-T2"),ResearchTestTime+100));
	TestTrue(TEXT("Old command does not pay or research again"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Final,Again->GetProfileSnapshot(),0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKResearchCombatSnapshotTest,"DYK.M207.NativeCombatResearchSnapshotWithoutPlay",ResearchTestFlags)
bool FDYKResearchCombatSnapshotTest::RunTest(const FString& Parameters)
{
	// A test-only world: no level, viewport, game loop, rendering or screenshots.
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine)); GI->Init();
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false); World->SetGameInstance(GI.Get());
	struct FResearchWorldCleanup {UWorld* W; UGameInstance* G; ~FResearchWorldCleanup(){W->DestroyWorld(false); G->Shutdown();}} Cleanup{World,GI.Get()};
	auto* Save=GI->GetSubsystem<UDYKSaveSubsystem>();
	if(!TestNotNull(TEXT("Actual profile service"),Save)) return false;
	FResearchTestService Fixture;
	TestTrue(TEXT("Save researched infantry modifiers"),Save->ApplyProfileCommand(FGuid::NewGuid(),[&Fixture](auto& P)
	{
		P=Fixture.State;
		for(const auto Key:{TEXT("RESEARCH-INFANTRY-HP"),TEXT("RESEARCH-INFANTRY-ARMOR"),TEXT("RESEARCH-INFANTRY-DAMAGE")}) P.CityEconomy.CompletedResearch.Add(Key);
		return DYKCityResearch::Validate(P);
	}));
	auto* Mode=World->SpawnActor<ADYKPolygonGameMode>(); auto* Defender=World->SpawnActor<ADYKDefenderActor>();
	if(!TestNotNull(TEXT("Native mode"),Mode) || !TestNotNull(TEXT("Native defender"),Defender)) return false;
	FDYKBattleDefinition Battle; Battle.ProtectedObjects.Add({TEXT("TestTarget"),100,true}); Battle.Waves.Add({{TEXT("TestEntry")},0}); Battle.InitialSupplies=10;
	FString Error;
	TestTrue(TEXT("Initialize pure battle state"),Mode->GetBattleSession().Initialize(Battle,Error));
	TestTrue(TEXT("Initialize actual combat actor"),Defender->Initialize(Mode,FGuid::NewGuid()));
	TestEqual(TEXT("Actual HP includes researched ten"),Defender->GetHP(),25);
	TestEqual(TEXT("Actual attack includes researched two"),Defender->GetUnitDefinition()->Base.Attack.DamagePerHit.Value,6);
	TestTrue(TEXT("Register defender in pure session"),Mode->GetBattleSession().AddDefense(Defender->GetDefenseId()));
	TestTrue(TEXT("Enter active phase without playing/ticking the world"),Mode->GetBattleSession().StartWave(Mode->GetBattleSession().GetPreparationToken(),0));
	TestTrue(TEXT("Apply actual physical damage"),Defender->ApplyDamage(4));
	TestEqual(TEXT("Flat researched physical armor subtracts one"),Defender->GetHP(),22);
	TestTrue(TEXT("Apply actual magic damage"),Defender->ApplyDamage(4,EDYKDamageType::Magical));
	TestEqual(TEXT("Physical research cannot reduce magic hit"),Defender->GetHP(),18);
	Defender->RestoreAfterWave(); TestEqual(TEXT("Restore uses researched maximum"),Defender->GetHP(),25);
	TestEqual(TEXT("Shared base catalog never changed"),DYKUnitCatalog::Find(DYKUnitIds::Peasant())->Base.MaxHP.Value,15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKResearchRewardTimeTest,"DYK.M207.NewRewardCannotBackdatePendingWork",ResearchTestFlags)
bool FDYKResearchRewardTimeTest::RunTest(const FString& Parameters)
{
	FResearchTestService S;
	const int64 Now=FDateTime::UtcNow().ToUnixTimestamp();
	TestTrue(TEXT("Research started before absence"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),TEXT("RESEARCH-INFANTRY-DAMAGE"),Now-100));
	for(auto& R:S.State.Resources) R.Amount=0;
	FDYKCityBuildingWorkRequest House; House.CommandId=FGuid::NewGuid(); House.InstanceId=FGuid::NewGuid(); House.BuildingId=FDYKBuildingId(TEXT("CITY-HOUSE"));
	FDYKCityBuildingWorkPlan Plan; bool Placed=false;
	for(int32 Y=0;Y<50 && !Placed;++Y) for(int32 X=0;X<50 && !Placed;++X)
	{
		House.Origin={X,Y};
		if(DYKCityBuildingRules::Prepare(S.State,House,DYKCityTerritory::DEV_PLACEHOLDER(),DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan)) Placed=true;
	}
	TestTrue(TEXT("Unfunded house waits"),Placed && DYKCityWork::Enqueue(S,House,Now-99));
	FDYKBattleResult Result; Result.AttemptId=FGuid::NewGuid(); Result.Outcome=EDYKBattlePhase::Victory;
	FDYKMissionRewardDefinition Reward; Reward.Campaign=FDYKCampaignId(TEXT("TEST-RESEARCH-CAMPAIGN"));
	Reward.Mission=FDYKMissionId(TEXT("TEST-RESEARCH-MISSION")); Reward.RewardId=TEXT("TEST-RESEARCH-TIMING-REWARD");
	Reward.Resources={{FDYKResourceId(TEXT("RES-WOOD")).ToContentId(),20},{FDYKResourceId(TEXT("RES-STONE")).ToContentId(),20}};
	TestTrue(TEXT("New mission reward received now"),DYKMissionRewards::Settle(S,Result,Reward));
	TestTrue(TEXT("Past research completion settled before new money"),S.State.CityEconomy.ResearchQueue.IsEmpty());
	TestTrue(TEXT("Refresh starts newly funded construction"),DYKCityWork::Update(S,FDateTime::UtcNow().ToUnixTimestamp()));
	TestEqual(TEXT("House still building, never completed using future money"),S.State.CityEconomy.Work.Num(),1);
	TestTrue(TEXT("Construction cannot be backdated to prior research completion"),S.State.CityEconomy.Work[0].StartedAtUnix>=Now);
	return true;
}
#endif
