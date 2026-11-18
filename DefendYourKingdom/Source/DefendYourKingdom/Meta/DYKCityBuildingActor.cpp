#include "DYKCityBuildingActor.h"
#include "DYKCityCatalog.h"
#include "DYKCityWidgets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "../UI/DYKVisuals.h"
#include "Engine/StaticMesh.h"
#include "Components/SceneComponent.h"
#include "DYKCityGameMode.h"
#include "DYKCityWorkWidget.h"
#include "Engine/World.h"

ADYKCityBuildingActor::ADYKCityBuildingActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("BuildingPosition")));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetRootComponent());
	CollectionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("CollectionWidget"));
	CollectionWidget->SetupAttachment(GetRootComponent());
	CollectionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	CollectionWidget->SetWidgetClass(UDYKCityCollectWidget::StaticClass());
	CollectionWidget->SetDrawSize(FVector2D(240., 115.));
	CollectionWidget->SetPivot(FVector2D(0.5, 1.));
	CollectionWidget->SetVisibility(false);
}

void ADYKCityBuildingActor::BeginPlay()
{
	Super::BeginPlay();
	const auto* Def = DYKCityCatalog::Find(BuildingId);
	if (!Def) { return; }
	const float Height = DYKVisuals::Building(this,Body,BuildingId.Name);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->OnBeginCursorOver.AddDynamic(this,&ThisClass::HoverBegin);
	Body->OnEndCursorOver.AddDynamic(this,&ThisClass::HoverEnd);
	CollectionWidget->SetWidgetClass(DYKUI::ScreenClass(UDYKCityCollectWidget::StaticClass(),TEXT("WBP_Collect")));
	CollectionWidget->SetDrawSize(FVector2D(230,Def->Resource.IsValid() ? 122 : 60));
	CollectionWidget->SetRelativeLocation(FVector(0,0,Height+35));
	CollectionWidget->InitWidget();
	if (auto* UI = Cast<UDYKCityCollectWidget>(CollectionWidget->GetUserWidgetObject()))
	{
		UI->Configure(InstanceId, BuildingId);
	}
	CollectionWidget->SetVisibility(false);
}

void ADYKCityBuildingActor::SetHovered(bool Hovered)
{
	if(Hovered) for(TObjectIterator<UDYKCityWorkWidget> It;It;++It)
		if(It->GetWorld()==GetWorld() && It->IsInViewport() && It->GetWindowState().IsModal()) {Hovered=false; break;}
	bHovered=Hovered;
	const auto* City=GetWorld() ? GetWorld()->GetAuthGameMode<ADYKCityGameMode>() : nullptr;
	CollectionWidget->SetVisibility(bHovered && (!City || !City->GetLayoutEditor().IsActive()));
	if(bHovered) if(auto* UI=Cast<UDYKCityCollectWidget>(CollectionWidget->GetUserWidgetObject())) UI->Refresh();
}
void ADYKCityBuildingActor::HoverBegin(UPrimitiveComponent* Component) {SetHovered(true);}
void ADYKCityBuildingActor::HoverEnd(UPrimitiveComponent* Component) {SetHovered(false);}
