#include "DYKCityBuildingActor.h"
#include "DYKCityCatalog.h"
#include "DYKCityWidgets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"

ADYKCityBuildingActor::ADYKCityBuildingActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Body->SetStaticMesh(Cube.Object);
	CollectionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("CollectionWidget"));
	CollectionWidget->SetupAttachment(Body);
	CollectionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	CollectionWidget->SetWidgetClass(UDYKCityCollectWidget::StaticClass());
	CollectionWidget->SetDrawSize(FVector2D(240., 115.));
	CollectionWidget->SetPivot(FVector2D(0.5, 1.));
}

void ADYKCityBuildingActor::BeginPlay()
{
	Super::BeginPlay();
	const auto* Def = DYKCityCatalog::Find(BuildingId);
	if (!Def) { return; }
	// Replaceable blockout visuals until the M1.10 art pass.
	const bool Castle = !Def->Resource.IsValid();
	Body->SetWorldScale3D(Castle ? FVector(5., 5., 6.) : FVector(3., 3., 2.5));
	Body->SetWorldLocation(GetActorLocation() + FVector(0., 0., Castle ? 300. : 125.));
	CollectionWidget->SetWorldScale3D(FVector::OneVector);
	CollectionWidget->SetWorldLocation(Body->GetComponentLocation() + FVector(0., 0., Castle ? 350. : 180.));
	CollectionWidget->InitWidget();
	if (auto* UI = Cast<UDYKCityCollectWidget>(CollectionWidget->GetUserWidgetObject()))
	{
		UI->Configure(InstanceId, BuildingId);
	}
}
