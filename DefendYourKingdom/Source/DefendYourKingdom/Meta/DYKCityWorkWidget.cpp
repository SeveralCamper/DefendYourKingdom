#include "DYKCityWorkWidget.h"
#include "DYKCityWork.h"
#include "DYKCityEconomy.h"
#include "DYKCityGameMode.h"
#include "DYKCityEditorWidget.h"
#include "DYKCityTerritoryWidget.h"
#include "DYKCityBuildingPreviewActor.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

TSharedRef<SWidget> UDYKCityWorkWidget::RebuildWidget()
{
	if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if(!WidgetTree->RootWidget)
	{
		auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* Border=DYKUI::Panel(WidgetTree,this); auto* WorkSlot=Canvas->AddChildToCanvas(Border);
		WorkSlot->SetPosition(FVector2D(16,115)); WorkSlot->SetAutoSize(true);
		auto* Width=WidgetTree->ConstructWidget<USizeBox>(); Width->SetWidthOverride(320); Border->AddChild(Width);
		auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Width->AddChild(Box);
		auto* Toggle=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKWork","Development","Развитие города"));
		Toggle->OnClicked.AddDynamic(this,&ThisClass::ToggleClicked); Box->AddChild(Toggle);
		Menu=WidgetTree->ConstructWidget<UVerticalBox>(); Box->AddChild(Menu);
		Details=DYKUI::Text(WidgetTree,FText::GetEmpty(),15); Details->SetAutoWrapText(true); Menu->AddChild(Details);
		auto* Previous=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKWork","Previous","◀ Предыдущее здание"));
		Previous->OnClicked.AddDynamic(this,&ThisClass::PreviousClicked); Menu->AddChild(Previous);
		auto* Next=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKWork","Next","Следующее здание ▶"));
		Next->OnClicked.AddDynamic(this,&ThisClass::NextClicked); Menu->AddChild(Next);
		BuildButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKWork","Build","Выбрать место строительства"));
		BuildButton->OnClicked.AddDynamic(this,&ThisClass::BuildClicked); Menu->AddChild(BuildButton);
		UpgradeButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKWork","Upgrade","Улучшить выбранное на карте"));
		UpgradeButton->OnClicked.AddDynamic(this,&ThisClass::UpgradeClicked); Menu->AddChild(UpgradeButton);
		CancelWorkButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKWork","Cancel","Отменить работу выбранного здания"));
		CancelWorkButton->OnClicked.AddDynamic(this,&ThisClass::CancelClicked); Menu->AddChild(CancelWorkButton);
	}
	Refresh(); return Super::RebuildWidget();
}

void UDYKCityWorkWidget::NativeConstruct()
{
	Super::NativeConstruct(); GetWorld()->GetTimerManager().SetTimer(WorkTimer,this,&ThisClass::Refresh,1.f,true); Refresh();
}
void UDYKCityWorkWidget::NativeDestruct()
{
	if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(WorkTimer);
	if(SitePreview) SitePreview->Destroy(); SitePreview=nullptr;
	Super::NativeDestruct();
}
void UDYKCityWorkWidget::Refresh()
{
	if(!Menu) return;
	Menu->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	auto* City=GetWorld() ? GetWorld()->GetAuthGameMode<ADYKCityGameMode>() : nullptr;
	auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	if(!City || !Save) return;
	const int64 Revision=Save->GetRevision();
	if(!DYKCityWork::Update(*Save,FDateTime::UtcNow().ToUnixTimestamp())) WorkMessage=TEXT("Не удалось сохранить прогресс. Повторите попытку.");
	if(Revision!=Save->GetRevision()) City->SyncLayoutVisuals();
	const auto* P=Save->GetProfileSnapshot(); if(!P) return;
	const auto Catalog=DYKCityCatalog::Buildings(); CatalogIndex=FMath::Clamp(CatalogIndex,0,Catalog.Num()-1);
	const auto& Def=Catalog[CatalogIndex];
	FString Text=FString::Printf(TEXT("Строители: %d\nКаталог: %s"),DYKCityWork::Builders(*P),Def.DisplayName);
	const auto& Rule=DYKCityBuildingRules::DEV_PLACEHOLDER().Buildings.FindChecked(Def.Id.GetName());
	const auto& Step=Rule.Levels[0];
	Text+=FString::Printf(TEXT("\nТребуется замок ур. %d · %lld с"),Step.RequiredCastleLevel,Step.DurationSeconds);
	for(const auto& Cost:Step.Costs) Text+=FString::Printf(TEXT("\n%s: %lld"),DYKCityCatalog::ResourceName(FDYKResourceId(Cost.ResourceId.Name)),Cost.Amount);
	for(const auto& W:P->CityEconomy.Work)
	{
		const auto* S=P->CityEconomy.Instances.FindByPredicate([&W](const auto& Row){return Row.InstanceId==W.InstanceId;});
		const auto* D=S ? DYKCityCatalog::Find(P->Buildings[S->BuildingIndex].BuildingId) : nullptr;
		Text+=FString::Printf(TEXT("\n%s: %s"),D ? D->DisplayName : TEXT(""),W.StartedAtUnix>0 ? *FString::Printf(TEXT("осталось %lld с"),FMath::Max<int64>(0,W.CompletesAtUnix-FDateTime::UtcNow().ToUnixTimestamp())) : TEXT("в очереди, не оплачено"));
	}
	for(const auto& R:P->CityEconomy.PendingRefunds) Text+=FString::Printf(TEXT("\nВозврат ожидает места: %s %lld"),DYKCityCatalog::ResourceName(FDYKResourceId(R.Resource)),R.Amount);
	if(!City->GetConstructionType().IsNone()) Text+=TEXT("\nВыберите свободное место на земле. Повторное нажатие отменяет выбор.");
	if(!WorkMessage.IsEmpty()) Text+=TEXT("\n")+WorkMessage;
	Details->SetText(FText::FromString(Text));
	const bool Editing=City->GetLayoutEditor().IsActive();
	BuildButton->SetIsEnabled(!Editing);
	const FGuid Selected=City->GetSelectedBuilding();
	const auto* S=P->CityEconomy.Instances.FindByPredicate([Selected](const auto& Row){return Row.InstanceId==Selected;});
	FDYKCityBuildingWorkRequest R; R.CommandId=FGuid::NewGuid(); R.InstanceId=Selected; R.Kind=EDYKCityBuildingWorkKind::Upgrade;
	if(S) { const auto& B=P->Buildings[S->BuildingIndex]; R.BuildingId=FDYKBuildingId(B.BuildingId.Name); R.ExpectedLevel=B.Level; }
	FDYKCityBuildingWorkPlan Plan;
	const bool HasWork=P->CityEconomy.Work.ContainsByPredicate([Selected](const auto& W){return W.InstanceId==Selected;});
	UpgradeButton->SetIsEnabled(!Editing && S && !HasWork && DYKCityBuildingRules::Prepare(*P,R,DYKCityTerritory::DEV_PLACEHOLDER(),DYKCityBuildingRules::DEV_PLACEHOLDER(),Plan));
	CancelWorkButton->SetIsEnabled(!Editing && HasWork);
}
void UDYKCityWorkWidget::ToggleClicked() {bOpen=!bOpen; Refresh();}
void UDYKCityWorkWidget::PreviousClicked() {CatalogIndex=(CatalogIndex+DYKCityCatalog::Buildings().Num()-1)%DYKCityCatalog::Buildings().Num(); Refresh();}
void UDYKCityWorkWidget::NextClicked() {CatalogIndex=(CatalogIndex+1)%DYKCityCatalog::Buildings().Num(); Refresh();}
void UDYKCityWorkWidget::BuildClicked()
{
	if(auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>())
	{ if(!City->GetConstructionType().IsNone()) City->CancelConstructionPlacement(); else City->BeginConstruction(DYKCityCatalog::Buildings()[CatalogIndex].Id.GetName()); }
	Refresh();
}
void UDYKCityWorkWidget::UpgradeClicked()
{
	auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>(); auto* Save=GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	if(!City || City->GetLayoutEditor().IsActive()) return;
	const auto* P=Save->GetProfileSnapshot(); const auto Id=City->GetSelectedBuilding();
	const auto* S=P ? P->CityEconomy.Instances.FindByPredicate([Id](const auto& Row){return Row.InstanceId==Id;}) : nullptr;
	if(!S) return;
	const auto& B=P->Buildings[S->BuildingIndex];
	FDYKCityBuildingWorkRequest R; R.CommandId=FGuid::NewGuid(); R.InstanceId=Id; R.BuildingId=FDYKBuildingId(B.BuildingId.Name);
	R.Kind=EDYKCityBuildingWorkKind::Upgrade; R.ExpectedLevel=B.Level;
	WorkMessage=DYKCityWork::Enqueue(*Save,R,FDateTime::UtcNow().ToUnixTimestamp()) ? TEXT("") : TEXT("Улучшение недоступно или не удалось сохранить.");
	Refresh();
}
void UDYKCityWorkWidget::CancelClicked()
{
	auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>(); auto* Save=GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	if(!City || City->GetLayoutEditor().IsActive()) return;
	const auto* P=Save->GetProfileSnapshot(); const auto Id=City->GetSelectedBuilding();
	const auto* W=P ? P->CityEconomy.Work.FindByPredicate([Id](const auto& Row){return Row.InstanceId==Id;}) : nullptr;
	if(!W) return; const auto WorkId=W->WorkId;
	WorkMessage=DYKCityWork::Cancel(*Save,FGuid::NewGuid(),WorkId,FDateTime::UtcNow().ToUnixTimestamp()) ? TEXT("") : TEXT("Работа уже завершена или не удалось сохранить отмену.");
	City->SyncLayoutVisuals(); Refresh();
}
void UDYKCityWorkWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
	Super::NativeTick(Geometry,DeltaTime);
	const auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>();
	FIntPoint Cell;
	if(!City || City->GetConstructionType().IsNone() || DYKCityPointer::HUDUnderPointer(this) || !DYKCityEditorInput::CursorCell(GetOwningPlayer(),Cell))
	{if(SitePreview) SitePreview->SetActorHiddenInGame(true); return;}
	if(!SitePreview || SitePreviewType!=City->GetConstructionType())
	{
		if(SitePreview) SitePreview->Destroy();
		SitePreviewType=City->GetConstructionType(); SitePreview=GetWorld()->SpawnActor<ADYKCityBuildingPreviewActor>();
		if(SitePreview && !SitePreview->Configure(SitePreviewType,true)) {SitePreview->Destroy(); SitePreview=nullptr;}
	}
	if(SitePreview)
	{
		FDYKBuildingSaveData B; B.BuildingId=FDYKBuildingId(SitePreviewType).ToContentId(); B.CellOrigin=Cell;
		SitePreview->SetActorHiddenInGame(false); SitePreview->SetActorLocation(DYKCityTerritory::BuildingLocation(B,DYKCityTerritory::DEV_PLACEHOLDER()));
	}
}
