#pragma once

#include "DYKCityLayoutEditor.h"

// Transient road meshes derived from the mandatory placement perimeter; no economic connectivity rule.
struct DEFENDYOURKINGDOM_API FDYKCityRoadNetwork
{
	TSet<FIntPoint> Cells;
	TArray<FIntPoint> OrderedCells;
	bool Rebuild(const FDYKPlayerProfileSaveData& Profile, const FDYKCityTerritoryConfig& Config,
		const FDYKCityLayoutEditor* Editor = nullptr);
	TArray<FIntPoint> Neighbors(FIntPoint Cell) const;
};

struct FDYKCityPedestrianConfig
{
	// Replaceable visual tuning; social chance is the approved 5%, not tuning.
	int32 Population = 12;
	float CellsPerSecond = .65f;
	float ConversationSeconds = 2.5f;
	float NearbyCells = .25f;
	float UpdateSeconds = .05f;
};

struct FDYKCityPedestrian
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	FIntPoint Next = FIntPoint::ZeroValue;
	float Progress = 0;
	float ConversationLeft = 0;
	FVector2D Position() const { return FVector2D(Cell) + FVector2D(Next-Cell)*Progress; }
};

class DEFENDYOURKINGDOM_API FDYKCityPedestrians
{
public:
	FDYKCityPedestrianConfig DEV_PLACEHOLDER;
	TArray<FDYKCityPedestrian> Walkers;
	void Reconcile(const FDYKCityRoadNetwork& Roads, FRandomStream& Random);
	bool SpawnAt(const FDYKCityRoadNetwork& Roads, FIntPoint Cell, FRandomStream& Random);
	void Advance(const FDYKCityRoadNetwork& Roads, float Seconds, FRandomStream& Random);
	int32 SocialRolls() const { return Rolls; }
private:
	int32 Rolls = 0;
};
