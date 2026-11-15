#include "DYKCityWidgets.h"
#include "DYKCityEconomy.h"
#include "DYKMissionRewards.h"
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
		WidgetTree->RootWidget = Panel;
		NameText = WidgetTree->ConstructWidget<UTextBlock>();
		NameText->SetJustification(ETextJustify::Center);
		Panel->AddChild(NameText);
		StatusText = WidgetTree->ConstructWidget<UTextBlock>();
		StatusText->SetJustification(ETextJustify::Center);
		Panel->AddChild(StatusText);
		CollectButton = WidgetTree->ConstructWidget<UButton>();
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(TEXT("Собрать")));
		Label->SetJustification(ETextJustify::Center);
		CollectButton->AddChild(Label);
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
		auto* Border = WidgetTree->ConstructWidget<UBorder>();
		Border->SetBrushColor(FLinearColor(.025f, .035f, .05f, .95f));
		Border->SetPadding(FMargin(24.f, 12.f));
		auto* CanvasSlot = Canvas->AddChildToCanvas(Border);
		CanvasSlot->SetAnchors(FAnchors(.5f, 0.f));
		CanvasSlot->SetAlignment(FVector2D(.5f, 0.f));
		CanvasSlot->SetPosition(FVector2D(0.f, 16.f));
		CanvasSlot->SetAutoSize(true);
		ResourceText = WidgetTree->ConstructWidget<UTextBlock>();
		auto* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
		Border->AddChild(Panel);
		Panel->AddChild(ResourceText);
		RewardText = WidgetTree->ConstructWidget<UTextBlock>();
		RewardText->SetJustification(ETextJustify::Center);
		Panel->AddChild(RewardText);
		CollectRewardsButton = WidgetTree->ConstructWidget<UButton>();
		auto* RewardLabel = WidgetTree->ConstructWidget<UTextBlock>();
		RewardLabel->SetText(FText::FromString(TEXT("Собрать награды — полигон")));
		RewardLabel->SetJustification(ETextJustify::Center);
		CollectRewardsButton->AddChild(RewardLabel);
		CollectRewardsButton->OnClicked.AddDynamic(this, &ThisClass::CollectRewardsClicked);
		Panel->AddChild(CollectRewardsButton);
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	return Super::RebuildWidget();
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
