#include "DYKPlacementWidget.h"
#include "DYKPolygonGameMode.h"
#include "DYKUnitSlot.h"
#include "DYKDefenderActor.h"
#include "../Core/DYKGameInstance.h"
#include "../Core/Content/DYKUnitCatalog.h"
#include "../Core/Interaction/DYKSelectionPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

TSharedRef<SWidget> UDYKPlacementWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = Canvas;
		auto* Border = WidgetTree->ConstructWidget<UBorder>();
		Border->SetBrushColor(FLinearColor(.025f, .035f, .05f, .95f));
		Border->SetPadding(FMargin(16.f, 10.f));
		auto* Layout = Canvas->AddChildToCanvas(Border);
		Layout->SetAnchors(FAnchors(.5f, 0.f));
		Layout->SetAlignment(FVector2D(.5f, 0.f));
		Layout->SetPosition(FVector2D(0.f, 12.f));
		Layout->SetAutoSize(true);
		auto* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
		Border->AddChild(Panel);
		StatusText = WidgetTree->ConstructWidget<UTextBlock>();
		StatusText->SetJustification(ETextJustify::Center);
		Panel->AddChild(StatusText);
		auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Panel->AddChild(Row);
		const auto Button = [this, Row](const FString& Label)
		{
			auto* Control = WidgetTree->ConstructWidget<UButton>();
			auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
			Text->SetText(FText::FromString(Label));
			Control->AddChild(Text);
			Row->AddChild(Control);
			return Control;
		};
		PlaceButton = Button(FString::Printf(TEXT("Крестьянин: %d припасов"), DYKUnitCatalog::Find(DYKUnitIds::Peasant())->Base.PlacementSupplies.Value));
		SellButton = Button(TEXT("Продать"));
		ResurrectButton = Button(TEXT("Воскресить"));
		StartButton = Button(TEXT("Начать волну"));
		ReturnButton = Button(TEXT("В город"));
		PlaceButton->OnClicked.AddDynamic(this, &ThisClass::PlaceClicked);
		SellButton->OnClicked.AddDynamic(this, &ThisClass::SellClicked);
		ResurrectButton->OnClicked.AddDynamic(this, &ThisClass::ResurrectClicked);
		StartButton->OnClicked.AddDynamic(this, &ThisClass::StartClicked);
		ReturnButton->OnClicked.AddDynamic(this, &ThisClass::ReturnClicked);
	}
	return Super::RebuildWidget();
}

void UDYKPlacementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (auto* PC = Cast<ADYKSelectionPlayerController>(GetOwningPlayer()))
	{
		SelectedSlot = Cast<ADYKUnitSlot>(PC->GetSelectedActor());
		PC->OnWorldSelectionChanged().AddUObject(this, &ThisClass::SelectionChanged);
	}
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::Refresh, .2f, true);
	Refresh();
}

void UDYKPlacementWidget::NativeDestruct()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	if (auto* PC = Cast<ADYKSelectionPlayerController>(GetOwningPlayer())) PC->OnWorldSelectionChanged().RemoveAll(this);
	Super::NativeDestruct();
}

void UDYKPlacementWidget::SelectionChanged(const FDYKWorldSelection& Selection)
{
	SelectedSlot = Cast<ADYKUnitSlot>(Selection.TargetActor.Get());
	Refresh();
}

void UDYKPlacementWidget::Refresh()
{
	auto* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADYKPolygonGameMode>() : nullptr;
	if (!GameMode || !StatusText) return;
	const auto& Battle = GameMode->GetBattleSession();
	DisplayedPreparation = Battle.GetPreparationToken();
	auto* Placement = SelectedSlot.Get();
	auto* Unit = Placement ? Placement->GetOccupant() : nullptr;
	const bool bPreparation = Battle.IsPreparation() && !UGameplayStatics::IsGamePaused(this);
	const int32 Cost = DYKUnitCatalog::Find(DYKUnitIds::Peasant())->Base.PlacementSupplies.Value;
	PlaceButton->SetIsEnabled(bPreparation && Placement && Placement->bPlayerEditable && !Unit && Battle.GetSupplies() >= Cost);
	SellButton->SetIsEnabled(bPreparation && Placement && Placement->bPlayerEditable && Unit);
	ResurrectButton->SetIsEnabled(Unit && Unit->CanResurrect());
	StartButton->SetIsEnabled(Battle.CanStartWave() && !UGameplayStatics::IsGamePaused(this));
	const auto* Result = Battle.GetResult();
	ReturnButton->SetVisibility(Result && Result->Outcome != EDYKBattlePhase::Aborted ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const auto* Local = GetGameInstance<UDYKGameInstance>();
	ReturnButton->SetIsEnabled(Local && !Local->IsLevelTravelPending());
	if (Result)
	{
		const TCHAR* Outcome = Result->Outcome == EDYKBattlePhase::Victory ? TEXT("Победа") : Result->Outcome == EDYKBattlePhase::Defeat ? TEXT("Поражение") : TEXT("Миссия прервана");
		const FString Error = !ReturnError.IsEmpty() ? ReturnError : GameMode->GetMissionResultError();
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("%s · Время: %.1f с · Убито: %d%s%s"),
			Outcome, Result->ActiveSeconds, Result->EnemiesKilled, Error.IsEmpty() ? TEXT("") : TEXT("\n"), *Error)));
		return;
	}
	const TCHAR* Phase = Battle.IsPreparation() ? TEXT("Подготовка") : Battle.IsTerminal() ? TEXT("Бой завершён") : TEXT("Волна идёт");
	const FString Selection = Unit ? FString::Printf(TEXT("%s · HP %d / %d"), *Placement->SlotId.ToString(), Unit->GetHP(), Unit->GetUnitDefinition()->Base.MaxHP.Value)
		: Placement ? Placement->SlotId.ToString() : TEXT("Выберите круглую площадку");
	StatusText->SetText(FText::FromString(FString::Printf(TEXT("%s · Припасы: %d\n%s"), Phase, Battle.GetSupplies(), *Selection)));
}

void UDYKPlacementWidget::PlaceClicked()
{
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ADYKPolygonGameMode>()) GameMode->PlaceDefender(SelectedSlot.Get(), DisplayedPreparation);
	Refresh();
}
void UDYKPlacementWidget::SellClicked()
{
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ADYKPolygonGameMode>()) GameMode->SellDefender(SelectedSlot.Get(), DisplayedPreparation);
	Refresh();
}
void UDYKPlacementWidget::ResurrectClicked()
{
	if (SelectedSlot.IsValid() && IsValid(SelectedSlot->GetOccupant())) SelectedSlot->GetOccupant()->Resurrect();
	Refresh();
}
void UDYKPlacementWidget::StartClicked()
{
	if (!UGameplayStatics::IsGamePaused(this))
		if (auto* GameMode = GetWorld()->GetAuthGameMode<ADYKPolygonGameMode>()) GameMode->GetBattleSession().StartWave(DisplayedPreparation, GetWorld()->GetTimeSeconds());
	Refresh();
}

void UDYKPlacementWidget::ReturnClicked()
{
	if (auto* Mode = GetWorld()->GetAuthGameMode<ADYKPolygonGameMode>()) Mode->ReturnToCity(ReturnError);
	Refresh();
}
