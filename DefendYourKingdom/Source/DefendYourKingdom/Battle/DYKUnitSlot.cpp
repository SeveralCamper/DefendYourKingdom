#include "DYKUnitSlot.h"
#include "DYKDefenderActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ADYKUnitSlot::ADYKUnitSlot()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AuthoredPosition")));
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UnitSlotMarker"));
	Marker->SetupAttachment(GetRootComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Marker->SetStaticMesh(Shape.Object);
	Marker->SetRelativeScale3D(FVector(1.2, 1.2, 0.1));
	Marker->SetRelativeLocation(FVector(0, 0, -60));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	auto* Selection = CreateDefaultSubobject<UBoxComponent>(TEXT("SlotAndOccupantSelection"));
	Selection->SetupAttachment(GetRootComponent());
	Selection->SetBoxExtent(FVector(65, 65, 65));
	Selection->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Selection->SetCollisionResponseToAllChannels(ECR_Ignore);
	Selection->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ADYKUnitSlot::UpdateMarker()
{
	Marker->SetVisibility(!Occupant);
}
