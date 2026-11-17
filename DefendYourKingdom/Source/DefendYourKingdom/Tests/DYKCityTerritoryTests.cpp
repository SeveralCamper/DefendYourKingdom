#include "../Meta/DYKCityTerritory.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Meta/DYKMissionRewards.h"
#include "../Saving/DYKProfileService.h"
#include "../Saving/DYKSaveSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

struct FDYKCityTerritoryTestAccess
{
	static void Open(UDYKSaveSubsystem& Service, const FString& Prefix)
	{
		Service.SlotPrefix=Prefix; Service.LoadOrCreateProfile();
	}
	static void SetPrefix(UDYKSaveSubsystem& Service, const FString& Prefix) { Service.SlotPrefix=Prefix; }
};

namespace
{
	constexpr auto TerritoryTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int64 TerritoryTestTime = 1794700000;
	const FDYKResourceId TerritoryWood(TEXT("RES-WOOD"));
	const FDYKResourceId TerritoryStone(TEXT("RES-STONE"));

	bool TerritorySame(const FDYKPlayerProfileSaveData& A, const FDYKPlayerProfileSaveData& B)
	{
		return FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}
	int32 TerritoryBuilding(const FDYKPlayerProfileSaveData& P, FName Name)
	{
		return P.Buildings.IndexOfByPredicate([Name](const auto& B) { return B.BuildingId.Name == Name; });
	}
	FGuid TerritoryInstance(const FDYKPlayerProfileSaveData& P, FName Name)
	{
		const int32 Index = TerritoryBuilding(P, Name);
		const auto* State = P.CityEconomy.Instances.FindByPredicate([Index](const auto& S) { return S.BuildingIndex == Index; });
		return State ? State->InstanceId : FGuid();
	}
	void TerritorySetBalance(FDYKPlayerProfileSaveData& P, FDYKResourceId Id, int64 Amount)
	{
		for (auto& R : P.Resources) if (R.ResourceId == Id.ToContentId()) R.Amount = Amount;
	}
	bool TerritorySeed(FDYKPlayerProfileSaveData& P, const FDYKCityTerritoryConfig& C)
	{
		return DYKCityEconomy::Initialize(P, TerritoryTestTime) && DYKCityTerritory::Initialize(P,C);
	}
	// Implements the transactional service contract; simulated durable-write failures
	// happen after command validation, so rollback assertions exercise both mutations.
	struct FTerritoryMemoryService : IDYKProfileService
	{
		FDYKPlayerProfileSaveData Profile;
		TArray<FGuid> Applied;
		int64 Revision = 0;
		bool bFailWrite = false;
		virtual const FDYKPlayerProfileSaveData* GetProfileSnapshot() const override { return &Profile; }
		virtual int64 GetRevision() const override { return Revision; }
		virtual bool ApplyProfileCommand(const FGuid& Id, TFunctionRef<bool(FDYKPlayerProfileSaveData&)> Command) override
		{
			if (!Id.IsValid()) return false;
			if (Applied.Contains(Id)) return true;
			auto Candidate = Profile;
			if (!Command(Candidate) || bFailWrite) return false;
			Profile = MoveTemp(Candidate); Applied.Add(Id); ++Revision;
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKTerritoryInitialTest, "DYK.M201.P12T01T10.InitialGridAndMigration", TerritoryTestFlags)
bool FDYKTerritoryInitialTest::RunTest(const FString& Parameters)
{
	const auto& C = DYKCityTerritory::DEV_PLACEHOLDER();
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Legacy economy seeded"), DYKCityEconomy::Initialize(P,TerritoryTestTime))) return false;
	TerritorySetBalance(P,TerritoryWood,37);
	P.CityEconomy.Instances[1].Stored=11;
	P.CityEconomy.Instances[1].RemainderSeconds=29;
	P.Buildings[TerritoryBuilding(P,TEXT("CITY-SAWMILL"))].RotationQuarterTurns=1;
	P.LastDailyRewardDateKey=20261115;
	P.ResearchedContent.Add({EDYKContentKind::Warrior,TEXT("WAR-PEASANT")});
	const auto Legacy=P;
	if (!TestTrue(TEXT("Legacy layout migrated"), DYKCityTerritory::Initialize(P,C))) return false;
	TestEqual(TEXT("Five by five parcels"), P.Parcels.Num(),25);
	TestEqual(TEXT("Exactly six parcels initially open"), P.Parcels.FilterByPredicate([](const auto& Row){return Row.bIsUnlocked;}).Num(),6);
	TestEqual(TEXT("Ten cells per parcel side"),DYKCityTerritory::CellsPerParcel,10);
	TestEqual(TEXT("Whole city fifty by fifty cells"),DYKCityTerritory::CellsPerSide,50);
	for (int32 Y=0;Y<5;++Y) for (int32 X=0;X<5;++X)
		TestEqual(TEXT("One record for each parcel coordinate"),P.Parcels.FilterByPredicate([X,Y](const auto& Row){return Row.Coordinate==FIntPoint(X,Y);}).Num(),1);
	TestTrue(TEXT("Migrated full city legal"),DYKCityTerritory::Validate(P,C));
	TestEqual(TEXT("No building added or removed"),P.Buildings.Num(),Legacy.Buildings.Num());
	for (int32 I=0;I<P.Buildings.Num();++I) TestTrue(TEXT("Building ownership/order/levels retained"),
		P.Buildings[I].BuildingId==Legacy.Buildings[I].BuildingId && P.Buildings[I].Level==Legacy.Buildings[I].Level);
	TestEqual(TEXT("Existing valid quarter-turn retained by migration"),P.Buildings[TerritoryBuilding(P,TEXT("CITY-SAWMILL"))].RotationQuarterTurns,1);
	auto ExpectedEconomy=Legacy.CityEconomy; ExpectedEconomy.TerritoryVersion=DYKCityTerritory::CurrentVersion;
	TestTrue(TEXT("Every production GUID/index/stored/cycle/timestamp retained"),FDYKCitySaveData::StaticStruct()->CompareScriptStruct(&P.CityEconomy,&ExpectedEconomy,0));
	TestEqual(TEXT("Wood balance retained"),DYKCityEconomy::Balance(P,TerritoryWood),int64(37));
	TestEqual(TEXT("Daily reward retained"),P.LastDailyRewardDateKey,Legacy.LastDailyRewardDateKey);
	TestTrue(TEXT("Research retained"),P.ResearchedContent==Legacy.ResearchedContent);
	const auto Before=P;
	TestTrue(TEXT("Second initialization succeeds"),DYKCityTerritory::Initialize(P,C));
	TestTrue(TEXT("Second initialization changes nothing"),TerritorySame(P,Before));
	const auto& Castle=P.Buildings[TerritoryBuilding(P,TEXT("CITY-CASTLE"))];
	TArray<FIntPoint> Cells;
	TestTrue(TEXT("Castle footprint available"),DYKCityTerritory::OccupiedCells(Castle,C,Cells));
	TestEqual(TEXT("Castle exactly one hundred cells"),Cells.Num(),100);
	for (int32 Y=0;Y<10;++Y) for (int32 X=0;X<10;++X)
		TestTrue(TEXT("Entire ten by ten castle area occupied"),Cells.Contains(Castle.CellOrigin+FIntPoint(X,Y)));
	for (int32 Delta : {1,10})
	{
		auto Invalid=P; Invalid.Parcels[Delta].Coordinate=Invalid.Parcels[0].Coordinate;
		TestFalse(TEXT("Duplicate parcel rejected"),DYKCityTerritory::Validate(Invalid,C));
	}
	auto Unknown=P; Unknown.CityEconomy.TerritoryVersion=DYKCityTerritory::CurrentVersion+1;
	TestFalse(TEXT("Unknown territory version rejected"),DYKCityTerritory::Initialize(Unknown,C));
	TestEqual(TEXT("Unknown version preserved"),Unknown.CityEconomy.TerritoryVersion,DYKCityTerritory::CurrentVersion+1);
	auto BadConfig=C; BadConfig.InitialParcels.Remove(*BadConfig.InitialParcels.CreateConstIterator());
	auto Failed=Legacy;
	TestFalse(TEXT("Invalid initial parcel configuration rejected"),DYKCityTerritory::Initialize(Failed,BadConfig));
	TestTrue(TEXT("Failed migration entirely atomic"),TerritorySame(Failed,Legacy));
	auto UnknownBuilding=Legacy; UnknownBuilding.Buildings[1].BuildingId.Name=TEXT("UNKNOWN-BUILDING");
	const auto UnknownBefore=UnknownBuilding;
	TestFalse(TEXT("Unknown legacy footprint cannot migrate"),DYKCityTerritory::Initialize(UnknownBuilding,C));
	TestTrue(TEXT("Unknown ownership not lost on migration failure"),TerritorySame(UnknownBuilding,UnknownBefore));
	for (int32 Turns : {-1,4,MIN_int32,MAX_int32})
	{
		auto InvalidTurn=Legacy;
		InvalidTurn.Buildings[TerritoryBuilding(InvalidTurn,TEXT("CITY-SAWMILL"))].RotationQuarterTurns=Turns;
		const auto InvalidBefore=InvalidTurn;
		TestFalse(TEXT("Migration rejects invalid pre-existing rotation"),DYKCityTerritory::Initialize(InvalidTurn,C));
		TestTrue(TEXT("Invalid legacy rotation rejection is atomic"),TerritorySame(InvalidTurn,InvalidBefore));
	}
	auto OpenLegacy=Legacy; OpenLegacy.Parcels.Add({{0,0},true});
	TestTrue(TEXT("Legacy owned land migrates"),DYKCityTerritory::Initialize(OpenLegacy,C));
	TestTrue(TEXT("Legacy owned extra parcel retained"),DYKCityTerritory::IsUnlocked(OpenLegacy,{0,0}));
	TestEqual(TEXT("Legacy extra ownership added to six initial parcels"),OpenLegacy.Parcels.FilterByPredicate([](const auto& Row){return Row.bIsUnlocked;}).Num(),7);
	for (auto Parcel : {FIntPoint(-1,0),FIntPoint(5,0)})
	{
		auto BadLegacy=Legacy; BadLegacy.Parcels.Add({Parcel,true}); const auto BadBefore=BadLegacy;
		TestFalse(TEXT("Malformed legacy parcel rejected"),DYKCityTerritory::Initialize(BadLegacy,C));
		TestTrue(TEXT("Malformed legacy land preserved on rejection"),TerritorySame(BadLegacy,BadBefore));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKTerritoryClearanceMigrationTest, "DYK.M204.MandatoryPerimeter.V1Migration", TerritoryTestFlags)
bool FDYKTerritoryClearanceMigrationTest::RunTest(const FString& Parameters)
{
	const auto& C=DYKCityTerritory::DEV_PLACEHOLDER();
	FDYKPlayerProfileSaveData P;
	if (!TestTrue(TEXT("Seed latest city"),TerritorySeed(P,C))) return false;
	P.CityEconomy.TerritoryVersion=1;
	P.Buildings[TerritoryBuilding(P,TEXT("CITY-CASTLE"))].CellOrigin={10,10};
	P.CityEconomy.Instances[1].Stored=11; P.CityEconomy.Instances[1].RemainderSeconds=29;
	P.Buildings[TerritoryBuilding(P,TEXT("CITY-MILL"))].RotationQuarterTurns=1;
	TerritorySetBalance(P,TerritoryWood,37); P.LastDailyRewardDateKey=20261115;
	P.Parcels.FindByPredicate([](const auto& R){return R.Coordinate==FIntPoint(4,2);})->bIsUnlocked=true;
	const auto Legacy=P;
	TestTrue(TEXT("Real V1 edge-castle save remains openable"),DYKCityTerritory::Initialize(P,C));
	TestTrue(TEXT("Castle moved minimally to allow all four road sides"),P.Buildings[TerritoryBuilding(P,TEXT("CITY-CASTLE"))].CellOrigin==FIntPoint(11,11));
	auto Expected=Legacy; Expected.CityEconomy.TerritoryVersion=DYKCityTerritory::CurrentVersion;
	for (int32 I=0;I<P.Buildings.Num();++I) Expected.Buildings[I].CellOrigin=P.Buildings[I].CellOrigin;
	TestTrue(TEXT("Only origins/version changed; GUIDs, order, paid costs, resources, production and open land preserved"),TerritorySame(P,Expected));
	const auto Migrated=P;
	TestTrue(TEXT("V2 reload succeeds"),DYKCityTerritory::Initialize(P,C));
	TestTrue(TEXT("V2 reload does not repeat repair"),TerritorySame(P,Migrated));
	P.CityEconomy.TerritoryVersion=1; Expected=P; Expected.CityEconomy.TerritoryVersion=DYKCityTerritory::CurrentVersion;
	TestTrue(TEXT("Already ring-safe V1 geometry migrates"),DYKCityTerritory::Initialize(P,C));
	TestTrue(TEXT("Ring-safe V1 changes version alone"),TerritorySame(P,Expected));

	// A dense V1 strip is legal under the old body-only contract, but lacks road clearance.
	auto Dense=Legacy;
	const TArray<FName> Names={TEXT("CITY-CASTLE"),TEXT("CITY-SAWMILL"),TEXT("CITY-QUARRY"),TEXT("CITY-MILL"),TEXT("CITY-HOUSE")};
	const TArray<FIntPoint> Origins={{10,10},{20,10},{23,10},{26,10},{29,10}};
	for (int32 I=0;I<Names.Num();++I)
	{
		auto& B=Dense.Buildings[TerritoryBuilding(Dense,Names[I])]; B.CellOrigin=Origins[I]; B.RotationQuarterTurns=0;
	}
	const auto DenseBefore=Dense;
	TestTrue(TEXT("Dense old strip repaired without losing owned buildings"),DYKCityTerritory::Initialize(Dense,C));
	TestTrue(TEXT("Repaired dense layout satisfies all mandatory perimeters"),DYKCityTerritory::Validate(Dense,C));
	Expected=DenseBefore; Expected.CityEconomy.TerritoryVersion=DYKCityTerritory::CurrentVersion;
	for (int32 I=0;I<Dense.Buildings.Num();++I) Expected.Buildings[I].CellOrigin=Dense.Buildings[I].CellOrigin;
	TestTrue(TEXT("Dense repair changes no non-placement field"),TerritorySame(Dense,Expected));

	// No 12x12 castle clearance can fit in these isolated one-parcel islands.
	auto Impossible=Legacy;
	const TSet<FIntPoint> Islands={{1,1},{3,1},{1,3},{3,3},{0,0},{4,4}};
	for (auto& Parcel:Impossible.Parcels) Parcel.bIsUnlocked=Islands.Contains(Parcel.Coordinate);
	const TArray<FIntPoint> IslandOrigins={{10,10},{31,11},{11,31},{31,31},{1,1}};
	for (int32 I=0;I<Names.Num();++I)
	{
		auto& B=Impossible.Buildings[TerritoryBuilding(Impossible,Names[I])]; B.CellOrigin=IslandOrigins[I]; B.RotationQuarterTurns=0;
	}
	const auto ImpossibleBefore=Impossible;
	TestFalse(TEXT("Impossible legacy ownership fails without buying land or deleting buildings"),DYKCityTerritory::Initialize(Impossible,C));
	TestTrue(TEXT("Failed repair preserves entire old save and old version"),TerritorySame(Impossible,ImpossibleBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKTerritoryFootprintTest, "DYK.M201.P12T06.RotationAndMalformedGeometry", TerritoryTestFlags)
bool FDYKTerritoryFootprintTest::RunTest(const FString& Parameters)
{
	auto C=DYKCityTerritory::DEV_PLACEHOLDER();
	const FName ShapeName(TEXT("TEST-ASYMMETRIC"));
	// The missing corner distinguishes all four rotations, rather than merely swapping a rectangle.
	C.Footprints.Add(ShapeName,{{0,0},{1,0},{2,0},{0,1}});
	FDYKBuildingSaveData B; B.BuildingId={EDYKContentKind::Building,ShapeName}; B.CellOrigin={20,20};
	const TArray<TArray<FIntPoint>> Expected={
		{{20,20},{21,20},{22,20},{20,21}},
		{{21,20},{21,21},{21,22},{20,20}},
		{{22,21},{21,21},{20,21},{22,20}},
		{{20,22},{20,21},{20,20},{21,22}}};
	for (int32 Turns=0;Turns<4;++Turns)
	{
		B.RotationQuarterTurns=Turns; TArray<FIntPoint> Cells;
		TestTrue(TEXT("Quarter turn accepted"),DYKCityTerritory::OccupiedCells(B,C,Cells));
		TestEqual(TEXT("Shape cell count preserved"),Cells.Num(),4);
		for (auto Cell:Expected[Turns]) TestTrue(TEXT("Every expected rotated cell occupied"),Cells.Contains(Cell));
	}
	TArray<FIntPoint> Cells;
	for (int32 Turns : {-1,4,MAX_int32,MIN_int32})
	{
		B.RotationQuarterTurns=Turns;
		TestFalse(TEXT("Non-quarter-turn value rejected"),DYKCityTerritory::OccupiedCells(B,C,Cells));
	}
	B.RotationQuarterTurns=0;
	for (FIntPoint Origin : {FIntPoint(-1,20),FIntPoint(20,-1),FIntPoint(50,20),FIntPoint(20,50),FIntPoint(MAX_int32,MIN_int32),FIntPoint(49,49)})
	{
		B.CellOrigin=Origin;
		TestFalse(TEXT("Negative/outside/overflowing complete footprint rejected"),DYKCityTerritory::OccupiedCells(B,C,Cells));
		TestTrue(TEXT("Rejected footprint never leaves partial cell result"),Cells.IsEmpty());
	}
	B.CellOrigin={20,20};
	for (const TArray<FIntPoint>& Shape : TArray<TArray<FIntPoint>>{{},{ {0,0},{0,0} },{{-1,0}},{{MAX_int32,0}},{{0,50}}})
	{
		C.Footprints[ShapeName]=Shape;
		TestFalse(TEXT("Invalid configured footprint rejected"),DYKCityTerritory::OccupiedCells(B,C,Cells));
	}
	C.Footprints[ShapeName]={{0,0}}; B.BuildingId.Kind=EDYKContentKind::Resource;
	TestFalse(TEXT("Wrong content kind cannot place"),DYKCityTerritory::OccupiedCells(B,C,Cells));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKTerritoryPlacementTest, "DYK.M201.P12T02T07T11T12T14T24.PlacementAtomicity", TerritoryTestFlags)
bool FDYKTerritoryPlacementTest::RunTest(const FString& Parameters)
{
	const auto& C=DYKCityTerritory::DEV_PLACEHOLDER(); FTerritoryMemoryService S;
	if (!TestTrue(TEXT("Seed legal city"),TerritorySeed(S.Profile,C))) return false;
	const auto Sawmill=TerritoryInstance(S.Profile,TEXT("CITY-SAWMILL"));
	const auto Castle=TerritoryInstance(S.Profile,TEXT("CITY-CASTLE"));
	const auto CastleOrigin=S.Profile.Buildings[TerritoryBuilding(S.Profile,TEXT("CITY-CASTLE"))].CellOrigin;
	const auto Before=S.Profile;
	for (FIntPoint Origin : {FIntPoint(0,0),FIntPoint(-1,20),FIntPoint(50,20),FIntPoint(20,29),FIntPoint(MAX_int32,MAX_int32),CastleOrigin+FIntPoint(4,4)})
	{
		TestFalse(TEXT("Illegal placement rejected"),DYKCityTerritory::Move(S,FGuid::NewGuid(),Sawmill,Origin,0,C));
		TestTrue(TEXT("Rejection preserves every profile field"),TerritorySame(S.Profile,Before));
		TestEqual(TEXT("Rejected move never commits"),S.Revision,int64(0));
	}
	TestFalse(TEXT("Unknown instance cannot move"),DYKCityTerritory::Move(S,FGuid::NewGuid(),FGuid::NewGuid(),{30,20},0,C));
	TestFalse(TEXT("Invalid rotation cannot move"),DYKCityTerritory::Move(S,FGuid::NewGuid(),Sawmill,{30,20},4,C));
	S.bFailWrite=true;
	TestFalse(TEXT("Valid move fails if persistence fails"),DYKCityTerritory::Move(S,FGuid::NewGuid(),Sawmill,{36,23},1,C));
	TestTrue(TEXT("Persistence failure preserves old placement"),TerritorySame(S.Profile,Before));
	S.bFailWrite=false;
	TestTrue(TEXT("Free existing building moved with rotation"),DYKCityTerritory::Move(S,FGuid::NewGuid(),Sawmill,{36,23},1,C));
	TestEqual(TEXT("Building ownership count unchanged"),S.Profile.Buildings.Num(),Before.Buildings.Num());
	TestEqual(TEXT("Move never charges resources"),DYKCityEconomy::Balance(S.Profile,TerritoryWood),DYKCityEconomy::Balance(Before,TerritoryWood));
	const auto& Moved=S.Profile.Buildings[TerritoryBuilding(S.Profile,TEXT("CITY-SAWMILL"))];
	TestTrue(TEXT("Position persisted"),Moved.CellOrigin==FIntPoint(36,23)); TestEqual(TEXT("Rotation persisted"),Moved.RotationQuarterTurns,1);
	TestTrue(TEXT("Castle moves with its full open perimeter for free"),DYKCityTerritory::Move(S,FGuid::NewGuid(),Castle,{21,11},3,C));
	const auto BeforeCastleCollision=S.Profile;
	TestFalse(TEXT("Castle cannot move onto partially occupied parcel away from origin"),DYKCityTerritory::Move(S,FGuid::NewGuid(),Castle,{30,20},0,C));
	TestTrue(TEXT("Castle collision preserves original and all other buildings"),TerritorySame(S.Profile,BeforeCastleCollision));
	for (int32 Turns=0;Turns<4;++Turns)
	{
		TestTrue(TEXT("All turns of castle preserve approved area"),DYKCityTerritory::Move(S,FGuid::NewGuid(),Castle,{21,11},Turns,C));
	}
	const auto Current=S.Profile;
	auto BadCastle=C; BadCastle.Footprints[TEXT("CITY-CASTLE")]={{0,0}};
	TestFalse(TEXT("Configuration cannot shrink castle"),DYKCityTerritory::Validate(Current,BadCastle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKTerritoryExpansionTest, "DYK.M201.P12T03T04T05.ExpansionConditionsAndTransactions", TerritoryTestFlags)
bool FDYKTerritoryExpansionTest::RunTest(const FString& Parameters)
{
	auto C=DYKCityTerritory::DEV_PLACEHOLDER(); FTerritoryMemoryService S;
	if (!TestTrue(TEXT("Seed"),TerritorySeed(S.Profile,C))) return false;
	TerritorySetBalance(S.Profile,TerritoryWood,100); TerritorySetBalance(S.Profile,TerritoryStone,40);
	C.ParcelRules[{0,1}]={{{TerritoryWood.ToContentId(),7}}, {TEXT("HasResearch")}};
	C.ParcelRules[{4,2}]={{{TerritoryStone.ToContentId(),11}}, {TEXT("HasBuildingProgress")}};
	C.Conditions.Add(TEXT("HasResearch"),[](const auto& P){return !P.ResearchedContent.IsEmpty();});
	C.Conditions.Add(TEXT("HasBuildingProgress"),[](const auto& P){return P.LastDailyRewardDateKey==20261115;});
	TestFalse(TEXT("Parcel-specific research requirement enforced"),DYKCityTerritory::CanUnlock(S.Profile,{0,1},C));
	TestFalse(TEXT("Different parcel-specific progression enforced"),DYKCityTerritory::CanUnlock(S.Profile,{4,2},C));
	S.Profile.ResearchedContent.Add({EDYKContentKind::Warrior,TEXT("WAR-PEASANT")});
	TestTrue(TEXT("Only research parcel becomes eligible"),DYKCityTerritory::CanUnlock(S.Profile,{0,1},C));
	TestFalse(TEXT("Other parcel still gated independently"),DYKCityTerritory::CanUnlock(S.Profile,{4,2},C));
	S.Profile.LastDailyRewardDateKey=20261115;
	TestTrue(TEXT("Other progression now satisfied"),DYKCityTerritory::CanUnlock(S.Profile,{4,2},C));
	for (auto Parcel : {FIntPoint(0,0),FIntPoint(4,0),FIntPoint(0,4),FIntPoint(-1,1),FIntPoint(MAX_int32,1)})
		TestFalse(TEXT("Diagonal/distant/outside parcel rejected despite resources"),DYKCityTerritory::CanUnlock(S.Profile,Parcel,C));
	const auto Before=S.Profile;
	S.bFailWrite=true; const auto Command=FGuid::NewGuid();
	TestFalse(TEXT("Failed durable write rejects expansion"),DYKCityTerritory::Unlock(S,Command,{4,2},C));
	TestTrue(TEXT("Failed write changes neither resources nor opened parcels"),TerritorySame(S.Profile,Before));
	TestEqual(TEXT("Failed write no revision"),S.Revision,int64(0)); S.bFailWrite=false;
	TestTrue(TEXT("Non-first eligible parcel opens without fixed order"),DYKCityTerritory::Unlock(S,Command,{4,2},C));
	TestEqual(TEXT("Only its own stone cost charged"),DYKCityEconomy::Balance(S.Profile,TerritoryStone),int64(29));
	TestEqual(TEXT("Other parcel wood cost untouched"),DYKCityEconomy::Balance(S.Profile,TerritoryWood),int64(100));
	const auto Once=S.Profile; const auto OnceRevision=S.Revision;
	TestTrue(TEXT("Same command idempotent"),DYKCityTerritory::Unlock(S,Command,{4,2},C));
	TestTrue(TEXT("Same command no double charge"),TerritorySame(S.Profile,Once));
	TestEqual(TEXT("Same command no extra revision"),S.Revision,OnceRevision);
	TestFalse(TEXT("Already open parcel with new command rejected"),DYKCityTerritory::Unlock(S,FGuid::NewGuid(),{4,2},C));
	TestTrue(TEXT("Duplicate expansion leaves state"),TerritorySame(S.Profile,Once));
	TestTrue(TEXT("Alternative branch opens later"),DYKCityTerritory::Unlock(S,FGuid::NewGuid(),{0,1},C));
	TestEqual(TEXT("Own exact wood price"),DYKCityEconomy::Balance(S.Profile,TerritoryWood),int64(93));
	TestTrue(TEXT("New edge makes formerly diagonal parcel eligible"),DYKCityTerritory::CanUnlock(S.Profile,{0,0},C));
	C.ParcelRules[{0,0}].Conditions={TEXT("UnknownAuthorCondition")};
	TestFalse(TEXT("Unknown extension condition fails closed"),DYKCityTerritory::CanUnlock(S.Profile,{0,0},C));
	C.ParcelRules[{0,0}].Conditions.Reset(); C.ParcelRules[{0,0}].Costs={{TerritoryWood.ToContentId(),MAX_int64}};
	TestFalse(TEXT("Insufficient resource and large cost rejected"),DYKCityTerritory::CanUnlock(S.Profile,{0,0},C));
	for (int64 Price : {int64(0),int64(-1),MIN_int64})
	{
		C.ParcelRules[{0,0}].Costs={{TerritoryWood.ToContentId(),Price}};
		TestFalse(TEXT("Malformed cost rejected"),DYKCityTerritory::CanUnlock(S.Profile,{0,0},C));
	}
	C.ParcelRules[{0,0}].Costs={{TerritoryWood.ToContentId(),1},{TerritoryWood.ToContentId(),1}};
	TestFalse(TEXT("Duplicate resource costs rejected"),DYKCityTerritory::CanUnlock(S.Profile,{0,0},C));
	C.ParcelRules.Remove({0,0});
	TestFalse(TEXT("Missing parcel rule fails closed"),DYKCityTerritory::CanUnlock(S.Profile,{0,0},C));
	auto Corrupt=S.Profile; const auto DuplicateResource=Corrupt.Resources[0]; Corrupt.Resources.Add(DuplicateResource);
	TestFalse(TEXT("Duplicate resource rows cannot bypass payment validation"),DYKCityTerritory::CanUnlock(Corrupt,{1,0},C));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKTerritoryMissionGateTest, "DYK.M201.P12T05.ExistingMissionCondition", TerritoryTestFlags)
bool FDYKTerritoryMissionGateTest::RunTest(const FString& Parameters)
{
	const auto& C=DYKCityTerritory::DEV_PLACEHOLDER(); FTerritoryMemoryService S;
	if (!TestTrue(TEXT("Seed"),TerritorySeed(S.Profile,C))) return false;
	TerritorySetBalance(S.Profile,TerritoryWood,100);
	TestTrue(TEXT("Open approach to mission-gated southern parcel"),DYKCityTerritory::Unlock(S,FGuid::NewGuid(),{2,3},C));
	TestFalse(TEXT("Mission-gated parcel closed before victory"),DYKCityTerritory::CanUnlock(S.Profile,{2,4},C));
	const auto& Mission=DYKMissionRewards::PolygonDEV_PLACEHOLDER();
	S.Profile.MissionCompletions.Add({Mission.Campaign.ToContentId(),Mission.Mission.ToContentId()});
	TestTrue(TEXT("Actual existing polygon completion satisfies gate"),DYKCityTerritory::CanUnlock(S.Profile,{2,4},C));
	TestTrue(TEXT("Mission-gated parcel unlocks"),DYKCityTerritory::Unlock(S,FGuid::NewGuid(),{2,4},C));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKTerritoryPersistenceTest, "DYK.M201.P12T03T06.PersistenceRoundTrip", TerritoryTestFlags)
bool FDYKTerritoryPersistenceTest::RunTest(const FString& Parameters)
{
	const auto& C=DYKCityTerritory::DEV_PLACEHOLDER(); FTerritoryMemoryService S;
	if (!TestTrue(TEXT("Seed"),TerritorySeed(S.Profile,C))) return false;
	TerritorySetBalance(S.Profile,TerritoryWood,100);
	const auto Expand=FGuid::NewGuid();
	TestTrue(TEXT("Expansion committed before save"),DYKCityTerritory::Unlock(S,Expand,{4,2},C));
	TestTrue(TEXT("Rotated placement committed before save"),DYKCityTerritory::Move(S,FGuid::NewGuid(),TerritoryInstance(S.Profile,TEXT("CITY-SAWMILL")),{42,22},3,C));
	TStrongObjectPtr<UDYKProfileSaveGame> Save(NewObject<UDYKProfileSaveGame>());
	Save->Profile=S.Profile; Save->AppliedCommandIds=S.Applied;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Native GVAS serialization"),UGameplayStatics::SaveGameToMemory(Save.Get(),Bytes))) return false;
	TStrongObjectPtr<UDYKProfileSaveGame> Reload(Cast<UDYKProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
	if (!TestNotNull(TEXT("Native GVAS deserialization"),Reload.Get())) return false;
	TestTrue(TEXT("All profile fields roundtrip exactly"),TerritorySame(Reload->Profile,S.Profile));
	TestTrue(TEXT("Command receipts roundtrip exactly"),Reload->AppliedCommandIds==S.Applied);
	TestTrue(TEXT("Deserialized occupancy remains legal"),DYKCityTerritory::Validate(Reload->Profile,C));
	FTerritoryMemoryService Restored; Restored.Profile=Reload->Profile; Restored.Applied=Reload->AppliedCommandIds;
	const auto Before=Restored.Profile;
	TestTrue(TEXT("Replayed expansion command after reload is idempotent"),DYKCityTerritory::Unlock(Restored,Expand,{4,2},C));
	TestTrue(TEXT("Reload replay changes no resources or parcel"),TerritorySame(Restored.Profile,Before));
	TestTrue(TEXT("Reload never resets layout"),DYKCityTerritory::Initialize(Restored.Profile,C));
	TestTrue(TEXT("Reload initialization preserves exact saved state"),TerritorySame(Restored.Profile,Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKTerritoryDiskTest, "DYK.M201.DurableReloadAndWriteFailure", TerritoryTestFlags)
bool FDYKTerritoryDiskTest::RunTest(const FString& Parameters)
{
	const auto& C=DYKCityTerritory::DEV_PLACEHOLDER();
	TStrongObjectPtr<UGameInstance> Owner(NewObject<UGameInstance>());
	const FString Directory=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("M201/test-saves"),FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	const FString Prefix=FPaths::Combine(Directory,TEXT("profile"));
	auto OpenService=[&]()
	{
		auto* Result=NewObject<UDYKSaveSubsystem>(Owner.Get()); FDYKCityTerritoryTestAccess::Open(*Result,Prefix); return Result;
	};
	auto* Service=OpenService();
	if (!TestNotNull(TEXT("Real save subsystem creates a profile"),Service->GetProfileSnapshot())) return false;
	TestTrue(TEXT("Seed real durable city"),Service->ApplyProfileCommand(FGuid::NewGuid(),[&C](auto& P)
	{
		if (!TerritorySeed(P,C)) return false;
		TerritorySetBalance(P,TerritoryWood,100); return true;
	}));
	const auto Before=*Service->GetProfileSnapshot(); const auto Revision=Service->GetRevision();
	const FString Blocker=FPaths::Combine(Directory,TEXT("blocked-parent"));
	if (!TestTrue(TEXT("Create actual filesystem obstruction"),FFileHelper::SaveStringToFile(TEXT("file, not directory"),*Blocker))) return false;
	FDYKCityTerritoryTestAccess::SetPrefix(*Service,FPaths::Combine(Blocker,TEXT("profile")));
	const auto UnlockCommand=FGuid::NewGuid();
	TestFalse(TEXT("Expansion fails on actual durable-write failure"),DYKCityTerritory::Unlock(*Service,UnlockCommand,{4,2},C));
	TestTrue(TEXT("Real write failure leaves resources and parcels identical"),TerritorySame(*Service->GetProfileSnapshot(),Before));
	TestEqual(TEXT("Real write failure leaves revision unchanged"),Service->GetRevision(),Revision);
	const auto MoveCommand=FGuid::NewGuid(); const auto Sawmill=TerritoryInstance(Before,TEXT("CITY-SAWMILL"));
	TestFalse(TEXT("Placement fails on actual durable-write failure"),DYKCityTerritory::Move(*Service,MoveCommand,Sawmill,{30,20},1,C));
	TestTrue(TEXT("Real move write failure leaves placement identical"),TerritorySame(*Service->GetProfileSnapshot(),Before));
	FDYKCityTerritoryTestAccess::SetPrefix(*Service,Prefix);
	TestTrue(TEXT("Same failed expansion command can retry"),DYKCityTerritory::Unlock(*Service,UnlockCommand,{4,2},C));
	TestTrue(TEXT("Same failed move command can retry"),DYKCityTerritory::Move(*Service,MoveCommand,Sawmill,{42,22},3,C));
	const auto Confirmed=*Service->GetProfileSnapshot(); const auto ConfirmedRevision=Service->GetRevision();
	Service=OpenService();
	if (!TestNotNull(TEXT("New subsystem instance reloads durable profile"),Service->GetProfileSnapshot())) return false;
	TestTrue(TEXT("Real files preserve every profile field after reload"),TerritorySame(*Service->GetProfileSnapshot(),Confirmed));
	TestEqual(TEXT("Real files preserve revision"),Service->GetRevision(),ConfirmedRevision);
	TestTrue(TEXT("Real files preserve valid occupancy"),DYKCityTerritory::Validate(*Service->GetProfileSnapshot(),C));
	TestTrue(TEXT("Reloaded expansion receipt prevents duplicate payment"),DYKCityTerritory::Unlock(*Service,UnlockCommand,{4,2},C));
	TestTrue(TEXT("Reloaded movement receipt is idempotent"),DYKCityTerritory::Move(*Service,MoveCommand,Sawmill,{42,22},3,C));
	TestTrue(TEXT("Reloaded receipt replay changes nothing"),TerritorySame(*Service->GetProfileSnapshot(),Confirmed));
	TestEqual(TEXT("Reloaded receipt replay creates no revision"),Service->GetRevision(),ConfirmedRevision);
	TestTrue(TEXT("Both alternating durable files exist"),IFileManager::Get().FileExists(*(Prefix+TEXT("_A.sav"))) && IFileManager::Get().FileExists(*(Prefix+TEXT("_B.sav"))));
	return true;
}
#endif
