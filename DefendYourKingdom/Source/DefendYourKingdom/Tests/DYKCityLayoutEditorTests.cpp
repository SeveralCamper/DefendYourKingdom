#include "../Meta/DYKCityLayoutEditor.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Meta/DYKCityTerritory.h"
#include "../Meta/DYKCityBuildingRules.h"
#include "../Saving/DYKProfileService.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr auto LayoutTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int64 LayoutTestTime = 1794700000;
	bool LayoutSame(const FDYKPlayerProfileSaveData& A, const FDYKPlayerProfileSaveData& B)
	{
		return FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}
	bool LayoutSeed(FDYKPlayerProfileSaveData& P)
	{
		return DYKCityEconomy::Initialize(P, LayoutTestTime)
			&& DYKCityTerritory::Initialize(P, DYKCityTerritory::DEV_PLACEHOLDER());
	}
	int32 LayoutIndex(const FDYKPlayerProfileSaveData& P, const TCHAR* Name)
	{
		return P.Buildings.IndexOfByPredicate([Name](const auto& B) { return B.BuildingId.Name == FName(Name); });
	}
	FGuid LayoutGuid(const FDYKPlayerProfileSaveData& P, int32 Index)
	{
		const auto* S = P.CityEconomy.Instances.FindByPredicate([Index](const auto& Row) { return Row.BuildingIndex == Index; });
		return S ? S->InstanceId : FGuid();
	}
	int32 LayoutAddOwned(FDYKPlayerProfileSaveData& P, const TCHAR* Name, FIntPoint Origin)
	{
		FDYKBuildingSaveData B;
		B.BuildingId = FDYKBuildingId(Name).ToContentId(); B.CellOrigin = Origin;
		const int32 Index = P.Buildings.Add(B);
		FDYKCityProductionSaveData S;
		S.InstanceId = FGuid::NewGuid(); S.BuildingIndex = Index;
		S.UpdatedAtUnix = S.CollectionStartedAtUnix = LayoutTestTime;
		P.CityEconomy.Instances.Add(S);
		return Index;
	}
	struct FLayoutMemoryService : IDYKProfileService
	{
		FDYKPlayerProfileSaveData Profile;
		TArray<FGuid> Applied;
		int64 Revision = 0;
		int32 WriteAttempts = 0;
		bool bFailWrite = false;
		virtual const FDYKPlayerProfileSaveData* GetProfileSnapshot() const override { return &Profile; }
		virtual int64 GetRevision() const override { return Revision; }
		virtual bool ApplyProfileCommand(const FGuid& Id, TFunctionRef<bool(FDYKPlayerProfileSaveData&)> Command) override
		{
			if (!Id.IsValid()) return false;
			if (Applied.Contains(Id)) return true;
			auto Candidate = Profile;
			if (!Command(Candidate)) return false;
			++WriteAttempts;
			if (bFailWrite) return false;
			Profile = MoveTemp(Candidate); Applied.Add(Id); ++Revision;
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKLayoutBufferCancelTest, "DYK.M203.P12T16T17P16T06.BufferAndCancel", LayoutTestFlags)
bool FDYKLayoutBufferCancelTest::RunTest(const FString& Parameters)
{
	FLayoutMemoryService S;
	if (!TestTrue(TEXT("Legal initial ownership"), LayoutSeed(S.Profile))) return false;
	const auto Before = S.Profile;
	FDYKCityLayoutEditor E;
	TestFalse(TEXT("Editor initially inactive"), E.IsActive());
	TestFalse(TEXT("Inactive commit cannot write"), E.Commit(S));
	if (!TestTrue(TEXT("Begin editor"), E.Begin(S.Profile))) return false;
	TestTrue(TEXT("Editor active"), E.IsActive());
	TestEqual(TEXT("Exactly one layout row for every owned instance, no unbought catalog types"), E.Entries().Num(), Before.Buildings.Num());
	const auto Saw = LayoutGuid(Before, LayoutIndex(Before,TEXT("CITY-SAWMILL")));
	TestTrue(TEXT("Free move and rotation affect draft"), E.Place(Saw,{30,20},1));
	TestFalse(TEXT("Repeated Begin cannot discard active draft"), E.Begin(S.Profile));
	const auto* Staged = E.Entries().FindByPredicate([Saw](const auto& Row){ return Row.InstanceId == Saw; });
	TestTrue(TEXT("Rejected nested Begin preserves staged move"), Staged && Staged->Origin == FIntPoint(30,20) && Staged->QuarterTurns == 1);
	for (const auto& Instance : Before.CityEconomy.Instances)
	{
		TestTrue(TEXT("Any owned building, including castle, can enter buffer"), E.Store(Instance.InstanceId));
		TestTrue(TEXT("Buffer does not modify saved state"), LayoutSame(S.Profile,Before));
		TestFalse(TEXT("Missing owned instance prevents commit"), E.Commit(S));
		TestTrue(TEXT("Failed incomplete commit keeps editor open"), E.IsActive());
	}
	int32 Stored = 0;
	TSet<FGuid> Seen;
	for (const auto& Row : E.Entries())
	{
		Stored += !Row.bPlaced;
		TestFalse(TEXT("No duplicated layout identities"), Seen.Contains(Row.InstanceId)); Seen.Add(Row.InstanceId);
	}
	TestEqual(TEXT("All owned rows buffered, none lost"), Stored, Before.Buildings.Num());
	TestEqual(TEXT("No partial write for incomplete draft"), S.WriteAttempts,0);
	TestFalse(TEXT("Unknown GUID cannot create object"), E.Store(FGuid::NewGuid()));
	TestFalse(TEXT("Invalid GUID cannot create object"), E.Place(FGuid(),{30,20},0));
	E.Cancel();
	TestFalse(TEXT("Cancel exits editor despite buffer contents"), E.IsActive());
	TestTrue(TEXT("Cancel preserves every original saved field"), LayoutSame(S.Profile,Before));
	TestEqual(TEXT("Cancel never writes or journals"), S.Revision,int64(0));
	TestTrue(TEXT("A new editing session starts from original layout"), E.Begin(S.Profile));
	for (const auto& Row : E.Entries())
	{
		const auto* State = Before.CityEconomy.Instances.FindByPredicate([&Row](const auto& I){ return I.InstanceId == Row.InstanceId; });
		if (!TestNotNull(TEXT("Original instance retained"),State)) continue;
		const auto& B = Before.Buildings[State->BuildingIndex];
		TestTrue(TEXT("Cancel restores original placement and rotation"), Row.bPlaced && Row.Origin == B.CellOrigin && Row.QuarterTurns == B.RotationQuarterTurns);
	}
	E.Cancel();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKLayoutGeometryCommitTest, "DYK.M203.P12T16P16T08.AtomicCommitGeometryAndFailure", LayoutTestFlags)
bool FDYKLayoutGeometryCommitTest::RunTest(const FString& Parameters)
{
	FLayoutMemoryService S;
	if (!TestTrue(TEXT("Seed city"),LayoutSeed(S.Profile))) return false;
	const auto Before = S.Profile;
	const int32 SI = LayoutIndex(Before,TEXT("CITY-SAWMILL"));
	const int32 QI = LayoutIndex(Before,TEXT("CITY-QUARRY"));
	const int32 CI = LayoutIndex(Before,TEXT("CITY-CASTLE"));
	const auto Saw = LayoutGuid(Before,SI), Quarry = LayoutGuid(Before,QI);
	FDYKCityLayoutEditor E;
	if (!TestTrue(TEXT("Begin"),E.Begin(S.Profile))) return false;
	for (FIntPoint Bad : {FIntPoint(0,0),FIntPoint(-1,20),FIntPoint(49,49),FIntPoint(MAX_int32,MIN_int32),FIntPoint(12,12),Before.Buildings[QI].CellOrigin})
	{
		TestFalse(TEXT("Locked, out of bounds, overflow, castle or occupied geometry rejected"),E.CanPlace(Saw,Bad,0));
		TestFalse(TEXT("Illegal move cannot change draft"),E.Place(Saw,Bad,0));
	}
	for (int32 Bad : {-1,4,MIN_int32,MAX_int32}) TestFalse(TEXT("Invalid rotation rejected"),E.Place(Saw,{30,20},Bad));
	TestFalse(TEXT("Unknown instance cannot occupy cells"),E.CanPlace(FGuid::NewGuid(),{30,20},0));
	TestTrue(TEXT("Buffer sawmill"),E.Store(Saw));
	TestFalse(TEXT("Other occupied building still blocks cells"),E.CanPlace(Saw,Before.Buildings[QI].CellOrigin,0));
	TestTrue(TEXT("Buffer quarry"),E.Store(Quarry));
	TestTrue(TEXT("Buffer releases cells for swap"),E.CanPlace(Quarry,Before.Buildings[SI].CellOrigin,2));
	TestTrue(TEXT("Quarry moves into former sawmill area"),E.Place(Quarry,Before.Buildings[SI].CellOrigin,2));
	TestTrue(TEXT("Sawmill moves and turns in former quarry area"),E.Place(Saw,Before.Buildings[QI].CellOrigin,1));
	TestTrue(TEXT("Castle temporarily buffered"),E.Store(LayoutGuid(Before,CI)));
	TestTrue(TEXT("Castle moves with full perimeter into empty open area"),E.Place(LayoutGuid(Before,CI),{26,17},3));
	TestTrue(TEXT("Draft actions do not save"),LayoutSame(S.Profile,Before));
	S.bFailWrite = true;
	TestFalse(TEXT("Persistence failure refuses entire commit"),E.Commit(S));
	TestTrue(TEXT("Persistence failure keeps editor open for retry"),E.IsActive());
	TestTrue(TEXT("Persistence failure cannot partially move either building"),LayoutSame(S.Profile,Before));
	TestEqual(TEXT("Persistence failure creates no journal entry"),S.Applied.Num(),0);
	S.bFailWrite = false;
	TestTrue(TEXT("Retry persists whole layout"),E.Commit(S));
	TestFalse(TEXT("Successful commit exits"),E.IsActive());
	TestEqual(TEXT("One successful transaction"),S.Revision,int64(1));
	TestEqual(TEXT("One successful command identity"),S.Applied.Num(),1);
	auto Expected = Before;
	Expected.Buildings[SI].CellOrigin = Before.Buildings[QI].CellOrigin; Expected.Buildings[SI].RotationQuarterTurns = 1;
	Expected.Buildings[QI].CellOrigin = Before.Buildings[SI].CellOrigin; Expected.Buildings[QI].RotationQuarterTurns = 2;
	Expected.Buildings[CI].CellOrigin = {26,17}; Expected.Buildings[CI].RotationQuarterTurns = 3;
	TestTrue(TEXT("Commit modifies only intended placements, preserving identity, level, resources, timers and progress"),LayoutSame(S.Profile,Expected));
	TestFalse(TEXT("Repeated inactive commit cannot duplicate transaction"),E.Commit(S));
	TestEqual(TEXT("Repeated commit has no extra payment or write"),S.Revision,int64(1));
	TestTrue(TEXT("Saved final placement passes city validation"),DYKCityTerritory::Validate(S.Profile,DYKCityTerritory::DEV_PLACEHOLDER()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKLayoutConcurrentStateTest, "DYK.M203.LatestEconomyAndLayoutConflicts", LayoutTestFlags)
bool FDYKLayoutConcurrentStateTest::RunTest(const FString& Parameters)
{
	FLayoutMemoryService S;
	if (!TestTrue(TEXT("Seed"),LayoutSeed(S.Profile))) return false;
	const int32 SI = LayoutIndex(S.Profile,TEXT("CITY-SAWMILL"));
	const auto Saw = LayoutGuid(S.Profile,SI);
	FDYKCityLayoutEditor E;
	if (!TestTrue(TEXT("Begin"),E.Begin(S.Profile))) return false;
	TestTrue(TEXT("Stage placement"),E.Place(Saw,{30,20},1));
	TestTrue(TEXT("Independent resource collection continues during edit"),DYKCityEconomy::Collect(S,FGuid::NewGuid(),Saw,LayoutTestTime+125));
	TestTrue(TEXT("Independent development fixture advances while editing"),S.ApplyProfileCommand(FGuid::NewGuid(),[SI](auto& P)
	{
		P.Buildings[SI].Level = 2;
		P.Buildings[SI].ConstructionStartedAtUnix = LayoutTestTime + 100;
		P.Buildings[SI].ConstructionCompletesAtUnix = LayoutTestTime + 160;
		P.ResearchedContent.Add({EDYKContentKind::Warrior,TEXT("WAR-PEASANT")});
		return DYKCityEconomy::Validate(P);
	}));
	const auto Latest = S.Profile;
	TestTrue(TEXT("Commit after independent economy command"),E.Commit(S));
	auto Expected = Latest;
	Expected.Buildings[SI].CellOrigin = {30,20}; Expected.Buildings[SI].RotationQuarterTurns = 1;
	TestTrue(TEXT("Commit retains latest balance, accumulation, cycle remainder and timestamps"),LayoutSame(S.Profile,Expected));
	TestEqual(TEXT("Collection, development and editor are separate durable commands"),S.Revision,int64(3));
	TestTrue(TEXT("Reenter"),E.Begin(S.Profile));
	TestTrue(TEXT("Stage another move"),E.Place(Saw,{34,20},0));
	TestTrue(TEXT("External layout change"),DYKCityTerritory::Move(S,FGuid::NewGuid(),Saw,{30,24},0,DYKCityTerritory::DEV_PLACEHOLDER()));
	const auto ExternallyMoved = S.Profile;
	TestFalse(TEXT("Stale editor cannot overwrite a committed layout change"),E.Commit(S));
	TestTrue(TEXT("Conflict preserves newer placement"),LayoutSame(S.Profile,ExternallyMoved));
	TestTrue(TEXT("Conflict retains draft for explicit cancel"),E.IsActive());
	E.Cancel();
	TestTrue(TEXT("Begin before ownership change"),E.Begin(S.Profile));
	TestTrue(TEXT("Stage change"),E.Place(Saw,{34,20},0));
	LayoutAddOwned(S.Profile,TEXT("CITY-BARRACKS"),{26,25});
	const auto NewOwnership = S.Profile;
	TestFalse(TEXT("Old editor cannot delete a newly owned building"),E.Commit(S));
	TestTrue(TEXT("Ownership conflict preserves all latest objects"),LayoutSame(S.Profile,NewOwnership));
	E.Cancel();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKLayoutOwnershipRoundtripTest, "DYK.M203.P16T07.DecorationLimitsAndRoundtrip", LayoutTestFlags)
bool FDYKLayoutOwnershipRoundtripTest::RunTest(const FString& Parameters)
{
	FLayoutMemoryService S;
	if (!TestTrue(TEXT("Seed"),LayoutSeed(S.Profile))) return false;
	const auto Base = S.Profile;
	FDYKCityLayoutEditor E;
	for (int32 Case = 0; Case < 6; ++Case)
	{
		auto Invalid = Base;
		switch (Case)
		{
		case 0: Invalid.CityEconomy.Instances[1].InstanceId = Invalid.CityEconomy.Instances[0].InstanceId; break;
		case 1: Invalid.CityEconomy.Instances[1].BuildingIndex = Invalid.CityEconomy.Instances[0].BuildingIndex; break;
		case 2: Invalid.CityEconomy.Instances.RemoveAt(1); break;
		case 3: Invalid.Buildings[1].CellOrigin = {10,10}; break;
		case 4: LayoutAddOwned(Invalid,TEXT("CITY-CASTLE"),{30,20}); break;
		case 5:
			LayoutAddOwned(Invalid,TEXT("CITY-HOUSE"),{27,25});
			LayoutAddOwned(Invalid,TEXT("CITY-HOUSE"),{30,25});
			LayoutAddOwned(Invalid,TEXT("CITY-HOUSE"),{34,25}); break;
		}
		TestFalse(TEXT("Malformed identity, geometry or over-limit ownership cannot open editor"),E.Begin(Invalid));
		TestFalse(TEXT("Rejected session does not activate editor"),E.IsActive());
	}
	const int32 DI = LayoutAddOwned(S.Profile,TEXT("CITY-DECOR-DEV_PLACEHOLDER"),{26,25});
	S.Profile.Buildings[DI].PaidCosts.Add({FDYKResourceId(TEXT("RES-WOOD")).ToContentId(),17});
	const auto Decor = LayoutGuid(S.Profile,DI);
	const auto Before = S.Profile;
	if (!TestTrue(TEXT("Optional decoration is valid owned object"),E.Begin(S.Profile))) return false;
	TestTrue(TEXT("Decoration can be buffered"),E.Store(Decor));
	TestFalse(TEXT("Buffered unsold decoration also prevents commit"),E.Commit(S));
	TestTrue(TEXT("Decoration remains owned and original saved profile intact"),LayoutSame(S.Profile,Before));
	TestTrue(TEXT("Return decoration to map"),E.Place(Decor,{27,25},3));
	TestTrue(TEXT("Commit with all owned objects placed"),E.Commit(S));
	auto Expected = Before; Expected.Buildings[DI].CellOrigin = {27,25}; Expected.Buildings[DI].RotationQuarterTurns = 3;
	TestTrue(TEXT("No sale/refund or loss caused by temporary storage"),LayoutSame(S.Profile,Expected));
	TStrongObjectPtr<UDYKProfileSaveGame> Save(NewObject<UDYKProfileSaveGame>());
	Save->Profile = S.Profile; Save->AppliedCommandIds = S.Applied;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Serialize committed full layout"),UGameplayStatics::SaveGameToMemory(Save.Get(),Bytes))) return false;
	TStrongObjectPtr<UDYKProfileSaveGame> Reload(Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
	if (!TestNotNull(TEXT("Deserialize committed layout"),Reload.Get())) return false;
	TestTrue(TEXT("Roundtrip retains all ownership, transforms, actual paid costs, economic and progression data"),LayoutSame(Reload->Profile,S.Profile));
	TestTrue(TEXT("Roundtrip preserves command ledger"),Reload->AppliedCommandIds == S.Applied);
	TestTrue(TEXT("Reloaded layout is legal"),DYKCityTerritory::Validate(Reload->Profile,DYKCityTerritory::DEV_PLACEHOLDER()));
	TestTrue(TEXT("Reloaded ownership obeys limits"),DYKCityBuildingRules::ValidateOwnership(Reload->Profile,DYKCityBuildingRules::DEV_PLACEHOLDER()));
	return true;
}
#endif
