#include "DYKCityEditorWidget.h"
#include "DYKCityGameMode.h"
#include "DYKCityCatalog.h"
#include "DYKCityTerritoryWidget.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"

bool DYKCityEditorInput::CursorCell(APlayerController* PC, FIntPoint& Cell)
{
	FVector Origin,Direction;
	if (!PC || !PC->DeprojectMousePositionToWorld(Origin,Direction) || Direction.Z >= -KINDA_SMALL_NUMBER) return false;
	const auto Point = Origin + Direction * (-Origin.Z / Direction.Z);
	const auto Local = (Point-DYKCityTerritory::CellCorner({0,0}))/DYKCityTerritory::CellSize;
	if (!FMath::IsFinite(Local.X) || !FMath::IsFinite(Local.Y) || Local.X<0 || Local.Y<0 || Local.X>=50 || Local.Y>=50) return false;
	Cell = {int32(FMath::FloorToInt(Local.X)),int32(FMath::FloorToInt(Local.Y))};
	return true;
}

TSharedRef<SWidget> UDYKCityEditorWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Panel=DYKUI::Panel(WidgetTree,this);
		auto* PanelSlot=Canvas->AddChildToCanvas(Panel);
		PanelSlot->SetAnchors(FAnchors(0,1)); PanelSlot->SetAlignment(FVector2D(0,1));
		PanelSlot->SetPosition(FVector2D(16,-16)); PanelSlot->SetAutoSize(true);
		auto* Width=WidgetTree->ConstructWidget<USizeBox>(); Width->SetWidthOverride(340); Panel->AddChild(Width);
		auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Width->AddChild(Box);
		Details=DYKUI::Text(WidgetTree,FText::GetEmpty(),16); Details->SetAutoWrapText(true); Box->AddChild(Details);
		MoveButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCityEditor","Move","Переместить"));
		MoveButton->OnClicked.AddDynamic(this,&ThisClass::MoveClicked); Box->AddChild(MoveButton);
		NextButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCityEditor","Next","Следующее здание / буфер"));
		NextButton->OnClicked.AddDynamic(this,&ThisClass::NextClicked); Box->AddChild(NextButton);
		RotateButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCityEditor","Rotate","Повернуть на 90°"));
		RotateButton->OnClicked.AddDynamic(this,&ThisClass::RotateClicked); Box->AddChild(RotateButton);
		StoreButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCityEditor","Store","Временно убрать"));
		StoreButton->OnClicked.AddDynamic(this,&ThisClass::StoreClicked); Box->AddChild(StoreButton);
		ApplyButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCityEditor","Apply","Применить"));
		ApplyButton->OnClicked.AddDynamic(this,&ThisClass::ApplyClicked); Box->AddChild(ApplyButton);
		CancelButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCityEditor","Cancel","Отменить"));
		CancelButton->OnClicked.AddDynamic(this,&ThisClass::CancelClicked); Box->AddChild(CancelButton);
	}
	Refresh();
	return Super::RebuildWidget();
}

void UDYKCityEditorWidget::Refresh()
{
	const auto* City=GetWorld() ? GetWorld()->GetAuthGameMode<ADYKCityGameMode>() : nullptr;
	if (!City || !Panel) return;
	const bool Active=City->GetLayoutEditor().IsActive();
	Panel->SetVisibility(City->GetSelectedBuilding().IsValid() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	MoveButton->SetVisibility(Active ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	for (auto* B : {NextButton.Get(),RotateButton.Get(),StoreButton.Get(),ApplyButton.Get(),CancelButton.Get()})
		B->SetVisibility(Active ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const auto* P=GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	const auto Id=City->GetSelectedBuilding();
	const auto* S=P ? P->CityEconomy.Instances.FindByPredicate([Id](const auto& State) { return State.InstanceId==Id; }) : nullptr;
	if (!S) return;
	const auto* Def=DYKCityCatalog::Find(P->Buildings[S->BuildingIndex].BuildingId);
	const auto* E=City->GetLayoutEditor().Entries().FindByPredicate([Id](const auto& Entry) { return Entry.InstanceId==Id; });
	int32 Buffered=0; for (const auto& Entry : City->GetLayoutEditor().Entries()) Buffered+=!Entry.bPlaced;
	StoreButton->SetIsEnabled(E && E->bPlaced);
	FText Name=Def ? FText::FromString(Def->DisplayName) : FText::GetEmpty();
	Details->SetText(Active ? FText::Format(NSLOCTEXT("DYKCityEditor","Details","{0}\nПоворот: {1}° · В буфере: {2}\n{3}\n{4}"),Name,
		FText::AsNumber(City->GetPlacementTurns()*90),FText::AsNumber(Buffered),
		E && !E->bPlaced ? NSLOCTEXT("DYKCityEditor","InBuffer","Выбрано здание из буфера") : NSLOCTEXT("DYKCityEditor","OnMap","Выбрано здание на карте"),City->GetLayoutMessage()) : Name);
}

void UDYKCityEditorWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
	Super::NativeTick(Geometry,DeltaTime);
	const auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>();
	if (!City || !City->GetLayoutEditor().IsActive() || BlocksWorldInput()) return;
	FIntPoint Cell;
	if (!DYKCityEditorInput::CursorCell(GetOwningPlayer(),Cell)) return;
	const auto* P=GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	const auto Id=City->GetSelectedBuilding();
	const auto* S=P ? P->CityEconomy.Instances.FindByPredicate([Id](const auto& State) { return State.InstanceId==Id; }) : nullptr;
	if (!S) return;
	auto B=P->Buildings[S->BuildingIndex]; B.CellOrigin=Cell; B.RotationQuarterTurns=City->GetPlacementTurns();
	TArray<FIntPoint> Cells;
	const bool Legal=City->GetLayoutEditor().CanPlace(Id,Cell,B.RotationQuarterTurns);
	if (!DYKCityTerritory::OccupiedCells(B,DYKCityTerritory::DEV_PLACEHOLDER(),Cells)) Cells.Add(Cell);
	for (auto C : Cells) DrawDebugBox(GetWorld(),DYKCityTerritory::CellCorner(C)+FVector(100,100,12),FVector(97,97,4),Legal ? FColor::Green : FColor::Red,false,0,0,3);
}

bool UDYKCityEditorWidget::BlocksWorldInput() const { return DYKCityPointer::HUDUnderPointer(this); }

void UDYKCityEditorWidget::MoveClicked() { if (auto* C=GetWorld()->GetAuthGameMode<ADYKCityGameMode>()) C->BeginLayoutEdit(); Refresh(); }
void UDYKCityEditorWidget::StoreClicked() { if (auto* C=GetWorld()->GetAuthGameMode<ADYKCityGameMode>()) C->StoreSelected(); Refresh(); }
void UDYKCityEditorWidget::RotateClicked() { if (auto* C=GetWorld()->GetAuthGameMode<ADYKCityGameMode>()) C->RotateSelected(); Refresh(); }
void UDYKCityEditorWidget::ApplyClicked() { if (auto* C=GetWorld()->GetAuthGameMode<ADYKCityGameMode>()) C->CommitLayoutEdit(); Refresh(); }
void UDYKCityEditorWidget::CancelClicked() { if (auto* C=GetWorld()->GetAuthGameMode<ADYKCityGameMode>()) C->CancelLayoutEdit(); Refresh(); }
void UDYKCityEditorWidget::NextClicked()
{
	auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>();
	if (!City || !City->GetLayoutEditor().IsActive()) return;
	const auto& Entries=City->GetLayoutEditor().Entries();
	const int32 I=Entries.IndexOfByPredicate([City](const auto& E) { return E.InstanceId==City->GetSelectedBuilding(); });
	if (!Entries.IsEmpty()) City->SelectBuilding(Entries[(I+1)%Entries.Num()].InstanceId);
	Refresh();
}
