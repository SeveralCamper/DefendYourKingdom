#include "DYKCityWidgets.h"
#include "DYKCityEconomy.h"
#include "DYKCityTerritoryWidget.h"
#include "DYKMissionRewards.h"
#include "../Core/DYKGameInstance.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

TSharedRef<SWidget> UDYKCityResourceWidget::RebuildWidget()
{
	if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree")); }
	if (!WidgetTree->RootWidget)
	{
		auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Canvas;
		auto* Border = DYKUI::Panel(WidgetTree, this);
		auto* CanvasSlot = Canvas->AddChildToCanvas(Border);
		CanvasSlot->SetAnchors(FAnchors(.5f, 0.f));
		CanvasSlot->SetAlignment(FVector2D(.5f, 0.f));
		CanvasSlot->SetPosition(FVector2D(0.f, 16.f));
		CanvasSlot->SetAutoSize(true);
		ResourceText = DYKUI::Text(WidgetTree, FText::GetEmpty(), 20);
		auto* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
		Border->AddChild(Panel);
		Panel->AddChild(ResourceText);
		RewardText = DYKUI::Text(WidgetTree, FText::GetEmpty(), 16);
		RewardText->SetJustification(ETextJustify::Center);
		Panel->AddChild(RewardText);
		CollectRewardsButton = DYKUI::Button(WidgetTree, this, NSLOCTEXT("DYKCity", "CollectReward", "Собрать награды — полигон"));
		CollectRewardsButton->OnClicked.AddDynamic(this, &ThisClass::CollectRewardsClicked);
		Panel->AddChild(CollectRewardsButton);
		auto* Footer = DYKUI::Panel(WidgetTree, this);
		auto* FooterSlot = Canvas->AddChildToCanvas(Footer);
		FooterSlot->SetAnchors(FAnchors(1.f,1.f)); FooterSlot->SetAlignment(FVector2D(1.f,1.f));
		FooterSlot->SetPosition(FVector2D(-20.f,-20.f)); FooterSlot->SetAutoSize(true);
		auto* Nav = WidgetTree->ConstructWidget<UVerticalBox>(); Footer->AddChild(Nav);
		Nav->AddChild(DYKUI::Text(WidgetTree, NSLOCTEXT("DYKCity", "Defense", "Защитите подступы к городу"), 20));
		MissionButton = DYKUI::Button(WidgetTree, this, NSLOCTEXT("DYKCity", "Polygon", "Подготовить оборону"));
		MissionButton->OnClicked.AddDynamic(this, &ThisClass::MissionClicked); Nav->AddChild(MissionButton);
		Nav->AddChild(DYKUI::Text(WidgetTree, NSLOCTEXT("DYKCity", "Camera", "WASD — камера • Колесо — масштаб"), 14));
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	return Super::RebuildWidget();
}

void UDYKCityResourceWidget::MissionClicked()
{
	FString Error;
	if (auto* GI = GetGameInstance<UDYKGameInstance>())
	{
		GI->bHasEnteredCity = true;
		if (!GI->RequestLevelTravel(EDYKLocalLevel::MissionPolygon, Error) && RewardText)
			RewardText->SetText(FText::FromString(Error));
		if (MissionButton) MissionButton->SetIsEnabled(!GI->IsLevelTravelPending());
	}
}
void UDYKCityResourceWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::Refresh, 1.f, true);
	Refresh();
}
void UDYKCityResourceWidget::NativeDestruct()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(RefreshTimer); }
	Super::NativeDestruct();
}
void UDYKCityResourceWidget::Refresh()
{
	const auto* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto* P = Save ? Save->GetProfileSnapshot() : nullptr;
	if (!P || !ResourceText) { return; }
	for (TObjectIterator<UDYKCityTerritoryWidget> It; It; ++It)
		if (It->GetWorld()==GetWorld()) It->Refresh();
	FString Text;
	for (auto Id : DYKCityCatalog::Resources())
	{
		// The permanent top bar retains its four approved resources.
		if (Id.GetName()==TEXT("RES-IRON")) continue;
		if (!Text.IsEmpty()) { Text += TEXT("     "); }
		Text += FString::Printf(TEXT("%s: %lld"), DYKCityCatalog::ResourceName(Id), DYKCityEconomy::Balance(*P, Id));
		if (DYKCityEconomy::Capacity(*P,Id) != MAX_uint32)
		{
			Text += FString::Printf(TEXT(" / %lld"), DYKCityEconomy::Capacity(*P,Id));
		}
	}
	ResourceText->SetText(FText::FromString(Text));
	const auto Campaign = DYKMissionRewards::PolygonDEV_PLACEHOLDER().Campaign;
	bool Pending = false;
	FString Remaining;
	for (const auto& Receipt : P->MissionRewards)
	{
		if (Receipt.CampaignId != Campaign.ToContentId()) continue;
		for (const auto& Row : Receipt.Remaining)
		{
			Pending = true;
			const auto Id = FDYKResourceId::FromContentId(Row.ResourceId);
			if (!Id) continue;
			Remaining += FString::Printf(TEXT(" %s: %lld"), DYKCityCatalog::ResourceName(Id.GetValue()), Row.Amount);
		}
	}
	CollectRewardsButton->SetVisibility(Pending ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CollectRewardsButton->SetIsEnabled(DYKMissionRewards::HasCollectable(*P, Campaign));
	RewardText->SetText(FText::FromString(bRewardSaveFailed ? TEXT("Не удалось сохранить. Повторите сбор награды.")
		: Pending ? TEXT("Ожидает места на складе:") + Remaining : FString()));
}

void UDYKCityResourceWidget::CollectRewardsClicked()
{
	auto* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	if (!Save) return;
	if (!RewardCommand.IsValid()) RewardCommand = FGuid::NewGuid();
	bRewardSaveFailed = !DYKMissionRewards::Collect(*Save, RewardCommand, DYKMissionRewards::PolygonDEV_PLACEHOLDER().Campaign);
	if (!bRewardSaveFailed) RewardCommand.Invalidate();
	Refresh();
}
