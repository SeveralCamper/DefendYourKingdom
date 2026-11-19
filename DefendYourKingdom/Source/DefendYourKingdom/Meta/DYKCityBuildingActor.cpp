#include "DYKCityBuildingActor.h"
#include "DYKCityCatalog.h"
#include "Components/StaticMeshComponent.h"
#include "../UI/DYKVisuals.h"
#include "Components/SceneComponent.h"

ADYKCityBuildingActor::ADYKCityBuildingActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BuildingPosition")));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetRootComponent());
}

void ADYKCityBuildingActor::BeginPlay()
{
	Super::BeginPlay();
	const auto* Def = DYKCityCatalog::Find(BuildingId);
	if (!Def) { return; }
	DYKVisuals::Building(this,Body,BuildingId.Name);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}
