#include "DYKPlacementWidget.h"
#include "DYKPolygonGameMode.h"
#include "DYKUnitSlot.h"
#include "DYKDefenderActor.h"
#include "../Core/DYKGameInstance.h"
#include "../Meta/DYKCityCatalog.h"
#include "GameFramework/WorldSettings.h"
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
		auto* Border = DYKUI::Panel(WidgetTree, this);
		auto* Layout = Canvas->AddChildToCanvas(Border);
		Layout->SetAnchors(FAnchors(.5f, 1.f));
		Layout->SetAlignment(FVector2D(.5f, 1.f));
		Layout->SetPosition(FVector2D(0.f, -16.f));
		Layout->SetAutoSize(true);
		auto* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
		Border->AddChild(Panel);
		StatusText = DYKUI::Text(WidgetTree, FText::GetEmpty(), 20);
		StatusText->SetJustification(ETextJustify::Center);
		Panel->AddChild(StatusText);
		auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Panel->AddChild(Row);
		const auto Button = [this, Row](const FText& Label)
		{
			auto* Control = DYKUI::Button(WidgetTree, this, Label);
			Row->AddChild(Control); return Control;
		};
		PlaceButton = Button(FText::Format(NSLOCTEXT("DYKBattle", "PeasantCost", "Крестьянин • {0} припасов"), DYKUnitCatalog::Find(DYKUnitIds::Peasant())->Base.PlacementSupplies.Value));
		SellButton = Button(NSLOCTEXT("DYKBattle", "SellButton", "Продать"));
		ResurrectButton = Button(NSLOCTEXT("DYKBattle", "ResurrectButton", "Воскресить"));
		StartButton = Button(NSLOCTEXT("DYKBattle", "StartButton", "Начать волну"));
		ReturnButton = Button(NSLOCTEXT("DYKBattle", "ReturnButton", "В город"));
		PlaceButton->OnClicked.AddDynamic(this, &ThisClass::PlaceClicked);
		SellButton->OnClicked.AddDynamic(this, &ThisClass::SellClicked);
		ResurrectButton->OnClicked.AddDynamic(this, &ThisClass::ResurrectClicked);
		StartButton->OnClicked.AddDynamic(this, &ThisClass::StartClicked);
		ReturnButton->OnClicked.AddDynamic(this, &ThisClass::ReturnClicked);
		RewardText = DYKUI::Text(WidgetTree, FText::GetEmpty(), 17); Panel->AddChild(RewardText);
		auto* Header = DYKUI::Panel(WidgetTree, this);
		auto* HeaderSlot = Canvas->AddChildToCanvas(Header);
		HeaderSlot->SetAnchors(FAnchors(.5f,0.f)); HeaderSlot->SetAlignment(FVector2D(.5f,0.f));
		HeaderSlot->SetPosition(FVector2D(0.f,12.f)); HeaderSlot->SetAutoSize(true);
		auto* HeaderStack = WidgetTree->ConstructWidget<UVerticalBox>(); Header->AddChild(HeaderStack);
		HUDText = DYKUI::Text(WidgetTree, FText::GetEmpty(), 19); HeaderStack->AddChild(HUDText);
		auto* Tools = WidgetTree->ConstructWidget<UHorizontalBox>(); HeaderStack->AddChild(Tools);
		const auto Tool = [this, Tools](const FText& Label) { auto* B = DYKUI::Button(WidgetTree,this,Label); Tools->AddChild(B); return B; };
		SpeedOneButton = Tool(NSLOCTEXT("DYKBattle","One","×1"));
		SpeedTwoButton = Tool(NSLOCTEXT("DYKBattle","Two","×2"));
		SpeedThreeButton = Tool(NSLOCTEXT("DYKBattle","Three","×3"));
		PauseButton = Tool(NSLOCTEXT("DYKBattle","Pause","Пауза / продолжить"));
		LeaveButton = Tool(NSLOCTEXT("DYKBattle","Leave","Покинуть миссию"));
		SpeedOneButton->OnClicked.AddDynamic(this,&ThisClass::SpeedOneClicked);
		SpeedTwoButton->OnClicked.AddDynamic(this,&ThisClass::SpeedTwoClicked);
		SpeedThreeButton->OnClicked.AddDynamic(this,&ThisClass::SpeedThreeClicked);
		PauseButton->OnClicked.AddDynamic(this,&ThisClass::PauseClicked);
		LeaveButton->OnClicked.AddDynamic(this,&ThisClass::LeaveClicked);
		// Full-screen hit-test surface prevents clicks reaching slots behind the confirmation.
		auto* Modal = WidgetTree->ConstructWidget<UBorder>();
		Modal->SetBrushColor(FLinearColor(0,0,0,.75f));
		auto* ModalSlot = Canvas->AddChildToCanvas(Modal);
		ModalSlot->SetAnchors(FAnchors(0,0,1,1)); ModalSlot->SetOffsets(FMargin(0));
		Modal->SetHorizontalAlignment(HAlign_Center); Modal->SetVerticalAlignment(VAlign_Center);
		ConfirmationPanel = Modal; Modal->SetVisibility(ESlateVisibility::Collapsed);
		auto* Paper = DYKUI::Panel(WidgetTree,this); Modal->AddChild(Paper);
		auto* ConfirmStack = WidgetTree->ConstructWidget<UVerticalBox>(); Paper->AddChild(ConfirmStack);
		ConfirmStack->AddChild(DYKUI::Text(WidgetTree,NSLOCTEXT("DYKBattle","Warning","Покинуть миссию?\nНезавершённый бой будет потерян. Награда не выдаётся."),20));
		ConfirmLeaveButton = DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKBattle","ConfirmLeave","Покинуть и вернуться в город"));
		CancelLeaveButton = DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKBattle","CancelLeave","Остаться"));
		ConfirmStack->AddChild(ConfirmLeaveButton); ConfirmStack->AddChild(CancelLeaveButton);
		ConfirmLeaveButton->OnClicked.AddDynamic(this,&ThisClass::ConfirmLeaveClicked);
		CancelLeaveButton->OnClicked.AddDynamic(this,&ThisClass::CancelLeaveClicked);

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
	const bool bTerminal = Battle.IsTerminal();
	if (bTerminal && ConfirmationPanel) ConfirmationPanel->SetVisibility(ESlateVisibility::Collapsed);
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	FText Objectives = FText::GetEmpty();
	for (const auto& Object : Battle.GetProtectedObjects())
	{
		if (!Object.bRequired) continue;
		const FText Name = Object.Id == FName(TEXT("PolygonCastle")) ? NSLOCTEXT("DYKBattle","Castle","Замок") : FText::FromName(Object.Id);
		Objectives = FText::Format(NSLOCTEXT("DYKBattle","Objective","{0}    {1}: {2} / {3} HP"), Objectives, Name, Battle.GetProtectedObjectHP(Object.Id), Object.MaxHP);
	}
	HUDText->SetText(FText::Format(NSLOCTEXT("DYKBattle","HUD","Волна {0} / {1}    •    Припасы: {2}    •    Монеты ловушек: —\n{3}    •    Скорость ×{4}{5}"),
		FMath::Max(1,Battle.GetWaveNumber()), Battle.GetTotalWaves(), Battle.GetSupplies(), Objectives,
		FMath::RoundToInt(UGameplayStatics::GetGlobalTimeDilation(this)), bPaused ? NSLOCTEXT("DYKBattle","Paused"," • Пауза") : FText::GetEmpty()));
	const bool bControls = !bTerminal && !bPaused;
	SpeedOneButton->SetIsEnabled(bControls); SpeedTwoButton->SetIsEnabled(bControls); SpeedThreeButton->SetIsEnabled(bControls);
	PauseButton->SetIsEnabled(!bTerminal); LeaveButton->SetVisibility(bTerminal ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	PlaceButton->SetVisibility(bTerminal ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	SellButton->SetVisibility(bTerminal ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ResurrectButton->SetVisibility(bTerminal ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	StartButton->SetVisibility(bTerminal ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	RewardText->SetVisibility(bTerminal ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

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
		const auto ResourceList = [](TConstArrayView<FDYKResourceSaveData> Rows)
		{
			FText Text = FText::GetEmpty();
			for (const auto& R : Rows)
			{
				const auto Id = FDYKResourceId::FromContentId(R.ResourceId); if (!Id) continue;
				Text = FText::Format(NSLOCTEXT("DYKBattle","ResourceRow","{0}  {1}: {2}"),Text,FText::FromString(DYKCityCatalog::ResourceName(Id.GetValue())),FText::AsNumber(R.Amount));
			}
			return Text.IsEmpty() ? NSLOCTEXT("DYKBattle","None","нет") : Text;
		};
		RewardText->SetText(FText::Format(NSLOCTEXT("DYKBattle","RewardSummary","Получено: {0}\nОжидает места на складе: {1}"),ResourceList(GameMode->GetAwardedResources()),ResourceList(GameMode->GetDeferredResources())));

		return;
	}
	const TCHAR* Phase = Battle.IsPreparation() ? TEXT("Подготовка") : Battle.IsTerminal() ? TEXT("Бой завершён") : TEXT("Волна идёт");
	const FText Selection = Unit ? FText::Format(NSLOCTEXT("DYKBattle","SelectedPeasant","Крестьянин · HP {0} / {1}"),Unit->GetHP(),Unit->GetUnitDefinition()->Base.MaxHP.Value)
		: Placement ? NSLOCTEXT("DYKBattle","EmptySlot","Свободная площадка") : NSLOCTEXT("DYKBattle","ChooseSlot","Выберите круглую площадку");
	StatusText->SetText(FText::Format(NSLOCTEXT("DYKBattle","SelectionStatus","{0} · Припасы: {1}\n{2}"),FText::FromString(Phase),Battle.GetSupplies(),Selection));
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

void UDYKPlacementWidget::SetSpeed(float Speed)
{
	const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADYKPolygonGameMode>() : nullptr;
	if (Mode && !Mode->GetBattleSession().IsTerminal() && !UGameplayStatics::IsGamePaused(this)) UGameplayStatics::SetGlobalTimeDilation(this,Speed);
	Refresh();
}

FReply UDYKPlacementWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (ConfirmationPanel && ConfirmationPanel->GetVisibility() == ESlateVisibility::Visible) return FReply::Handled();
	return Super::NativeOnMouseButtonDown(Geometry,Event);
}
void UDYKPlacementWidget::SpeedOneClicked() { SetSpeed(1.f); }
void UDYKPlacementWidget::SpeedTwoClicked() { SetSpeed(2.f); }
void UDYKPlacementWidget::SpeedThreeClicked() { SetSpeed(3.f); }
void UDYKPlacementWidget::PauseClicked()
{
	const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADYKPolygonGameMode>() : nullptr;
	if (Mode && !Mode->GetBattleSession().IsTerminal()) UGameplayStatics::SetGamePaused(this,!UGameplayStatics::IsGamePaused(this));
	Refresh();
}
void UDYKPlacementWidget::LeaveClicked()
{
	const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADYKPolygonGameMode>() : nullptr;
	if (Mode && !Mode->GetBattleSession().IsTerminal()) ConfirmationPanel->SetVisibility(ESlateVisibility::Visible);
}
void UDYKPlacementWidget::CancelLeaveClicked() { ConfirmationPanel->SetVisibility(ESlateVisibility::Collapsed); }
void UDYKPlacementWidget::ConfirmLeaveClicked()
{
	if (!ConfirmationPanel || ConfirmationPanel->GetVisibility() != ESlateVisibility::Visible) return;
	auto* Mode = GetWorld()->GetAuthGameMode<ADYKPolygonGameMode>();
	if (!Mode) return;
	if (!Mode->GetBattleSession().IsTerminal()) Mode->GetBattleSession().Abort(GetWorld()->GetTimeSeconds());
	UGameplayStatics::SetGamePaused(this,false);
	if (auto* GI = GetGameInstance<UDYKGameInstance>()) GI->RequestLevelTravel(EDYKLocalLevel::City,ReturnError);
	Refresh();
}
