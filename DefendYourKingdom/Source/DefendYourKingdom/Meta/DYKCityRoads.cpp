#include "DYKCityRoads.h"

namespace
{
	const FIntPoint Directions[] = {{1,0},{0,1},{-1,0},{0,-1}};
	FIntPoint ChooseNext(const FDYKCityRoadNetwork& Roads, FIntPoint Cell, FRandomStream& Random,
		FIntPoint PreviousDirection=FIntPoint::ZeroValue)
	{
		auto Options = Roads.Neighbors(Cell);
		const FIntPoint Straight=Cell+PreviousDirection;
		if (PreviousDirection!=FIntPoint::ZeroValue && Options.Contains(Straight))
		{
			// Approved directional persistence. A sole straight continuation needs no roll.
			if (Options.Num()==1 || Random.FRand()<.8f) return Straight;
			Options.RemoveSingle(Straight);
		}
		else if (PreviousDirection!=FIntPoint::ZeroValue && Options.Num()>1)
		{
			// Follow a forced corner rather than reversing on a road that continues around it.
			Options.RemoveSingle(Cell-PreviousDirection);
		}
		return Options.IsEmpty() ? Cell : Options[Random.RandRange(0,Options.Num()-1)];
	}
}

bool FDYKCityRoadNetwork::Rebuild(const FDYKPlayerProfileSaveData& P, const FDYKCityTerritoryConfig& C,
	const FDYKCityLayoutEditor* Editor)
{
	Cells.Reset(); OrderedCells.Reset();
	TArray<FDYKBuildingSaveData> Placed;
	for (int32 I=0; I<P.Buildings.Num(); ++I)
	{
		auto B=P.Buildings[I];
		if (Editor && Editor->IsActive())
		{
			const auto* S=P.CityEconomy.Instances.FindByPredicate([I](const auto& Row){ return Row.BuildingIndex==I; });
			const auto* E=S ? Editor->Entries().FindByPredicate([S](const auto& Row){ return Row.InstanceId==S->InstanceId; }) : nullptr;
			if (!E) return false;
			if (!E->bPlaced) continue;
			B.CellOrigin=E->Origin; B.RotationQuarterTurns=E->QuarterTurns;
		}
		Placed.Add(B);
	}
	// No clipping or footprint subtraction: a legal placement always owns its full ring.
	if (!DYKCityTerritory::ValidateLayout(P,Placed,C)) return false;
	for (const auto& B:Placed)
	{
		TArray<FIntPoint> Body,Perimeter;
		if (!DYKCityTerritory::PlacementCells(B,C,Body,Perimeter)) return false;
		Cells.Append(Perimeter);
	}
	OrderedCells=Cells.Array();
	OrderedCells.Sort([](FIntPoint A,FIntPoint B){ return A.Y==B.Y ? A.X<B.X : A.Y<B.Y; });
	return true;
}

TArray<FIntPoint> FDYKCityRoadNetwork::Neighbors(FIntPoint Cell) const
{
	TArray<FIntPoint> Result;
	if (Cells.Contains(Cell)) for (const auto D:Directions) if (Cells.Contains(Cell+D)) Result.Add(Cell+D);
	return Result;
}

bool FDYKCityPedestrians::SpawnAt(const FDYKCityRoadNetwork& Roads,FIntPoint Cell,FRandomStream& Random)
{
	if (!Roads.Cells.Contains(Cell) || Walkers.Num()>=FMath::Clamp(DEV_PLACEHOLDER.Population,0,64)) return false;
	FDYKCityPedestrian New;
	New.Cell=Cell; New.Next=ChooseNext(Roads,Cell,Random);
	New.Progress=New.Next==Cell ? 0 : Random.FRandRange(.25f,.75f);
	for (auto& Other:Walkers)
	{
		if (New.Next==Cell || Other.ConversationLeft>0 || FVector2D::Distance(Other.Position(),New.Position())>DEV_PLACEHOLDER.NearbyCells) continue;
		++Rolls;
		// Exactly one trial for this spawn, never repeated on contact or on a timer.
		if (Random.FRand()<.05f)
		{
			// Meet inside an available road edge: opposite departures even at a corner/dead end.
			Other.Cell=New.Next; Other.Next=New.Cell;
			New.Progress=Other.Progress=.7f; // Leave space between models while facing one another.
			New.ConversationLeft=Other.ConversationLeft=FMath::Max(.1f,DEV_PLACEHOLDER.ConversationSeconds);
		}
		break;
	}
	Walkers.Add(New);
	return true;
}

void FDYKCityPedestrians::Reconcile(const FDYKCityRoadNetwork& Roads,FRandomStream& Random)
{
	if (Roads.OrderedCells.IsEmpty()) { Walkers.Reset(); return; }
	for (auto& Walker:Walkers)
	{
		if (Roads.Cells.Contains(Walker.Cell) && Roads.Cells.Contains(Walker.Next)) continue;
		Walker={}; Walker.Cell=Roads.OrderedCells[Random.RandRange(0,Roads.OrderedCells.Num()-1)];
		Walker.Next=ChooseNext(Roads,Walker.Cell,Random);
	}
	const int32 Population=FMath::Clamp(DEV_PLACEHOLDER.Population,0,64);
	if (Walkers.Num()>Population) Walkers.SetNum(Population);
	while (Walkers.Num()<Population) SpawnAt(Roads,Roads.OrderedCells[Random.RandRange(0,Roads.OrderedCells.Num()-1)],Random);
}

void FDYKCityPedestrians::Advance(const FDYKCityRoadNetwork& Roads,float Seconds,FRandomStream& Random)
{
	if (!FMath::IsFinite(Seconds) || Seconds<=0) return;
	for (auto& W:Walkers)
	{
		float Remaining=FMath::Min(Seconds,1.f); // Cosmetic simulation has no offline catch-up.
		if (W.ConversationLeft>0)
		{
			const float Pause=FMath::Min(Remaining,W.ConversationLeft);
			W.ConversationLeft-=Pause; Remaining-=Pause;
		}
		W.Progress+=Remaining*FMath::Clamp(DEV_PLACEHOLDER.CellsPerSecond,.01f,10.f);
		while (W.Progress>=1)
		{
			const auto Direction=W.Next-W.Cell;
			W.Progress-=1; W.Cell=W.Next; W.Next=ChooseNext(Roads,W.Cell,Random,Direction);
		}
	}
}
