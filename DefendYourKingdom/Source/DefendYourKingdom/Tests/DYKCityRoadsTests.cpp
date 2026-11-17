#include "../Meta/DYKCityRoads.h"
#include "../Meta/DYKCityEconomy.h"
#include "../Meta/DYKCityBuildingRules.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
	constexpr auto RoadFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
	FDYKPlayerProfileSaveData RoadSeed()
	{
		FDYKPlayerProfileSaveData P;
		DYKCityEconomy::Initialize(P,1794700000);
		DYKCityTerritory::Initialize(P,DYKCityTerritory::DEV_PLACEHOLDER());
		return P;
	}
	bool RoadSame(const FDYKPlayerProfileSaveData& A,const FDYKPlayerProfileSaveData& B)
	{ return FDYKPlayerProfileSaveData::StaticStruct()->CompareScriptStruct(&A,&B,0); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKRoadPerimeterTest,"DYK.M204.P12T20.PerimeterUnion",RoadFlags)
bool FDYKRoadPerimeterTest::RunTest(const FString& Parameters)
{
	auto P=RoadSeed(); const auto Before=P;
	FDYKCityRoadNetwork Roads;
	TestTrue(TEXT("Initial roads"),Roads.Rebuild(P,DYKCityTerritory::DEV_PLACEHOLDER()));
	TestTrue(TEXT("Derivation preserves every profile field"),RoadSame(P,Before));
	TSet<FIntPoint> Occupied;
	for (const auto& B:P.Buildings)
	{
		TArray<FIntPoint> Shape;
		TestTrue(TEXT("Valid footprint"),DYKCityTerritory::OccupiedCells(B,DYKCityTerritory::DEV_PLACEHOLDER(),Shape));
		for (const auto Cell:Shape) Occupied.Add(Cell);
	}
	for (int32 Y=0;Y<50;++Y) for (int32 X=0;X<50;++X)
	{
		const FIntPoint Cell(X,Y); bool Near=false;
		for (const auto Taken:Occupied) Near|=FMath::Abs(Taken.X-X)<=1 && FMath::Abs(Taken.Y-Y)<=1;
		const bool Expected=Near && !Occupied.Contains(Cell) && DYKCityTerritory::IsUnlocked(P,{X/10,Y/10});
		if (Roads.Cells.Contains(Cell)!=Expected) { AddError(FString::Printf(TEXT("Perimeter mismatch %d,%d"),X,Y)); return false; }
	}
	// Exact shared ring between two adjacent 2x2 houses: 12+12-4 = 20 cells.
	P.Buildings.SetNum(2);
	for (int32 I=0;I<2;++I) { P.Buildings[I].BuildingId=FDYKBuildingId(TEXT("CITY-HOUSE")).ToContentId(); P.Buildings[I].CellOrigin={22+I*3,22}; P.Buildings[I].RotationQuarterTurns=0; }
	TestTrue(TEXT("Two adjacent perimeters"),Roads.Rebuild(P,DYKCityTerritory::DEV_PLACEHOLDER()));
	TestEqual(TEXT("Four shared cells rendered once"),Roads.Cells.Num(),20);
	const auto Order=Roads.OrderedCells;
	Roads.Rebuild(P,DYKCityTerritory::DEV_PLACEHOLDER());
	TestTrue(TEXT("Stable deduplicated rebuild"),Roads.OrderedCells==Order);
	TestEqual(TEXT("No duplicate render rows"),Roads.OrderedCells.Num(),Roads.Cells.Num());
	P.Buildings[1].CellOrigin={24,22};
	TestFalse(TEXT("Body cannot consume another building's mandatory road"),Roads.Rebuild(P,DYKCityTerritory::DEV_PLACEHOLDER()));
	TestTrue(TEXT("Illegal layout never produces clipped roads"),Roads.Cells.IsEmpty());
	P.Buildings[0].BuildingId=FDYKBuildingId(TEXT("UNKNOWN")).ToContentId();
	TestFalse(TEXT("Unknown footprint fails closed"),Roads.Rebuild(P,DYKCityTerritory::DEV_PLACEHOLDER()));
	TestTrue(TEXT("No partial roads after invalid input"),Roads.Cells.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKRoadClearanceTest,"DYK.M204.P12T20.MandatoryPerimeterGeometry",RoadFlags)
bool FDYKRoadClearanceTest::RunTest(const FString& Parameters)
{
	auto P=RoadSeed(); const auto& C=DYKCityTerritory::DEV_PLACEHOLDER();
	const auto& Castle=*P.Buildings.FindByPredicate([](const auto& B){return B.BuildingId.Name==TEXT("CITY-CASTLE");});
	TArray<FIntPoint> Body,Ring;
	TestTrue(TEXT("Castle body and mandatory ring available"),DYKCityTerritory::PlacementCells(Castle,C,Body,Ring));
	TestEqual(TEXT("Approved castle body remains 10x10"),Body.Num(),100);
	TestEqual(TEXT("Full castle ring includes every corner"),Ring.Num(),44);
	auto TouchEdge=Castle; TouchEdge.CellOrigin={10,10};
	TestTrue(TEXT("Body on old parcel edge still inside map"),DYKCityTerritory::OccupiedCells(TouchEdge,C,Body));
	TestFalse(TEXT("Closed land under any road segment forbids placement"),DYKCityTerritory::ValidateLayout(P,MakeArrayView(&TouchEdge,1),C));
	FDYKBuildingSaveData Decor; Decor.BuildingId=FDYKBuildingId(TEXT("CITY-DECOR-DEV_PLACEHOLDER")).ToContentId(); Decor.CellOrigin={0,20};
	TestTrue(TEXT("Edge decoration body itself fits"),DYKCityTerritory::OccupiedCells(Decor,C,Body));
	TestFalse(TEXT("Road outside map is never clipped"),DYKCityTerritory::PlacementCells(Decor,C,Body,Ring));
	TestTrue(TEXT("Failed ring result is atomic"),Body.IsEmpty() && Ring.IsEmpty());
	FDYKCityLayoutEditor Editor;
	TestTrue(TEXT("Begin ring-safe editor"),Editor.Begin(P));
	const auto Saw=P.CityEconomy.Instances[1].InstanceId;
	TestTrue(TEXT("Complete initial draft can finish"),Editor.CanFinish());
	TestFalse(TEXT("Initial draft has no external conflict"),Editor.HasConflict(P));
	TestFalse(TEXT("Stored-only rotation cannot change a placed body"),Editor.RotateStored(Saw,1));
	TestFalse(TEXT("Editor body cannot consume castle road row"),Editor.CanPlace(Saw,{21,12},0));
	TestFalse(TEXT("Editor body fits but its road crosses into closed land"),Editor.CanPlace(Saw,{35,28},0));
	TestTrue(TEXT("Buffer building"),Editor.Store(Saw));
	TestTrue(TEXT("Buffered rotation is retained in draft ownership row"),Editor.RotateStored(Saw,3));
	const auto* Buffered=Editor.Entries().FindByPredicate([Saw](const auto& E){return E.InstanceId==Saw;});
	TestTrue(TEXT("Stored turns and buffer membership retained"),Buffered && Buffered->QuarterTurns==3 && !Buffered->bPlaced);
	for (int32 Bad:{-1,4,MAX_int32,MIN_int32}) TestFalse(TEXT("Invalid buffered rotation rejected"),Editor.RotateStored(Saw,Bad));
	TestFalse(TEXT("Unknown ID cannot rotate a buffered building"),Editor.RotateStored(FGuid::NewGuid(),0));
	TestTrue(TEXT("Rejected buffered rotation preserves last approved turns"),Buffered && Buffered->QuarterTurns==3 && !Buffered->bPlaced);
	TestFalse(TEXT("Missing ownership blocks finish"),Editor.CanFinish());
	TestTrue(TEXT("Place with full clearance"),Editor.Place(Saw,{30,20},1));
	TestTrue(TEXT("Complete ring-safe draft can finish"),Editor.CanFinish());
	FDYKCityBuildingWorkRequest R;
	R.CommandId=FGuid::NewGuid(); R.InstanceId=FGuid::NewGuid(); R.BuildingId=FDYKBuildingId(TEXT("CITY-HOUSE"));
	FDYKCityBuildingWorkPlan Plan;
	R.Origin={24,22};
	TestFalse(TEXT("Build body cannot consume existing house road"),DYKCityBuildingRules::Prepare(P,R,C,DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan));
	R.Origin={25,22};
	TestTrue(TEXT("Build permits a shared road row"),DYKCityBuildingRules::Prepare(P,R,C,DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan));
	R.Origin={37,28};
	TestFalse(TEXT("Build rejects closed land under road although body fits"),DYKCityBuildingRules::Prepare(P,R,C,DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKRoadEditorTest,"DYK.M204.P12T21.EditorAndEconomy",RoadFlags)
bool FDYKRoadEditorTest::RunTest(const FString& Parameters)
{
	auto P=RoadSeed(); const auto Before=P;
	const auto& C=DYKCityTerritory::DEV_PLACEHOLDER();
	FDYKCityLayoutEditor Editor; FDYKCityRoadNetwork Roads;
	Roads.Rebuild(P,C); const auto Initial=Roads.OrderedCells;
	TestTrue(TEXT("Begin draft"),Editor.Begin(P));
	const auto Saw=P.CityEconomy.Instances[1].InstanceId;
	TestTrue(TEXT("Remove building"),Editor.Store(Saw)); Roads.Rebuild(P,C,&Editor);
	TestTrue(TEXT("Roads change immediately with buffer"),Roads.OrderedCells!=Initial);
	TestTrue(TEXT("Move and rotate"),Editor.Place(Saw,{30,20},1)); Roads.Rebuild(P,C,&Editor);
	TestTrue(TEXT("New rotated ring"),Roads.Cells.Contains({29,20}));
	TestFalse(TEXT("Old isolated sawmill ring removed"),Roads.Cells.Contains({25,12}));
	TestTrue(TEXT("Shared castle perimeter remains after sawmill moves"),Roads.Cells.Contains({21,12}));
	Editor.Cancel(); Roads.Rebuild(P,C,&Editor);
	TestTrue(TEXT("Cancel restores exact original network"),Roads.OrderedCells==Initial);
	TestTrue(TEXT("All economy/progress fields preserved"),RoadSame(P,Before));
	// Legality/economy never consult the derived road network.
	Roads.Cells.Reset(); Roads.OrderedCells.Reset();
	TestTrue(TEXT("No roads still valid city"),DYKCityTerritory::Validate(P,C));
	TestTrue(TEXT("Production still valid"),DYKCityEconomy::Validate(P));
	TestTrue(TEXT("Roadless edit allowed"),Editor.Begin(P) && Editor.Place(Saw,{30,20},1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKPedestrianTest,"DYK.M204.P12T22.WalkingAndSpawnSocial",RoadFlags)
bool FDYKPedestrianTest::RunTest(const FString& Parameters)
{
	FDYKCityRoadNetwork Roads;
	for (int32 X=0;X<8;++X) { Roads.Cells.Add({X,0}); Roads.OrderedCells.Add({X,0}); }
	FRandomStream Random(51); FDYKCityPedestrians People; People.Reconcile(Roads,Random);
	TestEqual(TEXT("Configured decorative population without houses"),People.Walkers.Num(),12);
	const auto Start=People.Walkers[0].Position(); const auto Rolls=People.SocialRolls();
	for (int32 I=0;I<200;++I)
	{
		People.Advance(Roads,.1f,Random);
		for (const auto& W:People.Walkers)
		{
			TestTrue(TEXT("Walk remains on graph"),Roads.Cells.Contains(W.Cell) && Roads.Cells.Contains(W.Next));
			TestTrue(TEXT("Only adjacent road edges"),FMath::Abs(W.Cell.X-W.Next.X)+FMath::Abs(W.Cell.Y-W.Next.Y)<=1);
			TestTrue(TEXT("Position along road"),W.Progress>=0 && W.Progress<1);
		}
	}
	TestTrue(TEXT("NPC visibly moves"),!People.Walkers[0].Position().Equals(Start));
	TestEqual(TEXT("No repeated social rolls on movement or contact"),People.SocialRolls(),Rolls);
	FDYKCityPedestrians EmptyPeople;
	TestFalse(TEXT("Cannot spawn off road despite free population capacity"),EmptyPeople.SpawnAt(Roads,{0,1},Random));
	TestTrue(TEXT("Invalid spawn does not add a resident or roll social chance"),EmptyPeople.Walkers.IsEmpty() && EmptyPeople.SocialRolls()==0);
	TestTrue(TEXT("Spawn first resident"),EmptyPeople.SpawnAt(Roads,{0,0},Random));
	TestTrue(TEXT("Spawn far resident"),EmptyPeople.SpawnAt(Roads,{7,0},Random));
	TestEqual(TEXT("Far appearance has no social trial"),EmptyPeople.SocialRolls(),0);
	const auto BeforeNegativeTime=EmptyPeople.Walkers[0].Position();
	EmptyPeople.Advance(Roads,-1,Random);
	TestTrue(TEXT("Negative time cannot move decoration"),EmptyPeople.Walkers[0].Position()==BeforeNegativeTime);
	// Test the actual 5% gate across deterministic independent seeds, not a test override.
	int32 Conversations=0;
	for (int32 Seed=0;Seed<2000;++Seed)
	{
		FRandomStream Trial(Seed); FDYKCityPedestrians Pair;
		Pair.DEV_PLACEHOLDER.NearbyCells=2; // Guarantee a nearby pair to exercise each seed's gate.
		Pair.SpawnAt(Roads,{3,0},Trial); Pair.SpawnAt(Roads,{3,0},Trial);
		TestEqual(TEXT("One trial for close spawn"),Pair.SocialRolls(),1);
		if (Pair.Walkers[0].ConversationLeft<=0) continue;
		++Conversations;
		const auto A=Pair.Walkers[0],B=Pair.Walkers[1];
		TestTrue(TEXT("Conversation leaves in opposite directions"),A.Next-A.Cell==B.Cell-B.Next);
		Pair.Advance(Roads,1,Trial);
		TestTrue(TEXT("Both stop to talk"),Pair.Walkers[0].Progress==A.Progress && Pair.Walkers[1].Progress==B.Progress);
		for (int32 I=0;I<3;++I) Pair.Advance(Roads,1,Trial);
		TestTrue(TEXT("Conversation ends and both leave"),Pair.Walkers[0].ConversationLeft==0 && Pair.Walkers[0].Position()!=A.Position() && Pair.Walkers[1].Position()!=B.Position());
		TestEqual(TEXT("Social timer never rerolls"),Pair.SocialRolls(),1);
	}
	TestTrue(TEXT("5% statistical gate within deterministic bounds"),Conversations>60 && Conversations<140);
	FDYKCityRoadNetwork DeadEnd;
	DeadEnd.Cells={{0,0},{1,0}}; DeadEnd.OrderedCells={{0,0},{1,0}};
	bool SawDeadEndSocial=false;
	for (int32 Seed=0;Seed<2000 && !SawDeadEndSocial;++Seed)
	{
		FRandomStream Trial(Seed); FDYKCityPedestrians Pair;
		Pair.DEV_PLACEHOLDER.NearbyCells=2;
		Pair.SpawnAt(DeadEnd,{0,0},Trial); Pair.SpawnAt(DeadEnd,{0,0},Trial);
		if (Pair.Walkers[0].ConversationLeft<=0) continue;
		SawDeadEndSocial=true;
		const auto& A=Pair.Walkers[0]; const auto& B=Pair.Walkers[1];
		TestTrue(TEXT("Dead-end social can leave in two opposite directions within the only edge"),A.Next-A.Cell==B.Cell-B.Next && A.Cell!=A.Next);
		TestTrue(TEXT("Talking residents have separate positions"),FVector2D::Distance(A.Position(),B.Position())>.3);
	}
	TestTrue(TEXT("Social gate also works at dead ends"),SawDeadEndSocial);
	Roads.Cells.Reset(); Roads.OrderedCells.Reset(); People.Reconcile(Roads,Random);
	TestTrue(TEXT("No roads removes residents"),People.Walkers.IsEmpty());
	Roads.Cells.Add({7,7}); Roads.OrderedCells.Add({7,7}); People.Reconcile(Roads,Random);
	for (const auto& W:People.Walkers) TestTrue(TEXT("Disconnected single tile safe"),W.Cell==FIntPoint(7,7) && W.Next==W.Cell);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKPedestrianDirectionTest,"DYK.M204.P12T22.DirectionPersistence80Percent",RoadFlags)
bool FDYKPedestrianDirectionTest::RunTest(const FString& Parameters)
{
	FDYKCityRoadNetwork Cross;
	Cross.Cells={{2,3},{3,3},{4,3},{3,2},{3,4}}; Cross.OrderedCells=Cross.Cells.Array();
	int32 Straight=0,Other=0;
	for (int32 Seed=0;Seed<10000;++Seed)
	{
		FRandomStream Random(Seed); FDYKCityPedestrians People;
		FDYKCityPedestrian W; W.Cell={2,3}; W.Next={3,3}; W.Progress=.95f;
		People.Walkers.Add(W); People.Advance(Cross,.2f,Random);
		const auto& Moved=People.Walkers[0];
		TestTrue(TEXT("Direction chosen only after reaching road node"),Moved.Cell==FIntPoint(3,3));
		if (Moved.Next==FIntPoint(4,3)) ++Straight; else ++Other;
		TestTrue(TEXT("80/20 decision stays on available road"),Cross.Neighbors(Moved.Cell).Contains(Moved.Next));
		TestEqual(TEXT("Movement never rolls a new social event"),People.SocialRolls(),0);
	}
	TestTrue(TEXT("Actual directional gate remains statistically near approved 80%"),Straight>7600 && Straight<8400 && Other==10000-Straight);
	FDYKCityRoadNetwork Corner;
	Corner.Cells={{0,0},{1,0},{1,1}}; Corner.OrderedCells=Corner.Cells.Array();
	for (int32 Seed=0;Seed<100;++Seed)
	{
		FRandomStream Random(Seed); FDYKCityPedestrians People;
		FDYKCityPedestrian W; W.Cell={0,0}; W.Next={1,0}; W.Progress=.95f;
		People.Walkers.Add(W); People.Advance(Corner,.2f,Random);
		TestTrue(TEXT("Blocked straight direction follows the available corner without random reversal"),People.Walkers[0].Next==FIntPoint(1,1));
	}
	FDYKCityRoadNetwork DeadEnd;
	DeadEnd.Cells={{0,0},{1,0}}; DeadEnd.OrderedCells=DeadEnd.Cells.Array();
	for (int32 Seed=0;Seed<100;++Seed)
	{
		FRandomStream Random(Seed); FDYKCityPedestrians People;
		FDYKCityPedestrian W; W.Cell={0,0}; W.Next={1,0}; W.Progress=.95f;
		People.Walkers.Add(W); People.Advance(DeadEnd,.2f,Random);
		TestTrue(TEXT("A true dead end permits the only return edge"),People.Walkers[0].Next==FIntPoint(0,0));
	}
	FDYKCityRoadNetwork SoleStraight;
	SoleStraight.Cells={{1,0},{2,0}}; SoleStraight.OrderedCells=SoleStraight.Cells.Array();
	for (int32 Seed=0;Seed<100;++Seed)
	{
		FRandomStream Random(Seed); FDYKCityPedestrians People;
		FDYKCityPedestrian W; W.Cell={0,0}; W.Next={1,0}; W.Progress=.95f;
		People.Walkers.Add(W); People.Advance(SoleStraight,.2f,Random);
		TestTrue(TEXT("A sole straight option is always followed, never replaced with staying put"),People.Walkers[0].Next==FIntPoint(2,0));
	}
	return true;
}
#endif
