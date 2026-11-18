#include "../Meta/DYKCityEconomy.h"
#include "../Saving/DYKSaveSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "../Meta/DYKCityBuildingActor.h"
#include "../Meta/DYKCityWidgets.h"
#include "../Meta/DYKCityWorkWidget.h"
#include "Components/Button.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Input/Events.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SWidget.h"

struct FDYKCityEconomyTestAccess
{
	static void Open(UDYKSaveSubsystem& Service, const FString& Prefix)
	{
		Service.SlotPrefix = Prefix;
		Service.LoadOrCreateProfile();
	}
	static void SetPrefix(UDYKSaveSubsystem& Service, const FString& Prefix) { Service.SlotPrefix = Prefix; }
};

namespace
{
	constexpr auto CityTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int64 Started = 1794700000;
	const FDYKResourceId Wood(TEXT("RES-WOOD"));
	const FDYKResourceId Food(TEXT("RES-FOOD"));
	const FDYKResourceId Gold(TEXT("RES-GOLD"));

	FDYKContentId BuildingId(const TCHAR* Name) { return {EDYKContentKind::Building, FName(Name)}; }
	FGuid Instance(const FDYKPlayerProfileSaveData& P, const TCHAR* Name)
	{
		for (const auto& S : P.CityEconomy.Instances)
		{
			if (P.Buildings.IsValidIndex(S.BuildingIndex) && P.Buildings[S.BuildingIndex].BuildingId == BuildingId(Name))
			{
				return S.InstanceId;
			}
		}
		return {};
	}
	FDYKResourceSaveData& Resource(FDYKPlayerProfileSaveData& P, FDYKResourceId Id)
	{
		return *P.Resources.FindByPredicate([Id](const auto& R) { return R.ResourceId == Id.ToContentId(); });
	}
	bool SameCityProfile(const FDYKPlayerProfileSaveData* Actual, const FDYKPlayerProfileSaveData& Expected)
	{
		return Actual && FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(Actual, &Expected, 0);
	}
	struct FCityStorage
	{
		TStrongObjectPtr<UGameInstance> Owner{NewObject<UGameInstance>()};
		FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),
			TEXT("M105/test-saves"), FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		FString Prefix = FPaths::Combine(Directory, TEXT("profile"));
		~FCityStorage() { IFileManager::Get().DeleteDirectory(*Directory, false, true); }
		UDYKSaveSubsystem* Open() const
		{
			auto* Service = NewObject<UDYKSaveSubsystem>(Owner.Get());
			FDYKCityEconomyTestAccess::Open(*Service, Prefix);
			return Service;
		}
		bool Seed(UDYKSaveSubsystem& Service) const
		{
			return Service.ApplyProfileCommand(FGuid::NewGuid(), [](auto& P) { return DYKCityEconomy::Initialize(P, Started); });
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityStarterTest, "DYK.M105.P13T01.StarterAndMigration", CityTestFlags)
bool FDYKCityStarterTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P;
	TestFalse(TEXT("Invalid initial timestamp rejected"), DYKCityEconomy::Initialize(P, 0));
	if (!TestTrue(TEXT("New city initialized"), DYKCityEconomy::Initialize(P, Started))) return false;
	TestEqual(TEXT("Exactly five starter buildings"), P.Buildings.Num(), 5);
	TestEqual(TEXT("One independent state for each starter"), P.CityEconomy.Instances.Num(), 5);
	TestEqual(TEXT("Four HUD resource balances"), P.Resources.Num(), 4);
	for (const auto& Def : DYKCityCatalog::Buildings())
	{
		if (!Def.bInitiallyOwned) continue;
		TestEqual(TEXT("One starter of each specified type"), P.Buildings.FilterByPredicate(
			[&Def](const auto& B) { return B.BuildingId == Def.Id.ToContentId(); }).Num(), 1);
	}
	for (const auto& Id : DYKCityCatalog::Resources()) TestEqual(TEXT("Starts with zero resources"), DYKCityEconomy::Balance(P, Id), int64(0));
	const auto Before = P;
	TestTrue(TEXT("Repeated migration accepted"), DYKCityEconomy::Initialize(P, Started + 1000));
	TestTrue(TEXT("Repeated migration preserves IDs/times/balances exactly"), SameCityProfile(&P, Before));

	FDYKPlayerProfileSaveData Old;
	FDYKBuildingSaveData Sawmill;
	Sawmill.BuildingId = BuildingId(TEXT("CITY-SAWMILL"));
	Sawmill.CellOrigin = FIntPoint(17, -4);
	Old.Buildings.Add(Sawmill);
	Old.Resources.Add({Wood.ToContentId(), 37});
	Old.LastDailyRewardDateKey = 20261114;
	TestTrue(TEXT("Existing city adapted"), DYKCityEconomy::Initialize(Old, Started));
	TestEqual(TEXT("Existing building not duplicated"), Old.Buildings.Num(), 5);
	TestTrue(TEXT("Existing layout preserved"), Old.Buildings[0].CellOrigin == FIntPoint(17, -4));
	TestEqual(TEXT("Existing balance preserved"), DYKCityEconomy::Balance(Old, Wood), int64(37));
	TestEqual(TEXT("Unrelated progress preserved"), Old.LastDailyRewardDateKey, 20261114);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityAccrualTest, "DYK.M105.P13T02T04T12.IndependentAccrualAndCaps", CityTestFlags)
bool FDYKCityAccrualTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Initialize"), DYKCityEconomy::Initialize(P, Started))) return false;
	const auto Before = P;
	for (const auto& Def : DYKCityCatalog::Buildings())
	{
		if (!Def.Resource.IsValid()) continue;
		const auto Id = Instance(P, *Def.Id.GetName().ToString());
		const auto Early = DYKCityEconomy::View(P, Id, Started + 59);
		TestEqual(TEXT("No collection before 60 seconds"), Early.Collectable, int64(0));
		const auto Complete = DYKCityEconomy::View(P, Id, Started + Def.CycleSeconds);
		TestEqual(TEXT("Own production cycle amount"), Complete.Stored, int64(Def.UnitsPerCycle));
		TestTrue(TEXT("Completed eligible cycle can collect"), Complete.Collectable > 0);
		const auto FarFuture = DYKCityEconomy::View(P, Id, MAX_int64);
		TestEqual(TEXT("Long offline time bounded by own storage"), FarFuture.Stored, int64(Def.UnitsPerCycle) * Def.StorageCycles);
		if (Def.Resource == Gold)
		{
			TestEqual(TEXT("House stores exactly one gold cycle"), FarFuture.Stored, int64(1));
		}
		else
		{
			TestEqual(TEXT("Level one storage is twelve hours"), Def.CycleSeconds * Def.StorageCycles, 12 * 60 * 60);
		}
		TestEqual(TEXT("Backward local time cannot produce resources"), DYKCityEconomy::View(P, Id, Started - 60).Stored, int64(0));
	}
	TestTrue(TEXT("Projection never collects or changes persistent state"), SameCityProfile(&P, Before));
	TestEqual(TEXT("Unknown instance is unavailable"), DYKCityEconomy::View(P, FGuid::NewGuid(), Started + 60).Collectable, int64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityCollectionTest, "DYK.M105.P13T03T07T09.PartialCollectionAndPersistence", CityTestFlags)
bool FDYKCityCollectionTest::RunTest(const FString& Parameters)
{
	FCityStorage Storage;
	auto* Service = Storage.Open();
	if (!TestTrue(TEXT("Seed"), Storage.Seed(*Service))) return false;
	const auto Id = Instance(*Service->GetProfileSnapshot(), TEXT("CITY-SAWMILL"));
	const int64 InitialRevision = Service->GetRevision();
	TestFalse(TEXT("Early collect fails"), DYKCityEconomy::Collect(*Service, FGuid::NewGuid(), Id, Started + 59));
	TestEqual(TEXT("Early collect never commits"), Service->GetRevision(), InitialRevision);
	TestTrue(TEXT("Seed near-full wood warehouse"), Service->ApplyProfileCommand(FGuid::NewGuid(), [](auto& P) { Resource(P, Wood).Amount = 70; return true; }));
	const auto PreCollection = DYKCityEconomy::View(*Service->GetProfileSnapshot(), Id, Started + 3600);
	TestTrue(TEXT("Enough production to exercise a partial transfer"), PreCollection.Stored > 30);
	TestEqual(TEXT("Only free warehouse space collectable"), PreCollection.Collectable, int64(30));
	const FGuid Command = FGuid::NewGuid();
	TestTrue(TEXT("Partial collection commits"), DYKCityEconomy::Collect(*Service, Command, Id, Started + 3600));
	TestEqual(TEXT("Wood reaches exact capacity"), DYKCityEconomy::Balance(*Service->GetProfileSnapshot(), Wood), int64(100));
	const auto AfterCollection = *Service->GetProfileSnapshot();
	TestEqual(TEXT("Uncollected wood remains in building"), DYKCityEconomy::View(AfterCollection, Id, Started + 3600).Stored, PreCollection.Stored - 30);
	TestEqual(TEXT("Full warehouse disables collection"), DYKCityEconomy::View(AfterCollection, Id, Started + 3600).Collectable, int64(0));
	TestFalse(TEXT("Full warehouse collect rejected"), DYKCityEconomy::Collect(*Service, FGuid::NewGuid(), Id, Started + 3600));
	TestTrue(TEXT("Full warehouse does not stop production"), DYKCityEconomy::View(AfterCollection, Id, Started + 7200).Stored > PreCollection.Stored - 30);
	const auto Mill = Instance(AfterCollection, TEXT("CITY-MILL"));
	TestTrue(TEXT("Another resource can still collect"), DYKCityEconomy::View(AfterCollection, Mill, Started + 3600).Collectable > 0);
	Service = Storage.Open();
	TestTrue(TEXT("All city state survives reload"), SameCityProfile(Service->GetProfileSnapshot(), AfterCollection));
	TestTrue(TEXT("Repeated command after reload is acknowledged"), DYKCityEconomy::Collect(*Service, Command, Id, Started + 7200));
	TestTrue(TEXT("Repeated command cannot change accumulated state"), SameCityProfile(Service->GetProfileSnapshot(), AfterCollection));
	TestTrue(TEXT("Spend creates room"), DYKCityEconomy::Spend(*Service, FGuid::NewGuid(), Wood, 10));
	TestTrue(TEXT("Remaining stored resources immediately collectable"), DYKCityEconomy::Collect(*Service, FGuid::NewGuid(), Id, Started + 3600));
	TestEqual(TEXT("Partial remainder conserved after another transfer"), DYKCityEconomy::View(*Service->GetProfileSnapshot(), Id, Started + 3600).Stored, PreCollection.Stored - 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCitySafetyTest, "DYK.M105.NonnegativeBalancesAndGoldRepresentation", CityTestFlags)
bool FDYKCitySafetyTest::RunTest(const FString& Parameters)
{
	FCityStorage Storage;
	auto* Service = Storage.Open();
	if (!TestTrue(TEXT("Seed"), Storage.Seed(*Service))) return false;
	const auto Before = *Service->GetProfileSnapshot();
	for (int64 Amount : {int64(-1), int64(0), int64(1), MAX_int64})
	{
		TestFalse(TEXT("Invalid or unaffordable spend fails"), DYKCityEconomy::Spend(*Service, FGuid::NewGuid(), Wood, Amount));
		TestTrue(TEXT("Failed spend has no state changes"), SameCityProfile(Service->GetProfileSnapshot(), Before));
	}
	TestFalse(TEXT("Unknown resource rejected"), DYKCityEconomy::Spend(*Service, FGuid::NewGuid(), FDYKResourceId(TEXT("NOT-A-RESOURCE")), 1));
	TestTrue(TEXT("Prepare max gold balance"), Service->ApplyProfileCommand(FGuid::NewGuid(), [](auto& P) { Resource(P, Gold).Amount = MAX_uint32; return true; }));
	const auto House = Instance(*Service->GetProfileSnapshot(), TEXT("CITY-HOUSE"));
	TestFalse(TEXT("Max uint32 gold cannot overflow"), DYKCityEconomy::Collect(*Service, FGuid::NewGuid(), House, Started + 1000));
	TestEqual(TEXT("Gold remains max uint32"), DYKCityEconomy::Balance(*Service->GetProfileSnapshot(), Gold), int64(MAX_uint32));
	TestEqual(TEXT("Uncollected gold stays in house"), DYKCityEconomy::View(*Service->GetProfileSnapshot(), House, Started + 1000).Stored, int64(1));
	TestTrue(TEXT("Spend one gold"), DYKCityEconomy::Spend(*Service, FGuid::NewGuid(), Gold, 1));
	TestTrue(TEXT("House can fill one representation slot"), DYKCityEconomy::Collect(*Service, FGuid::NewGuid(), House, Started + 1000));
	TestEqual(TEXT("Gold reaches max without overflow"), DYKCityEconomy::Balance(*Service->GetProfileSnapshot(), Gold), int64(MAX_uint32));
	TestEqual(TEXT("Complete collect empties house"), DYKCityEconomy::View(*Service->GetProfileSnapshot(), House, Started + 1000).Stored, int64(0));
	TestFalse(TEXT("Double-click with new command cannot duplicate house gold"), DYKCityEconomy::Collect(*Service, FGuid::NewGuid(), House, Started + 1000));
	const auto Sawmill = Instance(*Service->GetProfileSnapshot(), TEXT("CITY-SAWMILL"));
	TestTrue(TEXT("Collect complete basic production cycle"), DYKCityEconomy::Collect(*Service, FGuid::NewGuid(), Sawmill, Started + 60));
	TestEqual(TEXT("A completely emptied producer starts a new minute gate"), DYKCityEconomy::View(*Service->GetProfileSnapshot(), Sawmill, Started + 119).Collectable, int64(0));
	TestTrue(TEXT("Next minute becomes collectable"), DYKCityEconomy::View(*Service->GetProfileSnapshot(), Sawmill, Started + 120).Collectable > 0);
	const int64 RemainingWood = DYKCityEconomy::Balance(*Service->GetProfileSnapshot(), Wood);
	TestTrue(TEXT("Exact balance debit succeeds"), DYKCityEconomy::Spend(*Service, FGuid::NewGuid(), Wood, RemainingWood));
	TestEqual(TEXT("Exact debit reaches zero"), DYKCityEconomy::Balance(*Service->GetProfileSnapshot(), Wood), int64(0));
	TestFalse(TEXT("Another debit cannot cross zero"), DYKCityEconomy::Spend(*Service, FGuid::NewGuid(), Wood, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityRemainderTest, "DYK.M105.CollectionPreservesWorkedPartialCycle", CityTestFlags)
bool FDYKCityRemainderTest::RunTest(const FString& Parameters)
{
	FCityStorage Storage;
	auto* Service = Storage.Open();
	if (!TestTrue(TEXT("Seed"), Storage.Seed(*Service))) return false;
	const auto Id = Instance(*Service->GetProfileSnapshot(), TEXT("CITY-SAWMILL"));
	TestTrue(TEXT("Collect complete output after ninety seconds"), DYKCityEconomy::Collect(*Service, FGuid::NewGuid(), Id, Started + 90));
	const auto& States = Service->GetProfileSnapshot()->CityEconomy.Instances;
	const auto* S = States.FindByPredicate([Id](const auto& Item) { return Item.InstanceId == Id; });
	if (!TestNotNull(TEXT("Saved producer state retained"), S)) return false;
	TestEqual(TEXT("Whole produced units collected"), S->Stored, int64(0));
	TestEqual(TEXT("Thirty seconds already worked survive collection"), S->RemainderSeconds, 30);
	TestEqual(TEXT("New collection threshold starts after emptying"), S->CollectionStartedAtUnix, Started + 90);
	TestEqual(TEXT("New threshold still blocks at fifty-nine seconds"), DYKCityEconomy::View(*Service->GetProfileSnapshot(), Id, Started + 149).Collectable, int64(0));
	Service = Storage.Open();
	TestEqual(TEXT("After restart three total work minutes produce the full remainder"),
		DYKCityEconomy::View(*Service->GetProfileSnapshot(), Id, Started + 180).Stored, int64(4));
	TestTrue(TEXT("Threshold elapsed after restart"), DYKCityEconomy::View(*Service->GetProfileSnapshot(), Id, Started + 180).Collectable > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityFailureTest, "DYK.M105.WriteFailureConservesProductionAndRetry", CityTestFlags)
bool FDYKCityFailureTest::RunTest(const FString& Parameters)
{
	FCityStorage Storage;
	auto* Service = Storage.Open();
	if (!TestTrue(TEXT("Seed"), Storage.Seed(*Service))) return false;
	const auto Before = *Service->GetProfileSnapshot();
	const auto Id = Instance(Before, TEXT("CITY-SAWMILL"));
	const int64 Revision = Service->GetRevision();
	const FString Blocker = FPaths::Combine(Storage.Directory, TEXT("blocked-parent"));
	TestTrue(TEXT("Real filesystem write obstruction created"), FFileHelper::SaveStringToFile(TEXT("blocked"), *Blocker));
	FDYKCityEconomyTestAccess::SetPrefix(*Service, FPaths::Combine(Blocker, TEXT("profile")));
	const auto Event = FGuid::NewGuid();
	TestFalse(TEXT("Collect fails if save cannot commit"), DYKCityEconomy::Collect(*Service, Event, Id, Started + 60));
	TestTrue(TEXT("Failure keeps both balance and producer state"), SameCityProfile(Service->GetProfileSnapshot(), Before));
	TestEqual(TEXT("Failure keeps revision"), Service->GetRevision(), Revision);
	FDYKCityEconomyTestAccess::SetPrefix(*Service, Storage.Prefix);
	TestTrue(TEXT("Same collection ID retries after storage restored"), DYKCityEconomy::Collect(*Service, Event, Id, Started + 60));
	const auto Committed = *Service->GetProfileSnapshot();
	TestTrue(TEXT("Retry actually collects resources"), DYKCityEconomy::Balance(Committed, Wood) > 0);
	TestTrue(TEXT("Retry persisted exactly"), SameCityProfile(Storage.Open()->GetProfileSnapshot(), Committed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityCorruptStateTest, "DYK.M105.InvalidStateAndInstanceIsolation", CityTestFlags)
bool FDYKCityCorruptStateTest::RunTest(const FString& Parameters)
{
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Initialize"), DYKCityEconomy::Initialize(P, Started))) return false;
	const auto Original = P;
	const auto First = Instance(P, TEXT("CITY-SAWMILL"));
	FDYKBuildingSaveData Duplicate = P.Buildings[P.CityEconomy.Instances[1].BuildingIndex];
	P.Buildings.Add(Duplicate);
	auto Extra = P.CityEconomy.Instances[1];
	Extra.InstanceId = FGuid::NewGuid();
	Extra.BuildingIndex = P.Buildings.Num() - 1;
	Extra.Stored = 12;
	Extra.UpdatedAtUnix = Extra.CollectionStartedAtUnix = Started + 30;
	P.CityEconomy.Instances.Add(Extra);
	TestTrue(TEXT("Distinct duplicate producer state accepted"), DYKCityEconomy::Validate(P));
	TestEqual(TEXT("First producer starts at its own time"), DYKCityEconomy::View(P, First, Started + 60).Stored, int64(2));
	TestEqual(TEXT("Second producer retains independent stock and time"), DYKCityEconomy::View(P, Extra.InstanceId, Started + 60).Stored, int64(12));
	P.CityEconomy.Instances.Last().InstanceId = First;
	TestFalse(TEXT("Duplicate instance GUID rejected"), DYKCityEconomy::Validate(P));
	P = Original;
	Resource(P, Wood).Amount = -1;
	TestFalse(TEXT("Negative balance rejected"), DYKCityEconomy::Validate(P));
	P = Original;
	const auto DuplicateResource = P.Resources[0];
	P.Resources.Add(DuplicateResource);
	TestFalse(TEXT("Duplicate resource row rejected"), DYKCityEconomy::Validate(P));
	P = Original;
	P.CityEconomy.Instances[1].Stored = MAX_int64;
	TestFalse(TEXT("Overflowing production rejected"), DYKCityEconomy::Validate(P));
	P = Original;
	P.CityEconomy.Instances[1].RemainderSeconds = -1;
	TestFalse(TEXT("Negative partial-cycle time rejected"), DYKCityEconomy::Validate(P));
	for (const auto& Def : DYKCityCatalog::Buildings())
	{
		if (!Def.bInitiallyOwned) continue;
		P = Original;
		const int32 RemovedIndex = P.Buildings.IndexOfByPredicate([&Def](const auto& B) { return B.BuildingId == Def.Id.ToContentId(); });
		P.Buildings.RemoveAt(RemovedIndex);
		P.CityEconomy.Instances.RemoveAll([RemovedIndex](const auto& S) { return S.BuildingIndex == RemovedIndex; });
		for (auto& S : P.CityEconomy.Instances) { if (S.BuildingIndex > RemovedIndex) --S.BuildingIndex; }
		TestFalse(TEXT("Missing mandatory starter type rejected"), DYKCityEconomy::Validate(P));
	}
	P = Original;
	const auto HouseGuid = Instance(P, TEXT("CITY-HOUSE"));
	const auto HouseState = *P.CityEconomy.Instances.FindByPredicate([HouseGuid](const auto& S) { return S.InstanceId == HouseGuid; });
	const auto HouseBuilding = P.Buildings[HouseState.BuildingIndex];
	for (int32 HouseCount = 2; HouseCount <= 4; ++HouseCount)
	{
		P.Buildings.Add(HouseBuilding);
		auto State = HouseState;
		State.InstanceId = FGuid::NewGuid();
		State.BuildingIndex = P.Buildings.Num() - 1;
		P.CityEconomy.Instances.Add(State);
		if (HouseCount <= 3) TestTrue(TEXT("Up to three homes allowed with level one castle"), DYKCityEconomy::Validate(P));
		else TestFalse(TEXT("Fourth home rejected with level one castle"), DYKCityEconomy::Validate(P));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityRuntimeInputTest, "DYK.M105.RuntimeSlateButtonInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDYKCityRuntimeInputTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
	}
	if (!World)
	{
		AddInfo(TEXT("Runtime Slate scenario requires separate -game city invocation; no runtime assertion performed in the editor-only suite."));
		return true;
	}
	auto* Save = World->GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	if (!TestNotNull(TEXT("Runtime has profile service"), Save)) return false;
	if (!TestNotNull(TEXT("Runtime has profile snapshot"), Save->GetProfileSnapshot())) return false;
	const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
	TestTrue(TEXT("Prepare accrued production without collecting"), Save->ApplyProfileCommand(FGuid::NewGuid(), [Now](auto& P)
	{
		for (auto& S : P.CityEconomy.Instances)
		{
			S.UpdatedAtUnix = S.CollectionStartedAtUnix = Now - 60;
			S.Stored = P.Buildings[S.BuildingIndex].BuildingId.Name==TEXT("CITY-SAWMILL") ? 2 : 0;
			S.RemainderSeconds = 0;
		}
		Resource(P, Wood).Amount = 0;
		return true;
	}));
	UDYKCityWorkWidget* Widget=nullptr;
	for(TObjectIterator<UDYKCityWorkWidget> It;It;++It) if(It->GetWorld()==World && It->IsInViewport()) {Widget=*It; break;}
	if(!TestNotNull(TEXT("Actual building window"),Widget)) return false;
	Widget->CloseClicked(); Widget->OpenBuilding(Instance(*Save->GetProfileSnapshot(),TEXT("CITY-SAWMILL")),TEXT("CITY-SAWMILL"));
	auto* Button=Widget->FindAction(EDYKCityUIAction::Collect);
	if(!TestNotNull(TEXT("Native card collect button"),Button) || !TestTrue(TEXT("Completed output enables collection"),Button->GetIsEnabled())) return false;
	const auto SlateButton = Button->TakeWidget();
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(240.f, 40.f), FSlateLayoutTransform());
	const FKeyEvent Accept(EKeys::SpaceBar, FModifierKeysState(), 0, false, 0, 0);
	TestTrue(TEXT("Slate accepts key-down"), SlateButton->OnKeyDown(Geometry, Accept).IsEventHandled());
	TestTrue(TEXT("Slate key-up executes UButton delegate"), SlateButton->OnKeyUp(Geometry, Accept).IsEventHandled());
	const int64 Collected = DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), Wood);
	TestTrue(TEXT("Native Slate input collected actual resource"), Collected > 0);
	TestFalse(TEXT("Emptied producer disables the current card button"), Widget->FindAction(EDYKCityUIAction::Collect)->GetIsEnabled());
	SlateButton->OnKeyDown(Geometry, Accept);
	SlateButton->OnKeyUp(Geometry, Accept);
	TestEqual(TEXT("Repeated disabled input cannot collect twice"), DYKCityEconomy::Balance(*Save->GetProfileSnapshot(), Wood), Collected);
	return true;
}

#endif
