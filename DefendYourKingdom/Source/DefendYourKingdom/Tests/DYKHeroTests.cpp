#include "../Meta/DYKHeroes.h"
#include "DYKCityHUDTestProfile.h"
#include "../Core/Content/DYKUnitCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SplineComponent.h"
#include "Components/Button.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "../Battle/DYKPolygonGameMode.h"
#include "../Battle/DYKDefenderActor.h"
#include "../Battle/DYKEnemyActor.h"
#include "../Battle/DYKEnemyRoute.h"
#include "../Battle/DYKUnitSlot.h"
#include "../UI/DYKHeroAbilityWidget.h"
#include "../UI/DYKHeroPortraitWidget.h"
#include "Blueprint/WidgetTree.h"
#include "../Meta/DYKCityDecorationActor.h"
#include "../Meta/DYKCityWorkWidget.h"
#include "UObject/StrongObjectPtr.h"

struct FDYKHeroTestAccess
{
	static void RefreshTargets(ADYKPolygonGameMode& Mode)
	{Mode.RefreshCombatTargets(Mode.GetWorld(),LEVELTICK_All,0);}
};

namespace
{
	constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
	using namespace DYKCityHUDTest;
	void Fund(FDYKPlayerProfileSaveData& P,int64 Gold=10000)
	{for(auto& R:P.Resources) if(R.ResourceId.Name==TEXT("RES-GOLD")) R.Amount=Gold;}
	bool Upgrade(Service& S,FName Branch,int64& Now)
	{
		const FName Id=DYKHeroes::NextResearch(S.State,Branch);
		if(!DYKCityResearch::Enqueue(S,FGuid::NewGuid(),Id,Now)) return false;
		Now+=20; return DYKCityWork::Update(S,Now);
	}
	struct FHeroWorld
	{
		TStrongObjectPtr<UGameInstance> GI{NewObject<UGameInstance>(GEngine)};
		UWorld* World=nullptr;
		ADYKPolygonGameMode* Mode=nullptr;
		FHeroWorld()
		{
			GI->Init(); World=UWorld::CreateWorld(EWorldType::Game,false); World->SetGameInstance(GI.Get());
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Mode=World->SpawnActor<ADYKPolygonGameMode>();
			FDYKBattleDefinition B; B.InitialSupplies=100; B.ProtectedObjects.Add({TEXT("TestTarget"),100,true});
			B.Waves.Add({{TEXT("TestEntry")},10}); B.Waves.Add({{TEXT("TestEntry")},0});
			FString Error; check(Mode->GetBattleSession().Initialize(B,Error));
		}
		~FHeroWorld() {World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GI->Shutdown();}
		TArray<ADYKDefenderActor*> Heroes;
		ADYKDefenderActor* Hero(bool Story=false)
		{
			auto* H=World->SpawnActor<ADYKDefenderActor>(); H->HeroType=DYKUnitIds::Paladin().GetName(); H->bMissionPlaced=Story;
			if(!H->Initialize(Mode,FGuid::NewGuid())) {H->Destroy(); return nullptr;}
			Mode->GetBattleSession().AddDefense(H->GetDefenseId()); Heroes.Add(H); return H;
		}
		ADYKEnemyActor* Enemy(FVector Position)
		{
			auto* R=World->SpawnActor<ADYKEnemyRoute>(); R->RouteId=FName(*FGuid::NewGuid().ToString()); R->ProtectedObjectId=TEXT("TestTarget");
			R->Path->SetSplinePoints({Position,Position+FVector(100,0,0)},ESplineCoordinateSpace::World,true);
			return Mode->SpawnEnemy(R,Mode->GetBattleSession().GetWaveToken(),TEXT("TestEntry"));
		}
		void Timers(float Delta) {++GFrameCounter; World->TimeSeconds+=Delta; World->GetTimerManager().Tick(Delta);}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKHeroHireTest,"DYK.M208.AcquisitionAndTransactions",Flags)
bool FDYKHeroHireTest::RunTest(const FString&)
{
	Service S; Fund(S.State); const int64 Gold=DYKCityEconomy::Balance(S.State,FDYKResourceId(TEXT("RES-GOLD")));
	const int32 Castle=S.State.Buildings[0].Level; S.State.Buildings[0].Level=1;
	TestFalse(TEXT("P07-T01 composite condition blocks hire"),DYKHeroes::Hire(S,FGuid::NewGuid(),Time));
	TestTrue(TEXT("Locked reason names castle level"),DYKHeroes::HireBlockReason(S.State).Contains(TEXT("2")));
	TestEqual(TEXT("Rejected hire never charges"),DYKCityEconomy::Balance(S.State,FDYKResourceId(TEXT("RES-GOLD"))),Gold);
	S.State.Buildings[0].Level=Castle;
	S.Fail=true; TestFalse(TEXT("Write failure rejects hire"),DYKHeroes::Hire(S,FGuid::NewGuid(),Time));
	TestFalse(TEXT("Write failure grants no ownership"),DYKHeroes::Owns(S.State)); S.Fail=false;
	const FGuid Command=FGuid::NewGuid(); TestTrue(TEXT("Hire permanently"),DYKHeroes::Hire(S,Command,Time));
	TestTrue(TEXT("Retry same command is acknowledged"),DYKHeroes::Hire(S,Command,Time));
	TestFalse(TEXT("New command cannot hire duplicate"),DYKHeroes::Hire(S,FGuid::NewGuid(),Time));
	TestEqual(TEXT("One permanent hero"),S.State.OwnedHeroes.Num(),1);
	TestEqual(TEXT("Exactly one gold payment"),DYKCityEconomy::Balance(S.State,FDYKResourceId(TEXT("RES-GOLD"))),Gold-DYKHeroes::HireGoldDEV_PLACEHOLDER());
	TestTrue(TEXT("Hero collection uses Hero category"),S.State.OwnedHeroes[0].Kind==EDYKContentKind::Hero);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKHeroProgressTest,"DYK.M208.LevelEquipmentQueueAndCancellation",Flags)
bool FDYKHeroProgressTest::RunTest(const FString&)
{
	Service S; Fund(S.State); int64 Now=Time;
	TestFalse(TEXT("Unowned hero cannot upgrade"),DYKCityResearch::CanEnqueue(S.State,DYKHeroes::ResearchId(TEXT("LEVEL"),1)));
	TestTrue(TEXT("Hire fixture"),DYKHeroes::Hire(S,FGuid::NewGuid(),Now));
	TestTrue(TEXT("First weapon tier"),Upgrade(S,TEXT("WEAPON"),Now));
	const auto Before=S.State;
	TestFalse(TEXT("P14-T09 second weapon tier blocked before hero level 3"),DYKCityResearch::Enqueue(S,FGuid::NewGuid(),DYKHeroes::ResearchId(TEXT("WEAPON"),2),Now));
	TestTrue(TEXT("Blocked gate changes no state or costs"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Before,&S.State,0));
	TestTrue(TEXT("Level 2"),Upgrade(S,TEXT("LEVEL"),Now)); TestTrue(TEXT("Level 3"),Upgrade(S,TEXT("LEVEL"),Now));
	TestTrue(TEXT("Tier 2 now allowed"),DYKCityResearch::CanEnqueue(S.State,DYKHeroes::ResearchId(TEXT("WEAPON"),2)));
	const auto Gold=DYKCityEconomy::Balance(S.State,FDYKResourceId(TEXT("RES-GOLD")));
	const FGuid Weapon=FGuid::NewGuid(),Armor=FGuid::NewGuid();
	TestTrue(TEXT("Start weapon"),DYKCityResearch::Enqueue(S,Weapon,DYKHeroes::ResearchId(TEXT("WEAPON"),2),Now));
	TestTrue(TEXT("Independent branch queues in same research cell"),DYKCityResearch::Enqueue(S,Armor,DYKHeroes::ResearchId(TEXT("ARMOR"),1),Now));
	TestEqual(TEXT("One active payment only"),DYKCityEconomy::Balance(S.State,FDYKResourceId(TEXT("RES-GOLD"))),Gold-100);
	TestTrue(TEXT("Cancel pending work"),DYKCityResearch::Cancel(S,FGuid::NewGuid(),Armor,Now));
	TestTrue(TEXT("Cancel paid work"),DYKCityResearch::Cancel(S,FGuid::NewGuid(),Weapon,Now));
	TestEqual(TEXT("Exact refund"),DYKCityEconomy::Balance(S.State,FDYKResourceId(TEXT("RES-GOLD"))),Gold);
	TestFalse(TEXT("Double refund rejected"),DYKCityResearch::Cancel(S,FGuid::NewGuid(),Weapon,Now));
	TestTrue(TEXT("Armor tier 1"),Upgrade(S,TEXT("ARMOR"),Now)); TestTrue(TEXT("Ability tier 1"),Upgrade(S,TEXT("ABILITY"),Now));
	const auto Stats=DYKCityResearch::EffectiveStats(S.State,*DYKUnitCatalog::Find(DYKUnitIds::Paladin()));
	TestEqual(TEXT("Permanent HP"),Stats.MaxHP.Value,90); TestEqual(TEXT("Separate level and weapon damage"),Stats.Attack.DamagePerHit.Value,14);
	TestEqual(TEXT("Physical armor"),Stats.PhysicalArmor.Value,3); TestEqual(TEXT("Magical armor"),Stats.MagicalArmor.Value,3);
	TestEqual(TEXT("Ability branch"),DYKHeroes::Ability(&S.State).Damage.Value,25);
	while(DYKHeroes::Level(S.State)<10) if(!TestTrue(TEXT("Sequential level upgrade"),Upgrade(S,TEXT("LEVEL"),Now))) return false;
	TestTrue(TEXT("Level cap 10"),DYKHeroes::NextResearch(S.State,TEXT("LEVEL")).IsNone());
	TestFalse(TEXT("Cannot invent level 11"),DYKCityResearch::CanEnqueue(S.State,DYKHeroes::ResearchId(TEXT("LEVEL"),10)));
	TestEqual(TEXT("Shared base stays 70"),DYKUnitCatalog::Find(DYKUnitIds::Paladin())->Base.MaxHP.Value,70);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKHeroDiskTest,"DYK.M208.RealDiskOwnershipAndOfflineProgress",Flags)
bool FDYKHeroDiskTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameInstance> Owner(NewObject<UGameInstance>());
	const FString Prefix=FPaths::ProjectSavedDir()/TEXT("M208Profiles")/FGuid::NewGuid().ToString();
	auto* Save=NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKCityHUDTestAccess::Open(*Save,Prefix);
	TestTrue(TEXT("Seed durable city"),Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& P){if(!Seed(P)) return false; Fund(P); return true;}));
	const FGuid Hire=FGuid::NewGuid(),Work=FGuid::NewGuid();
	TestTrue(TEXT("Durable hire"),DYKHeroes::Hire(*Save,Hire,Time));
	TestTrue(TEXT("Durable active level"),DYKCityResearch::Enqueue(*Save,Work,DYKHeroes::ResearchId(TEXT("LEVEL"),1),Time));
	TestTrue(TEXT("Durable pending weapon"),DYKCityResearch::Enqueue(*Save,FGuid::NewGuid(),DYKHeroes::ResearchId(TEXT("WEAPON"),1),Time));
	auto* Reload=NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKCityHUDTestAccess::Open(*Reload,Prefix);
	TestTrue(TEXT("P07-T02 ownership survives restart"),DYKHeroes::Owns(*Reload->GetProfileSnapshot()));
	TestEqual(TEXT("Unfinished queue survives restart"),Reload->GetProfileSnapshot()->CityEconomy.ResearchQueue.Num(),2);
	TestTrue(TEXT("Offline completion"),DYKCityWork::Update(*Reload,Time+40));
	TestEqual(TEXT("Level survives offline"),DYKHeroes::Level(*Reload->GetProfileSnapshot()),2);
	TestEqual(TEXT("Equipment survives offline"),DYKHeroes::BranchLevel(*Reload->GetProfileSnapshot(),TEXT("WEAPON")),1);
	const auto Before=*Reload->GetProfileSnapshot();
	TestTrue(TEXT("Hire retry after process restart"),DYKHeroes::Hire(*Reload,Hire,Time+100));
	TestTrue(TEXT("Queue retry after restart"),DYKCityResearch::Enqueue(*Reload,Work,DYKHeroes::ResearchId(TEXT("LEVEL"),1),Time+100));
	TestTrue(TEXT("No repeated costs or progress"),FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&Before,Reload->GetProfileSnapshot(),0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKHeroDeploymentTest,"DYK.M208.DeploymentLimitsStoryAndSale",Flags)
bool FDYKHeroDeploymentTest::RunTest(const FString&)
{
	TArray<FName> Players{TEXT("A"),TEXT("B")},Story{TEXT("C")};
	TestFalse(TEXT("P07-T03 third player hero blocked"),DYKHeroes::CanDeploy(TEXT("D"),Players,Story,false));
	TestTrue(TEXT("P07-T28 story ignores two-player cap"),DYKHeroes::CanDeploy(TEXT("D"),Players,Story,true));
	TestFalse(TEXT("Story identity also unique"),DYKHeroes::CanDeploy(TEXT("A"),Players,Story,true));
	Players.Pop(); TestTrue(TEXT("Story does not occupy player cap"),DYKHeroes::CanDeploy(TEXT("D"),Players,Story,false));
	FHeroWorld F;
	auto* Save=F.GI->GetSubsystem<UDYKSaveSubsystem>();
	TestTrue(TEXT("Seed city and hire"),Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& P){if(!Seed(P)) return false; Fund(P);return true;}));
	TestTrue(TEXT("Hire"),DYKHeroes::Hire(*Save,FGuid::NewGuid(),Time));
	auto* Slot=F.World->SpawnActor<ADYKUnitSlot>(); Slot->SlotId=TEXT("HeroTestSlot");
	auto* Other=F.World->SpawnActor<ADYKUnitSlot>(); Other->SlotId=TEXT("OtherSlot");
	auto& B=F.Mode->GetBattleSession(); const auto Prep=B.GetPreparationToken();
	F.Mode->bAllowPlayerHeroes=false; TestNull(TEXT("Mission ban blocks placement"),F.Mode->PlaceDefender(Slot,Prep,true)); F.Mode->bAllowPlayerHeroes=true;
	F.Mode->AllowedHeroIds={TEXT("OTHER")}; TestNull(TEXT("Mission allowlist blocks placement"),F.Mode->PlaceDefender(Slot,Prep,true)); F.Mode->AllowedHeroIds.Reset();
	TestNull(TEXT("Stale token does not place"),F.Mode->PlaceDefender(Slot,FGuid::NewGuid(),true));
	auto* Hero=F.Mode->PlaceDefender(Slot,Prep,true); if(!TestNotNull(TEXT("Ordinary slot accepts owned hero"),Hero)) return false;
	TestEqual(TEXT("Authored 12 supplies"),B.GetSupplies(),88);
	TestNull(TEXT("Duplicate rejected without charge"),F.Mode->PlaceDefender(Other,Prep,true)); TestEqual(TEXT("Still 88"),B.GetSupplies(),88);
	TestTrue(TEXT("Sell returns exact price"),F.Mode->SellDefender(Slot,Prep)); TestEqual(TEXT("Full refund"),B.GetSupplies(),100);
	TestTrue(TEXT("Sale preserves collection"),DYKHeroes::Owns(*Save->GetProfileSnapshot()));
	TestFalse(TEXT("Cannot sell twice"),F.Mode->SellDefender(Slot,Prep));
	Hero=F.Hero(true); if(!TestNotNull(TEXT("Story hero independent of slot and ownership"),Hero)) return false;
	TestFalse(TEXT("Story blocks duplicate identity"),F.Mode->CanDeployHero(Hero->HeroType));
	auto* City=F.World->SpawnActor<ADYKCityDecorationActor>(); City->Refresh(*Save->GetProfileSnapshot());
	TestEqual(TEXT("Owned hero has a city road resident"),City->HeroResidents->GetInstanceCount(),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKHeroAbilityTest,"DYK.M208.NativeAbilityDeathPauseAndWaveBoundary",Flags)
bool FDYKHeroAbilityTest::RunTest(const FString&)
{
	FHeroWorld F; auto* Hero=F.Hero(true); if(!TestNotNull(TEXT("Native story paladin"),Hero)) return false;
	TestEqual(TEXT("Passport HP"),Hero->GetHP(),70); TestEqual(TEXT("Passport death recovery"),Hero->GetUnitDefinition()->Base.DeathCooldownSeconds.Value,60.f);
	TestFalse(TEXT("Preparation blocks ability"),Hero->ActivateAbility());
	auto* UI=NewObject<UDYKHeroAbilityWidget>(F.World); UI->Initialize(); UI->TakeWidget(); UI->Configure(Hero);
	TestTrue(TEXT("Overhead button hidden in preparation"),UI->Button->GetVisibility()==ESlateVisibility::Collapsed);
	auto& B=F.Mode->GetBattleSession(); TestTrue(TEXT("Start automated wave without Play"),B.StartWave(B.GetPreparationToken(),0));
	UI->Refresh(); TestTrue(TEXT("Overhead button enabled in active wave"),UI->Button->GetIsEnabled());
	TestTrue(TEXT("Overhead button visible in active wave"),UI->Button->GetVisibility()==ESlateVisibility::Visible);
	auto* Near=F.Enemy({150,0,0}); auto* Far=F.Enemy({600,0,0});
	if(!TestNotNull(TEXT("Near target"),Near) || !TestNotNull(TEXT("Far target"),Far)) return false;
	F.Timers(1); TestEqual(TEXT("No autocast"),Near->GetHP(),10);
	F.World->GetWorldSettings()->SetPauserPlayerState(F.World->SpawnActor<APlayerState>());
	TestFalse(TEXT("Paused ability rejected"),Hero->ActivateAbility()); F.World->GetWorldSettings()->SetPauserPlayerState(nullptr);
	UI->Activate(); UI->Refresh();
	TestTrue(TEXT("Overhead button disappears during cooldown"),UI->Button->GetVisibility()==ESlateVisibility::Collapsed);
	TestTrue(TEXT("Cooldown begins at press"),Hero->AbilityCooldownRemaining()>24.9);
	TestFalse(TEXT("Double press rejected"),Hero->ActivateAbility()); F.Timers(.4f);
	TestFalse(TEXT("Automatic forward area kills near target"),Near->IsAlive()); TestEqual(TEXT("Outside area untouched"),Far->GetHP(),10);
	Hero->ApplyDamage(1000); TestFalse(TEXT("Dead hero cannot cast"),Hero->ActivateAbility()); TestFalse(TEXT("Death cooldown distinct from ability"),Hero->CanResurrect());
	F.World->TimeSeconds+=59; TestFalse(TEXT("Not before 60 seconds"),Hero->CanResurrect());
	F.World->TimeSeconds+=1; TestTrue(TEXT("Manual free recovery after 60"),Hero->Resurrect()); TestEqual(TEXT("Full HP"),Hero->GetHP(),70);
	Far->SetActorLocation({150,0,0});
	TestTrue(TEXT("Ability can be used after independent cooldown"),Hero->ActivateAbility()); Hero->ApplyDamage(1000); F.Timers(.4f);
	TestEqual(TEXT("Death cancels pending impact"),Far->GetHP(),10);
	Far->ApplyDamage(1000); const auto Wave=B.GetWaveToken(); B.CompleteSpawnGroup(Wave,TEXT("TestEntry"));
	TestTrue(TEXT("Resolve wave"),B.ResolveWave(Wave,F.World->GetTimeSeconds()));
	TestEqual(TEXT("Wave restores dead hero"),Hero->GetHP(),70); TestEqual(TEXT("Wave resets ability"),Hero->AbilityCooldownRemaining(),0.);
	TestFalse(TEXT("Interwave still forbids ability"),Hero->ActivateAbility());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKHeroTavernTest,"DYK.M208.NativeTavernHireUpgradeAndBiography",Flags)
bool FDYKHeroTavernTest::RunTest(const FString&)
{
	FHeroWorld F; auto* Save=F.GI->GetSubsystem<UDYKSaveSubsystem>();
	const int64 Now=FDateTime::UtcNow().ToUnixTimestamp();
	TestTrue(TEXT("Seed tavern"),Save->ApplyProfileCommand(FGuid::NewGuid(),[Now](auto& P){if(!Seed(P,Now)) return false;Fund(P);return true;}));
	FURL URL; URL.AddOption(TEXT("game=/Script/DefendYourKingdom.DYKCityGameMode"));
	if(!TestTrue(TEXT("City authority without Play"),F.World->SetGameMode(URL))) return false;
	auto* UI=NewObject<UDYKCityWorkWidget>(F.World); UI->Initialize(); UI->TakeWidget();
	UI->OpenBuilding(Instance(*Save->GetProfileSnapshot(),TEXT("CITY-TAVERN")),TEXT("CITY-TAVERN"));
	UI->Dispatch(EDYKCityUIAction::Visit);
	const auto Card=DYKCityBuildingUI::Items(TEXT("CITY-TAVERN"))[0];
	TestFalse(TEXT("Paladin is implemented content, not a future cell"),Card.bFuture);
	TestFalse(TEXT("Available paladin has no lock"),DYKCityBuildingUI::IsLocked(*Save->GetProfileSnapshot(),Card));
	bool HasPortrait=false; UI->WidgetTree->ForEachWidget([&HasPortrait](UWidget* W){HasPortrait|=W->IsA<UDYKHeroPortraitWidget>();});
	TestTrue(TEXT("Actual recruitment card contains monochrome hero portrait"),HasPortrait);
	UI->Dispatch(EDYKCityUIAction::Item,0);
	auto* Hire=UI->FindAction(EDYKCityUIAction::HeroHire);
	if(!TestNotNull(TEXT("Tavern exposes real hire action"),Hire)) return false;
	TestTrue(TEXT("Hire enabled"),Hire->GetIsEnabled()); Hire->OnClicked.Broadcast();
	TestTrue(TEXT("UMG action permanently acquires hero"),DYKHeroes::Owns(*Save->GetProfileSnapshot()));
	TestFalse(TEXT("Owned paladin stays visible in collection"),DYKCityBuildingUI::IsLocked(*Save->GetProfileSnapshot(),Card));
	TestNull(TEXT("Hire button replaced with progression"),UI->FindAction(EDYKCityUIAction::HeroHire));
	for(int32 I=0;I<4;++I) TestNotNull(TEXT("Level and three equipment branches"),UI->FindAction(EDYKCityUIAction::HeroResearch,I));
	UI->Dispatch(EDYKCityUIAction::HeroBiography); TestNotNull(TEXT("Biography toggle remains available"),UI->FindAction(EDYKCityUIAction::HeroBiography));
	UI->Dispatch(EDYKCityUIAction::HeroBiography);
	UI->FindAction(EDYKCityUIAction::HeroResearch,0)->OnClicked.Broadcast();
	TestEqual(TEXT("Real upgrade queue from UMG"),Save->GetProfileSnapshot()->CityEconomy.ResearchQueue.Num(),1);
	auto* Cancel=UI->FindAction(EDYKCityUIAction::CancelResearch,0);
	if(!TestNotNull(TEXT("Active improvement cancellable in tavern"),Cancel)) return false;
	Cancel->OnClicked.Broadcast(); TestEqual(TEXT("Cancellation removes research"),Save->GetProfileSnapshot()->CityEconomy.ResearchQueue.Num(),0);
	UI->Dispatch(EDYKCityUIAction::Back); TestTrue(TEXT("Return to tavern collection"),UI->GetWindowState().Window==EDYKCityWindow::Interior);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKHeroCardAvailabilityTest,"DYK.M208.CardUnlockIndependentOfWallet",Flags)
bool FDYKHeroCardAvailabilityTest::RunTest(const FString&)
{
	Service S; const auto Card=DYKCityBuildingUI::Items(TEXT("CITY-TAVERN"))[0]; Fund(S.State,0);
	TestFalse(TEXT("No gold does not hide an unlocked character behind a lock"),DYKCityBuildingUI::IsLocked(S.State,Card));
	TestFalse(TEXT("No gold still prevents paying for hire"),DYKHeroes::Hire(S,FGuid::NewGuid(),Time));
	S.State.Buildings[0].Level=1;
	TestTrue(TEXT("Actual unlock requirement still locks hero"),DYKCityBuildingUI::IsLocked(S.State,Card));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKHeroRegistryDeathTest,"DYK.M208.InstantKillAndHammerDuringTargetRefresh",Flags)
bool FDYKHeroRegistryDeathTest::RunTest(const FString&)
{
	FHeroWorld F; auto* Hero=F.Hero(true); if(!TestNotNull(TEXT("Paladin"),Hero)) return false;
	auto& B=F.Mode->GetBattleSession(); if(!TestTrue(TEXT("Active wave"),B.StartWave(B.GetPreparationToken(),0))) return false;
	auto* First=F.Enemy({100,0,0}); auto* Second=F.Enemy({120,0,0}); auto* Third=F.Enemy({140,0,0});
	if(!TestNotNull(TEXT("First target"),First) || !TestNotNull(TEXT("Second target"),Second) || !TestNotNull(TEXT("Third target"),Third)) return false;
	FDYKHeroTestAccess::RefreshTargets(*F.Mode);
	TestFalse(TEXT("Immediate normal hit destroys a later registry entry in the same pass"),First->IsAlive());
	TestTrue(TEXT("Other entries still valid after lethal target refresh"),Second->IsAlive() && Third->IsAlive());
	TestTrue(TEXT("Click hammer after lethal normal attack"),Hero->ActivateAbility());
	F.Timers(0); F.Timers(.4f);
	TestFalse(TEXT("Hammer kills second enemy"),Second->IsAlive()); TestFalse(TEXT("Hammer kills third enemy"),Third->IsAlive());
	FDYKHeroTestAccess::RefreshTargets(*F.Mode);
	TestEqual(TEXT("Registry update after AoE leaves no active enemies"),B.GetActiveEnemyCount(),0);
	TestNull(TEXT("No retained destroyed target"),Hero->GetCurrentTarget());
	TestEqual(TEXT("Ability and target refresh spend no extra supplies"),B.GetSupplies(),100);
	return true;
}
#endif
