#include "../Meta/DYKCityBuildingRules.h"
#include "../Meta/DYKCityCatalog.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Meta/DYKCityTerritory.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr auto BuildingRulesTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int64 BuildingRulesTestTime = 1794700000;
	FDYKBuildingId RulesId(const TCHAR* Name) { return FDYKBuildingId(Name); }
	int32 RulesIndex(const FDYKPlayerProfileSaveData& P, const TCHAR* Name)
	{
		return P.Buildings.IndexOfByPredicate([Name](const auto& B) { return B.BuildingId.Name == FName(Name); });
	}
	FGuid RulesGuid(const FDYKPlayerProfileSaveData& P, int32 Index)
	{
		const auto* S = P.CityEconomy.Instances.FindByPredicate([Index](const auto& Row) { return Row.BuildingIndex == Index; });
		return S ? S->InstanceId : FGuid();
	}
	bool RulesSeed(FDYKPlayerProfileSaveData& P)
	{
		return DYKCityEconomy::Initialize(P, BuildingRulesTestTime)
			&& DYKCityTerritory::Initialize(P, DYKCityTerritory::DEV_PLACEHOLDER());
	}
	bool RulesSame(const FDYKPlayerProfileSaveData& A, const FDYKPlayerProfileSaveData& B)
	{
		return FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}
	// Test fixtures represent completed ownership; no queue execution is simulated.
	int32 RulesAddOwned(FDYKPlayerProfileSaveData& P, const TCHAR* Name, FIntPoint Origin, int32 Level = 1)
	{
		FDYKBuildingSaveData B;
		B.BuildingId = RulesId(Name).ToContentId(); B.CellOrigin = Origin; B.Level = Level;
		const int32 Index = P.Buildings.Add(B);
		FDYKCityProductionSaveData S;
		S.InstanceId = FGuid::NewGuid(); S.BuildingIndex = Index;
		S.UpdatedAtUnix = S.CollectionStartedAtUnix = BuildingRulesTestTime;
		P.CityEconomy.Instances.Add(S);
		return Index;
	}
	FDYKCityBuildingWorkRequest RulesBuild(const TCHAR* Name, FIntPoint Origin = {27, 25})
	{
		FDYKCityBuildingWorkRequest R;
		R.CommandId = FGuid::NewGuid(); R.InstanceId = FGuid::NewGuid(); R.BuildingId = RulesId(Name); R.Origin = Origin;
		return R;
	}
	FDYKCityBuildingWorkRequest RulesUpgrade(const FDYKPlayerProfileSaveData& P, const TCHAR* Name)
	{
		const int32 Index = RulesIndex(P, Name);
		FDYKCityBuildingWorkRequest R;
		R.CommandId = FGuid::NewGuid(); R.Kind = EDYKCityBuildingWorkKind::Upgrade;
		R.BuildingId = RulesId(Name); R.InstanceId = RulesGuid(P, Index);
		R.ExpectedLevel = P.Buildings[Index].Level;
		R.Origin = P.Buildings[Index].CellOrigin; R.QuarterTurns = P.Buildings[Index].RotationQuarterTurns;
		return R;
	}
	void RulesFund(FDYKPlayerProfileSaveData& P)
	{
		for (auto& Resource : P.Resources)
			Resource.Amount = DYKCityCatalog::ResourceCapacity(FDYKResourceId(Resource.ResourceId.Name));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBuildingOwnershipRulesTest, "DYK.M202.CatalogAndOwnership", BuildingRulesTestFlags)
bool FDYKBuildingOwnershipRulesTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Starter city initialized"), RulesSeed(P))) return false;
	const auto& C = DYKCityBuildingRules::DEV_PLACEHOLDER();
	const auto& T = DYKCityTerritory::DEV_PLACEHOLDER();
	TestEqual(TEXT("Only approved five starter buildings granted"), P.Buildings.Num(), 5);
	TestTrue(TEXT("Optional catalog buildings need not be owned"), DYKCityBuildingRules::ValidateOwnership(P, C));
	const int32 CastleIndex = RulesIndex(P, TEXT("CITY-CASTLE"));
	TArray<FIntPoint> CastleCells;
	TestTrue(TEXT("Castle occupied cells available"), DYKCityTerritory::OccupiedCells(P.Buildings[CastleIndex], T, CastleCells));
	TestEqual(TEXT("Approved castle footprint covers entire parcel"), CastleCells.Num(), 100);
	for (int32 Y = 0; Y < 10; ++Y) for (int32 X = 0; X < 10; ++X)
		TestTrue(TEXT("Every castle parcel cell occupied"), CastleCells.Contains(P.Buildings[CastleIndex].CellOrigin + FIntPoint(X,Y)));
	for (const auto& Def : DYKCityCatalog::Buildings())
	{
		TestTrue(TEXT("Catalog ID valid"), Def.Id.IsValid());
		TestTrue(TEXT("Every catalog type has progression"), C.Buildings.Contains(Def.Id.GetName()));
		TestTrue(TEXT("Every catalog type has placement shape"), T.Footprints.Contains(Def.Id.GetName()));
	}
	auto Invalid = P;
	Invalid.Buildings[1].BuildingId = RulesId(TEXT("CITY-UNKNOWN")).ToContentId();
	TestFalse(TEXT("Unknown owned type rejected"), DYKCityBuildingRules::ValidateOwnership(Invalid, C));
	Invalid = P; Invalid.Buildings[1].Level = 0;
	TestFalse(TEXT("Zero completed level rejected"), DYKCityBuildingRules::ValidateOwnership(Invalid, C));
	Invalid = P; Invalid.Buildings[1].Level = MAX_int32;
	TestFalse(TEXT("Level beyond configured type maximum rejected"), DYKCityBuildingRules::ValidateOwnership(Invalid, C));
	Invalid = P; RulesAddOwned(Invalid, TEXT("CITY-CASTLE"), {30,20});
	TestFalse(TEXT("Second castle cannot exist"), DYKCityBuildingRules::ValidateOwnership(Invalid, C));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBuildingPlacementRulesTest, "DYK.M202.P12T08P14T10.BuildPlacementAndLimits", BuildingRulesTestFlags)
bool FDYKBuildingPlacementRulesTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Starter city initialized"), RulesSeed(P))) return false;
	const auto& C = DYKCityBuildingRules::DEV_PLACEHOLDER();
	const auto& T = DYKCityTerritory::DEV_PLACEHOLDER();
	FDYKCityBuildingWorkPlan Plan;
	const auto Before = P;
	TestTrue(TEXT("Second house request allowed at castle level one"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-HOUSE")), T, C, Plan));
	TestTrue(TEXT("Preparation leaves profile entirely unchanged"), RulesSame(P, Before));
	TestTrue(TEXT("Building request requires nonzero duration"), Plan.DurationSeconds > 0);
	TestEqual(TEXT("Building target begins at level one"), Plan.TargetLevel, 1);
	RulesAddOwned(P, TEXT("CITY-HOUSE"), {27,25});
	TestTrue(TEXT("Third house request allowed at castle level one"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-HOUSE"), {30,25}), T, C, Plan));
	RulesAddOwned(P, TEXT("CITY-HOUSE"), {30,25});
	const auto ThreeHouses = P;
	TestFalse(TEXT("Fourth house request rejected"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-HOUSE"), {34,25}), T, C, Plan));
	TestTrue(TEXT("Rejected request preserves all ownership/resources/progress"), RulesSame(P, ThreeHouses));
	auto Inflated = C; Inflated.Buildings.FindChecked(TEXT("CITY-HOUSE")).LimitsByCastleLevel[0] = 100;
	TestFalse(TEXT("Placeholder data cannot override approved level-one three-house limit"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-HOUSE"), {34,25}), T, Inflated, Plan));
	P = Before;
	TestTrue(TEXT("First optional unique building allowed without buying other optional types"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-BARRACKS")), T, C, Plan));
	RulesAddOwned(P, TEXT("CITY-BARRACKS"), {27,25});
	const auto UniqueBefore = P;
	TestFalse(TEXT("Second unique functional type rejected"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-BARRACKS"), {34,25}), T, C, Plan));
	TestTrue(TEXT("Duplicate unique refusal creates no inventory entry"), RulesSame(P, UniqueBefore));
	P = Before;
	auto ProductionConfig = C;
	ProductionConfig.Buildings.FindChecked(TEXT("CITY-SAWMILL")).LimitsByCastleLevel = {2,3,3,3,3};
	TestTrue(TEXT("Production duplicates allowed by castle-level table"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-SAWMILL")), T, ProductionConfig, Plan));
	RulesAddOwned(P, TEXT("CITY-SAWMILL"), {27,25});
	TestFalse(TEXT("Production duplicate cannot exceed current castle-level limit"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-SAWMILL"), {34,25}), T, ProductionConfig, Plan));
	P.Buildings[RulesIndex(P,TEXT("CITY-CASTLE"))].Level = 2;
	TestTrue(TEXT("Completed castle level unlocks configured extra producer"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-SAWMILL"), {34,25}), T, ProductionConfig, Plan));
	P = Before;
	for (const auto Origin : {FIntPoint(10,10), FIntPoint(0,0), FIntPoint(-1,20), FIntPoint(49,49), FIntPoint(MAX_int32,20)})
		TestFalse(TEXT("Overlap, locked land and out-of-range footprints rejected"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-HOUSE"), Origin), T, C, Plan));
	for (const int32 Turn : {-1,4,MAX_int32})
	{
		auto R = RulesBuild(TEXT("CITY-HOUSE")); R.QuarterTurns = Turn;
		TestFalse(TEXT("Invalid rotation rejected"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	}
	TestFalse(TEXT("Unknown requested type rejected"), DYKCityBuildingRules::Prepare(P, RulesBuild(TEXT("CITY-UNKNOWN")), T, C, Plan));
	auto R = RulesBuild(TEXT("CITY-HOUSE")); R.CommandId.Invalidate();
	TestFalse(TEXT("Invalid command identity rejected"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	R = RulesBuild(TEXT("CITY-HOUSE")); R.InstanceId = RulesGuid(P,RulesIndex(P,TEXT("CITY-HOUSE")));
	TestFalse(TEXT("Build cannot reuse owned instance identity"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	TestTrue(TEXT("All validation queries leave seeded profile unchanged"), RulesSame(P, Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBuildingQueueContractRulesTest, "DYK.M202.UpgradeAndQ40Contracts", BuildingRulesTestFlags)
bool FDYKBuildingQueueContractRulesTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Starter city initialized"), RulesSeed(P))) return false;
	const auto& T = DYKCityTerritory::DEV_PLACEHOLDER();
	auto C = DYKCityBuildingRules::DEV_PLACEHOLDER();
	FDYKCityBuildingWorkPlan Plan;
	auto R = RulesBuild(TEXT("CITY-HOUSE"));
	auto& FirstHouse = C.Buildings.FindChecked(TEXT("CITY-HOUSE")).Levels[0];
	FirstHouse.Costs = {{{EDYKContentKind::Resource,TEXT("RES-WOOD")},7}};
	FirstHouse.DurationSeconds = 17;
	const auto Empty = P;
	TestTrue(TEXT("Unfunded request may wait in queue without payment"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	TestFalse(TEXT("Actual start requires sufficient resources"), DYKCityBuildingRules::CanStart(P,R,T,C,Plan));
	TestTrue(TEXT("Queue preparation neither charges nor constructs"), RulesSame(P,Empty));
	RulesFund(P); const auto Funded = P;
	TestTrue(TEXT("Funded start plan accepted"), DYKCityBuildingRules::CanStart(P,R,T,C,Plan));
	TestEqual(TEXT("Plan duration comes from trusted replaceable type data"), Plan.DurationSeconds, int64(17));
	TestEqual(TEXT("Plan quotes full configured costs"), Plan.Costs[0].Amount, int64(7));
	TestTrue(TEXT("Start planning also leaves state unchanged"), RulesSame(P,Funded));
	FirstHouse.Conditions = {TEXT("OnlyAfterTwoConditions")};
	TestFalse(TEXT("Unknown condition denies request"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	C.Conditions.Add(TEXT("OnlyAfterTwoConditions"), [](const auto&){return false;});
	TestFalse(TEXT("Unmet configured condition denies request"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	C.Conditions[TEXT("OnlyAfterTwoConditions")] = [](const auto&){return true;};
	TestTrue(TEXT("Fulfilled configured condition allows request"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	FirstHouse.Conditions.Add(TEXT("SecondCondition"));
	C.Conditions.Add(TEXT("SecondCondition"), [](const auto&){return false;});
	TestFalse(TEXT("All conditions must pass"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	FirstHouse.Conditions.Reset(); FirstHouse.DurationSeconds = 0;
	TestFalse(TEXT("Zero duration cannot bypass Q40"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	FirstHouse.DurationSeconds = -1;
	TestFalse(TEXT("Negative duration rejected"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	FirstHouse.DurationSeconds = 17; FirstHouse.Costs[0].Amount = -7;
	TestFalse(TEXT("Negative price cannot create resources"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	TestEqual(TEXT("Rejected plan clears previously quoted duration"), Plan.DurationSeconds, int64(0));
	TestTrue(TEXT("Rejected plan clears previously quoted price"), Plan.Costs.IsEmpty());
	FirstHouse.Costs[0].Amount = 7;
	const auto DuplicateCost = FirstHouse.Costs[0]; FirstHouse.Costs.Add(DuplicateCost);
	TestFalse(TEXT("Duplicate resource costs rejected rather than undercharged"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	FirstHouse.Costs.SetNum(1); FirstHouse.Costs[0].ResourceId = {EDYKContentKind::Building,TEXT("RES-WOOD")};
	TestFalse(TEXT("Cost resource category validated"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	C = DYKCityBuildingRules::DEV_PLACEHOLDER();
	C.Buildings.FindChecked(TEXT("CITY-HOUSE")).Levels[1].RequiredCastleLevel = 1;
	R = RulesUpgrade(P,TEXT("CITY-HOUSE"));
	const auto UpgradeBefore = P;
	TestTrue(TEXT("Next house level plan accepted"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	TestEqual(TEXT("Only next sequential level targeted"), Plan.TargetLevel, 2);
	TestTrue(TEXT("Upgrade never changes level/footprint/timestamps at preparation"), RulesSame(P,UpgradeBefore));
	TArray<FIntPoint> OriginalCells, LaterCells;
	const int32 HouseIndex = RulesIndex(P,TEXT("CITY-HOUSE"));
	TestTrue(TEXT("Original footprint resolves"), DYKCityTerritory::OccupiedCells(P.Buildings[HouseIndex],T,OriginalCells));
	auto LeveledBuilding = P.Buildings[HouseIndex]; LeveledBuilding.Level = Plan.TargetLevel;
	TestTrue(TEXT("Upgraded footprint resolves"), DYKCityTerritory::OccupiedCells(LeveledBuilding,T,LaterCells));
	TestTrue(TEXT("Upgrade footprint unchanged at next completed level"), OriginalCells == LaterCells);
	auto BadR = R; BadR.ExpectedLevel = 0;
	TestFalse(TEXT("Stale expected level rejected"), DYKCityBuildingRules::Prepare(P,BadR,T,C,Plan));
	BadR = R; BadR.InstanceId = FGuid::NewGuid();
	TestFalse(TEXT("Unknown upgrade instance rejected"), DYKCityBuildingRules::Prepare(P,BadR,T,C,Plan));
	BadR = R; BadR.BuildingId = RulesId(TEXT("CITY-MILL"));
	TestFalse(TEXT("Upgrade identity/type mismatch rejected"), DYKCityBuildingRules::Prepare(P,BadR,T,C,Plan));
	BadR = R; ++BadR.Origin.X;
	TestTrue(TEXT("Stale spatial upgrade fields normalized"), DYKCityBuildingRules::Prepare(P,BadR,T,C,Plan));
	TestTrue(TEXT("Upgrade plan retains current saved origin"), Plan.Request.Origin == P.Buildings[HouseIndex].CellOrigin);
	BadR = R; BadR.QuarterTurns = (BadR.QuarterTurns + 1) % 4;
	TestTrue(TEXT("Stale rotation accepted without rotating instance"), DYKCityBuildingRules::Prepare(P,BadR,T,C,Plan));
	TestEqual(TEXT("Upgrade plan retains current saved rotation"), Plan.Request.QuarterTurns, P.Buildings[HouseIndex].RotationQuarterTurns);
	P.Buildings[HouseIndex].CellOrigin += FIntPoint(3,1);
	const auto Relocated = P;
	TestTrue(TEXT("Queued upgrade survives legal relocation and aliased plan request"), DYKCityBuildingRules::CanStart(P,Plan.Request,T,C,Plan));
	TestTrue(TEXT("Start plan follows actual relocated origin"), Plan.Request.Origin == P.Buildings[HouseIndex].CellOrigin);
	TestTrue(TEXT("Aliased start planning does not mutate relocated ownership"), RulesSame(P,Relocated));
	P = UpgradeBefore;
	P.Buildings[HouseIndex].ConstructionStartedAtUnix = BuildingRulesTestTime;
	P.Buildings[HouseIndex].ConstructionCompletesAtUnix = BuildingRulesTestTime + 17;
	TestFalse(TEXT("Existing timed work blocks overlapping upgrade"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	P.Buildings[HouseIndex].ConstructionStartedAtUnix = P.Buildings[HouseIndex].ConstructionCompletesAtUnix = 0;
	C.Buildings.FindChecked(TEXT("CITY-HOUSE")).Levels[1].RequiredCastleLevel = 2;
	TestFalse(TEXT("Upgrade prerequisite castle level enforced"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	P.Buildings[RulesIndex(P,TEXT("CITY-CASTLE"))].Level = 2;
	TestTrue(TEXT("Upgrade available after actual castle requirement fulfilled"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	P.Buildings[HouseIndex].Level = C.Buildings.FindChecked(TEXT("CITY-HOUSE")).Levels.Num();
	R = RulesUpgrade(P,TEXT("CITY-HOUSE"));
	TestFalse(TEXT("Type maximum cannot be exceeded"), DYKCityBuildingRules::Prepare(P,R,T,C,Plan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKBuildingSaleContractRulesTest, "DYK.M202.DecorationSaleContracts", BuildingRulesTestFlags)
bool FDYKBuildingSaleContractRulesTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Starter city initialized"), RulesSeed(P))) return false;
	TArray<FDYKResourceSaveData> Refund;
	for (const auto& State : P.CityEconomy.Instances)
	{
		TestFalse(TEXT("Functional buildings including castle cannot be sold"), DYKCityBuildingRules::CanSell(P,State.InstanceId));
		TestFalse(TEXT("Functional buildings have no sale refund plan"), DYKCityBuildingRules::PrepareSale(P,State.InstanceId,Refund));
	}
	const int32 DecorIndex = RulesAddOwned(P,TEXT("CITY-DECOR-DEV_PLACEHOLDER"),{27,25});
	P.Buildings[DecorIndex].PaidCosts = {{{EDYKContentKind::Resource,TEXT("RES-WOOD")},13}, {{EDYKContentKind::Resource,TEXT("RES-STONE")},5}};
	const auto Before = P;
	const FGuid Id = RulesGuid(P,DecorIndex);
	TestTrue(TEXT("Owned decorative type is sellable"), DYKCityBuildingRules::CanSell(P,Id));
	TestTrue(TEXT("Decoration sale quotes refund"), DYKCityBuildingRules::PrepareSale(P,Id,Refund));
	TestEqual(TEXT("Each actually paid resource returned"), Refund.Num(),2);
	if (Refund.Num() == 2)
	{
		TestTrue(TEXT("Refund wood ID retained"), Refund[0].ResourceId == P.Buildings[DecorIndex].PaidCosts[0].ResourceId);
		TestEqual(TEXT("Refund exactly 100 percent of paid wood"), Refund[0].Amount,int64(13));
		TestEqual(TEXT("Refund exactly 100 percent of paid stone"), Refund[1].Amount,int64(5));
	}
	TestTrue(TEXT("Sale plan neither destroys ownership nor refunds early"), RulesSame(P,Before));
	auto* Save = NewObject<UDYKProfileSaveGame>();
	Save->Profile = P;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Paid expenditure serializes through production SaveGame format"), UGameplayStatics::SaveGameToMemory(Save,Bytes));
	auto* Reloaded = Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (TestNotNull(TEXT("SaveGame memory roundtrip reloads profile"),Reloaded))
	{
		TestTrue(TEXT("PaidCosts and complete profile survive roundtrip"), RulesSame(Reloaded->Profile,P));
		TArray<FDYKResourceSaveData> ReloadedRefund;
		TestTrue(TEXT("Reloaded decoration retains sale entitlement"), DYKCityBuildingRules::PrepareSale(Reloaded->Profile,Id,ReloadedRefund));
		TestEqual(TEXT("Reloaded actual expenditure has both resources"),ReloadedRefund.Num(),2);
		if (ReloadedRefund.Num() == 2)
		{
			TestEqual(TEXT("Reloaded wood refund remains exactly actual payment"),ReloadedRefund[0].Amount,int64(13));
			TestEqual(TEXT("Reloaded stone refund remains exactly actual payment"),ReloadedRefund[1].Amount,int64(5));
		}
	}
	TestFalse(TEXT("Unknown instance cannot be sold"), DYKCityBuildingRules::PrepareSale(P,FGuid::NewGuid(),Refund));
	P.Buildings[DecorIndex].PaidCosts[0].Amount = -1;
	TestFalse(TEXT("Corrupt negative paid price rejects refund"), DYKCityBuildingRules::PrepareSale(P,Id,Refund));
	return true;
}
#endif
