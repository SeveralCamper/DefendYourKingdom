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

TSharedRef<SWidget> UDYKCityCollectWidget::RebuildWidget()
{
	if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree")); }
	if (!WidgetTree->RootWidget)
	{
		auto* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
		auto* Paper = DYKUI::Panel(WidgetTree,this);
		Paper->SetPadding(FMargin(10.f,6.f)); Paper->AddChild(Panel);
		WidgetTree->RootWidget = Paper;
		NameText = DYKUI::Text(WidgetTree, FText::GetEmpty(), 18);
		NameText->SetJustification(ETextJustify::Center);
		Panel->AddChild(NameText);
		StatusText = DYKUI::Text(WidgetTree, FText::GetEmpty(), 14);
		StatusText->SetJustification(ETextJustify::Center);
		Panel->AddChild(StatusText);
		CollectButton = DYKUI::Button(WidgetTree, this, NSLOCTEXT("DYKCity", "Collect", "Собрать"));
		Panel->AddChild(CollectButton);
		CollectButton->OnClicked.AddDynamic(this, &ThisClass::CollectClicked);
	}
	return Super::RebuildWidget();
}

void UDYKCityCollectWidget::Configure(FGuid Instance, FDYKContentId Building)
{
	InstanceId = Instance;
	BuildingId = Building;
	Refresh();
}

void UDYKCityCollectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::Refresh, 1.f, true);
	Refresh();
}
void UDYKCityCollectWidget::NativeDestruct()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(RefreshTimer); }
	Super::NativeDestruct();
}

void UDYKCityCollectWidget::Refresh()
{
	const auto* Def = DYKCityCatalog::Find(BuildingId);
	const auto* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto* P = Save ? Save->GetProfileSnapshot() : nullptr;
	if (!Def || !P || !CollectButton) { return; }
	NameText->SetText(FText::FromString(Def->DisplayName));
	CollectButton->SetVisibility(Def->Resource.IsValid() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const auto View = DYKCityEconomy::View(*P, InstanceId, FDateTime::UtcNow().ToUnixTimestamp());
	CollectButton->SetIsEnabled(View.Collectable > 0);
	StatusText->SetText(!Def->Resource.IsValid() ? FText::GetEmpty() : FText::FromString(bSaveFailed
		? TEXT("Не удалось сохранить. Повторите сбор.")
		: FString::Printf(TEXT("%lld / %lld  ·  к сбору %lld"), View.Stored, View.Capacity, View.Collectable)));
}

void UDYKCityCollectWidget::CollectClicked()
{
	auto* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	if (!Save) { return; }
	if (!PendingCommand.IsValid()) { PendingCommand = FGuid::NewGuid(); }
	bSaveFailed = !DYKCityEconomy::Collect(*Save, PendingCommand, InstanceId, FDateTime::UtcNow().ToUnixTimestamp());
	if (!bSaveFailed) { PendingCommand.Invalidate(); }
	Refresh();
	for (TObjectIterator<UDYKCityResourceWidget> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld()) { It->Refresh(); }
	}
}

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
		if (!Text.IsEmpty()) { Text += TEXT("     "); }
		Text += FString::Printf(TEXT("%s: %lld"), DYKCityCatalog::ResourceName(Id), DYKCityEconomy::Balance(*P, Id));
		if (DYKCityCatalog::ResourceCapacity(Id) != MAX_uint32)
		{
			Text += FString::Printf(TEXT(" / %lld"), DYKCityCatalog::ResourceCapacity(Id));
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
