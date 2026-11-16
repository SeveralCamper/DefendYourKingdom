#include "DYKCityTerritoryActor.h"
#include "DYKCityTerritory.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/GameInstance.h"
#include "UObject/ConstructorHelpers.h"

ADYKCityTerritoryActor::ADYKCityTerritoryActor()
{
	PrimaryActorTick.bCanEverTick=false;
	Lines=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TerritoryLines"));
	SetRootComponent(Lines);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Lines->SetStaticMesh(Cube.Object);
	Lines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lines->SetCastShadow(false);
	// Extend the technical city's terrain to cover every plot and decorative margin.
	Ground=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerritoryGround"));
	Ground->SetupAttachment(Lines); Ground->SetStaticMesh(Cube.Object);
	Ground->SetRelativeLocation(FVector(2000.,2000.,-35.));
	Ground->SetRelativeScale3D(FVector(500.,500.,.1));
	Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grass(TEXT("/Game/DefendYourKingdom/Art/M_Grass.M_Grass"));
	if (Grass.Succeeded()) Ground->SetMaterial(0,Grass.Object);
}
void ADYKCityTerritoryActor::BeginPlay() { Super::BeginPlay(); Refresh(); }
void ADYKCityTerritoryActor::Refresh()
{
	Lines->ClearInstances();
	const auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto* P=Save ? Save->GetProfileSnapshot() : nullptr;
	if (!P) return;
	auto Line=[this](FVector A,FVector B,double Width)
	{
		FVector Center=(A+B)*.5; Center.Z=8.;
		FVector Scale(FMath::Max(FMath::Abs(B.X-A.X),Width)/100., FMath::Max(FMath::Abs(B.Y-A.Y),Width)/100., .025);
		Lines->AddInstance(FTransform(FRotator::ZeroRotator,Center,Scale));
	};
	for (const auto& Parcel : P->Parcels)
	{
		const FIntPoint Start=Parcel.Coordinate*10;
		for (int32 Offset=0; Offset<=10; ++Offset)
		{
			if (!Parcel.bIsUnlocked && Offset!=0 && Offset!=10) continue;
			const double Width=(Offset==0 || Offset==10) ? 12. : 3.;
			Line(DYKCityTerritory::CellCorner(Start+FIntPoint(Offset,0)),DYKCityTerritory::CellCorner(Start+FIntPoint(Offset,10)),Width);
			Line(DYKCityTerritory::CellCorner(Start+FIntPoint(0,Offset)),DYKCityTerritory::CellCorner(Start+FIntPoint(10,Offset)),Width);
		}
	}
}
