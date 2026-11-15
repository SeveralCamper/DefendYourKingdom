#include "DYKEnemyRoute.h"
#include "Components/SplineComponent.h"

ADYKEnemyRoute::ADYKEnemyRoute()
{
	PrimaryActorTick.bCanEverTick = false;
	Path = CreateDefaultSubobject<USplineComponent>(TEXT("AuthoredPath"));
	SetRootComponent(Path);
	Path->SetClosedLoop(false);
}

bool ADYKEnemyRoute::IsUsable() const
{
	if (RouteId.IsNone() || ProtectedObjectId.IsNone() || Path->IsClosedLoop() ||
		Path->GetNumberOfSplinePoints() < 2 || !FMath::IsFinite(Path->GetSplineLength()) || Path->GetSplineLength() <= 0) return false;
	for (int32 Index = 0; Index < Path->GetNumberOfSplinePoints(); ++Index)
	{
		const FVector Point = Path->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World);
		if (Point.ContainsNaN() || (Index > 0 && Point.Equals(Path->GetLocationAtSplinePoint(Index - 1, ESplineCoordinateSpace::World)))) return false;
	}
	return true;
}
