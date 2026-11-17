#include "../Meta/DYKCityEconomy.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr auto ProductionFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int64 ProductionTime = 1794700000;
	int32 ProductionIndex(const FDYKPlayerProfileSaveData& P, const TCHAR* Name)
	{
		return P.Buildings.IndexOfByPredicate([Name](const auto& B) { return B.BuildingId.Name == FName(Name); });
	}
	FGuid ProductionGuid(const FDYKPlayerProfileSaveData& P, int32 Index)
	{
		return P.CityEconomy.Instances.FindByPredicate([Index](const auto& S) { return S.BuildingIndex == Index; })->InstanceId;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProductionLevelsTest,"DYK.M205.LevelsAndOfflineLimits",ProductionFlags)
bool FDYKProductionLevelsTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Initialize"),DYKCityEconomy::Initialize(P,ProductionTime))) return false;
	const int32 Saw = ProductionIndex(P,TEXT("CITY-SAWMILL"));
	const FGuid Id = ProductionGuid(P,Saw);
	const int32 Hours[] = {12,16,24};
	const int64 Rates[] = {2,4,6};
	for (int32 Level=1;Level<=3;++Level)
	{
		P.Buildings[Saw].Level=Level;
		const auto V=DYKCityEconomy::View(P,Id,ProductionTime+30*3600);
		TestEqual(TEXT("Author storage duration at current rate"),V.Capacity,Hours[Level-1]*60*Rates[Level-1]);
		TestEqual(TEXT("Thirty hours capped only by this instance"),V.Stored,V.Capacity);
		TestEqual(TEXT("Arbitrary long interval saturates without overflow"),DYKCityEconomy::View(P,Id,MAX_int64).Stored,V.Capacity);
		TestEqual(TEXT("No backwards-time income"),DYKCityEconomy::View(P,Id,ProductionTime-1).Stored,int64(0));
		TestEqual(TEXT("Manual collection only"),DYKCityEconomy::Balance(P,FDYKResourceId(TEXT("RES-WOOD"))),int64(0));
	}
	const int32 House=ProductionIndex(P,TEXT("CITY-HOUSE"));
	for (int32 Level=1;Level<=3;++Level)
	{
		P.Buildings[House].Level=Level;
		const auto V=DYKCityEconomy::View(P,ProductionGuid(P,House),ProductionTime+30*3600);
		TestEqual(TEXT("House stores precisely one cycle"),V.Stored,int64(Level));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKProductionTransitionTest,"DYK.M205.UpgradeBoundaryAndSaveRoundtrip",ProductionFlags)
bool FDYKProductionTransitionTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Initialize"),DYKCityEconomy::Initialize(P,ProductionTime))) return false;
	const int32 Saw=ProductionIndex(P,TEXT("CITY-SAWMILL"));
	const FGuid Id=ProductionGuid(P,Saw);
	TestTrue(TEXT("Complete level 2 at exact boundary"),DYKCityEconomy::CompleteUpgrade(P,Id,2,ProductionTime+90));
	const auto* S=P.CityEconomy.Instances.FindByPredicate([Id](const auto& Row){return Row.InstanceId==Id;});
	TestEqual(TEXT("Old rate applies before completion"),S->Stored,int64(2));
	TestEqual(TEXT("Partial cycle conserved"),S->RemainderSeconds,30);
	TestEqual(TEXT("New rate starts at completion, including partial work"),DYKCityEconomy::View(P,Id,ProductionTime+120).Stored,int64(6));
	TestFalse(TEXT("Duplicate completion rejected"),DYKCityEconomy::CompleteUpgrade(P,Id,2,ProductionTime+120));
	TestFalse(TEXT("Backwards completion rejected"),DYKCityEconomy::CompleteUpgrade(P,Id,3,ProductionTime+89));
	auto* Save=NewObject<UDYKProfileSaveGame>(); Save->Profile=P;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Serialize"),UGameplayStatics::SaveGameToMemory(Save,Bytes))) return false;
	auto* Reload=Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Reload"),Reload)) return false;
	TestEqual(TEXT("Reload accrues at correct level from persisted boundary"),DYKCityEconomy::View(Reload->Profile,Id,ProductionTime+180).Stored,int64(10));
	// A future configuration can lower storage; existing stock must survive.
	P.CityEconomy.Instances[S-P.CityEconomy.Instances.GetData()].Stored=5000;
	TestTrue(TEXT("Retained above-cap stock is valid"),DYKCityEconomy::Validate(P));
	TestEqual(TEXT("Above-cap stock never destroyed or increased"),DYKCityEconomy::View(P,Id,MAX_int64).Stored,int64(5000));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKWarehouseLevelsTest,"DYK.M205.PerResourceWarehouseLevels",ProductionFlags)
bool FDYKWarehouseLevelsTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Initialize"),DYKCityEconomy::Initialize(P,ProductionTime))) return false;
	const FDYKResourceId ProductionWood(TEXT("RES-WOOD")), ProductionFood(TEXT("RES-FOOD")), ProductionGold(TEXT("RES-GOLD"));
	FDYKBuildingSaveData B; B.BuildingId=FDYKBuildingId(TEXT("CITY-WAREHOUSE")).ToContentId(); B.Level=2;
	FDYKCityProductionSaveData S; S.InstanceId=FGuid::NewGuid(); S.BuildingIndex=P.Buildings.Add(B);
	S.UpdatedAtUnix=S.CollectionStartedAtUnix=ProductionTime; P.CityEconomy.Instances.Add(S);
	TestTrue(TEXT("Warehouse ownership valid"),DYKCityEconomy::Validate(P));
	TestEqual(TEXT("ProductionWood capacity level 2"),DYKCityEconomy::Capacity(P,ProductionWood),int64(400));
	TestEqual(TEXT("ProductionFood has independent capacity"),DYKCityEconomy::Capacity(P,ProductionFood),int64(300));
	TestEqual(TEXT("ProductionGold retains uint32 technical bound"),DYKCityEconomy::Capacity(P,ProductionGold),int64(MAX_uint32));
	for (auto& R:P.Resources) if(R.ResourceId==ProductionWood.ToContentId()) R.Amount=370;
	const auto V=DYKCityEconomy::View(P,ProductionGuid(P,ProductionIndex(P,TEXT("CITY-SAWMILL"))),ProductionTime+3000);
	TestEqual(TEXT("Partial collection respects upgraded free space"),V.Collectable,int64(30));
	for (auto& R:P.Resources) if(R.ResourceId==ProductionWood.ToContentId()) R.Amount=401;
	TestFalse(TEXT("Warehouse overflow rejected"),DYKCityEconomy::Validate(P));
	return true;
}
#endif
