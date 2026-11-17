#include "DYKCityEditorWidget.h"
#include "DYKCityGameMode.h"
#include "DYKCityCatalog.h"
#include "DYKCityTerritoryWidget.h"
#include "DYKCityBuildingPreviewActor.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Components/ScrollBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
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
		RotateButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCityEditor","Rotate","Повернуть на 90°"));
		RotateButton->OnClicked.AddDynamic(this,&ThisClass::RotateClicked); Box->AddChild(RotateButton);
		StoreButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCityEditor","Store","Временно убрать"));
		StoreButton->OnClicked.AddDynamic(this,&ThisClass::StoreClicked); Box->AddChild(StoreButton);
		ApplyButton=DYKUI::Button(WidgetTree,this,NSLOCTEXT("DYKCityEditor","Finish","Закончить изменение"));
		ApplyButton->OnClicked.AddDynamic(this,&ThisClass::ApplyClicked); Box->AddChild(ApplyButton);
		BufferPanel=DYKUI::Panel(WidgetTree,this);
		auto* BufferSlot=Canvas->AddChildToCanvas(BufferPanel);
		BufferSlot->SetAnchors(FAnchors(.5,1)); BufferSlot->SetAlignment(FVector2D(.5,1));
		BufferSlot->SetPosition(FVector2D(0,0)); BufferSlot->SetAutoSize(true);
		auto* BufferWidth=WidgetTree->ConstructWidget<USizeBox>();
		BufferWidth->SetWidthOverride(560); BufferWidth->SetHeightOverride(170); BufferPanel->AddChild(BufferWidth);
		BufferScroll=WidgetTree->ConstructWidget<UScrollBox>(); BufferScroll->SetOrientation(Orient_Horizontal);
		BufferScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		BufferScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always); BufferWidth->AddChild(BufferScroll);
	}
	Refresh();
	return Super::RebuildWidget();
}

void UDYKCityEditorWidget::Refresh()
{
	const auto* City=GetWorld() ? GetWorld()->GetAuthGameMode<ADYKCityGameMode>() : nullptr;
	if (!City || !Panel) return;
	const bool Active=City->GetLayoutEditor().IsActive();
	Panel->SetVisibility(Active || City->GetSelectedBuilding().IsValid() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	MoveButton->SetVisibility(Active ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	for (auto* B : {RotateButton.Get(),StoreButton.Get(),ApplyButton.Get()})
		B->SetVisibility(Active ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RefreshBuffer();
	if (!Active && PlacementPreview) PlacementPreview->SetActorHiddenInGame(true);
	const auto* P=GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	const auto Id=City->GetSelectedBuilding();
	const auto* S=P ? P->CityEconomy.Instances.FindByPredicate([Id](const auto& State) { return State.InstanceId==Id; }) : nullptr;
	if (!S)
	{
		Details->SetText(City->GetLayoutMessage()); StoreButton->SetIsEnabled(false); RotateButton->SetIsEnabled(false); return;
	}
	const auto* Def=DYKCityCatalog::Find(P->Buildings[S->BuildingIndex].BuildingId);
	const auto* E=City->GetLayoutEditor().Entries().FindByPredicate([Id](const auto& Entry) { return Entry.InstanceId==Id; });
	int32 Buffered=0; for (const auto& Entry : City->GetLayoutEditor().Entries()) Buffered+=!Entry.bPlaced;
	StoreButton->SetIsEnabled(E && E->bPlaced);
	RotateButton->SetIsEnabled(Active);
	FText Name=Def ? FText::FromString(Def->DisplayName) : FText::GetEmpty();
	Details->SetText(Active ? FText::Format(NSLOCTEXT("DYKCityEditor","Details","{0}\nПоворот: {1}° · В буфере: {2}\n{3}\n{4}"),Name,
		FText::AsNumber(City->GetPlacementTurns()*90),FText::AsNumber(Buffered),
		E && !E->bPlaced ? NSLOCTEXT("DYKCityEditor","InBuffer","Выбрано здание из буфера") : NSLOCTEXT("DYKCityEditor","OnMap","Выбрано здание на карте"),City->GetLayoutMessage()) : Name);
}

void UDYKCityEditorWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
	Super::NativeTick(Geometry,DeltaTime);
	const auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>();
	if (!City || !City->GetLayoutEditor().IsActive() || BlocksWorldInput())
	{ if (PlacementPreview) PlacementPreview->SetActorHiddenInGame(true); return; }
	FIntPoint Cell;
	if (!DYKCityEditorInput::CursorCell(GetOwningPlayer(),Cell))
	{ if (PlacementPreview) PlacementPreview->SetActorHiddenInGame(true); return; }
	const auto* P=GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	const auto Id=City->GetSelectedBuilding();
	const auto* S=P ? P->CityEconomy.Instances.FindByPredicate([Id](const auto& State) { return State.InstanceId==Id; }) : nullptr;
	if (!S) { if (PlacementPreview) PlacementPreview->SetActorHiddenInGame(true); return; }
	auto B=P->Buildings[S->BuildingIndex]; B.CellOrigin=Cell; B.RotationQuarterTurns=City->GetPlacementTurns();
	TArray<FIntPoint> Cells;
	const bool Legal=City->GetLayoutEditor().CanPlace(Id,Cell,B.RotationQuarterTurns);
	const auto& Config=DYKCityTerritory::DEV_PLACEHOLDER();
	auto Local=B; Local.CellOrigin={0,0};
	if (!DYKCityTerritory::OccupiedCells(Local,Config,Cells)) return;
	FIntPoint Min=Cell,Max=Cell;
	TSet<FIntPoint> Footprint;
	for (auto& C:Cells)
	{
		C+=Cell; Footprint.Add(C);
		Min.X=FMath::Min(Min.X,C.X); Min.Y=FMath::Min(Min.Y,C.Y);
		Max.X=FMath::Max(Max.X,C.X); Max.Y=FMath::Max(Max.Y,C.Y);
	}
	TSet<FIntPoint> Shown=Footprint;
	for (const auto C:Footprint) for (int32 Y=-1;Y<=1;++Y) for (int32 X=-1;X<=1;++X)
		Shown.Add(C+FIntPoint(X,Y));
	for (const auto C:Shown) DrawDebugBox(GetWorld(),DYKCityTerritory::CellCorner(C)+FVector(100,100,12),FVector(97,97,4),Legal ? FColor::Green : FColor::Red,false,0,0,3);
	if (!PlacementPreview || PreviewType!=B.BuildingId.Name)
	{
		if (PlacementPreview) PlacementPreview->Destroy();
		PlacementPreview=GetWorld()->SpawnActor<ADYKCityBuildingPreviewActor>();
		PreviewType=B.BuildingId.Name;
		if (PlacementPreview && !PlacementPreview->Configure(PreviewType,true)) { PlacementPreview->Destroy(); PlacementPreview=nullptr; }
	}
	if (PlacementPreview)
	{
		const auto Center=(DYKCityTerritory::CellCorner(Min)+DYKCityTerritory::CellCorner(Max+FIntPoint(1,1)))*.5;
		PlacementPreview->SetActorHiddenInGame(false);
		PlacementPreview->SetActorLocationAndRotation(Center,FRotator(0,B.RotationQuarterTurns*90.,0));
	}
}

void UDYKCityBufferButton::SelectBuffered() { if (Editor) Editor->BufferClicked(InstanceId); }

void UDYKCityEditorWidget::BufferClicked(FGuid Instance)
{
	if (auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>(); City && City->GetLayoutEditor().IsActive())
		if (const auto* Entry=City->GetLayoutEditor().Entries().FindByPredicate([Instance](const auto& Row){ return Row.InstanceId==Instance; }); Entry && !Entry->bPlaced)
		{ City->SelectBuilding(Instance); City->SyncLayoutVisuals(); }
	Refresh();
}

UButton* UDYKCityEditorWidget::FindBufferButton(FGuid Instance) const
{
	for (const auto& Button:BufferButtons) if (Button->InstanceId==Instance) return Button.Get();
	return nullptr;
}

UTextureRenderTarget2D* UDYKCityEditorWidget::FindThumbnail(FName BuildingType) const
{
	const auto* Found=Thumbnails.Find(BuildingType); return Found ? Found->Get() : nullptr;
}

UTextureRenderTarget2D* UDYKCityEditorWidget::EnsureThumbnail(FName BuildingType)
{
	if (auto* Existing=FindThumbnail(BuildingType)) return Existing;
	auto* Model=GetWorld()->SpawnActor<ADYKCityBuildingPreviewActor>(FVector(0,0,-100000-ThumbnailActors.Num()*10000),FRotator::ZeroRotator);
	if (!Model) return nullptr;
	if (!Model->Configure(BuildingType,false)) { Model->Destroy(); return nullptr; }
	ThumbnailActors.Add(Model);
	auto* Target=Model->CaptureThumbnail(this); Thumbnails.Add(BuildingType,Target); return Target;
}

void UDYKCityEditorWidget::RefreshBuffer()
{
	if (!BufferPanel || !BufferScroll) return;
	const auto* City=GetWorld()->GetAuthGameMode<ADYKCityGameMode>();
	TArray<FGuid> Buffered;
	if (City && City->GetLayoutEditor().IsActive())
		for (const auto& Entry:City->GetLayoutEditor().Entries()) if (!Entry.bPlaced) Buffered.Add(Entry.InstanceId);
	BufferPanel->SetVisibility(Buffered.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	bool Same=Buffered.Num()==BufferButtons.Num();
	if (Same) for (int32 I=0;I<Buffered.Num();++I) Same&=BufferButtons[I]->InstanceId==Buffered[I];
	if (!Same)
	{
		BufferScroll->ClearChildren(); BufferButtons.Reset();
		const auto* P=GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
		for (const auto Id:Buffered)
		{
			const auto* S=P ? P->CityEconomy.Instances.FindByPredicate([Id](const auto& Row){ return Row.InstanceId==Id; }) : nullptr;
			if (!S) continue;
			const auto& B=P->Buildings[S->BuildingIndex]; const auto* Def=DYKCityCatalog::Find(B.BuildingId);
			auto* Button=WidgetTree->ConstructWidget<UDYKCityBufferButton>(); Button->Editor=this; Button->InstanceId=Id;
			Button->OnClicked.AddDynamic(Button,&UDYKCityBufferButton::SelectBuffered);
			auto* Size=WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(148); Size->SetHeightOverride(145); Button->AddChild(Size);
			auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Size->AddChild(Box);
			auto* ImageSize=WidgetTree->ConstructWidget<USizeBox>(); ImageSize->SetHeightOverride(112); Box->AddChild(ImageSize);
			auto* Image=WidgetTree->ConstructWidget<UImage>(); ImageSize->AddChild(Image);
			FSlateBrush Brush; Brush.SetResourceObject(EnsureThumbnail(B.BuildingId.Name)); Brush.ImageSize=FVector2D(112,112); Brush.DrawAs=ESlateBrushDrawType::Image;
			Image->SetBrush(Brush); Image->SetVisibility(ESlateVisibility::HitTestInvisible);
			auto* Label=DYKUI::Text(WidgetTree,Def ? FText::FromString(Def->DisplayName) : FText::GetEmpty(),13);
			Label->SetAutoWrapText(true); Box->AddChild(Label); Button->SetToolTipText(Label->GetText());
			BufferScroll->AddChild(Button); BufferButtons.Add(Button);
		}
	}
	for (const auto& Button:BufferButtons) Button->SetBackgroundColor(City && Button->InstanceId==City->GetSelectedBuilding() ? FLinearColor(1.f,.82f,.35f,1.f) : FLinearColor::White);
}

void UDYKCityEditorWidget::NativeDestruct()
{
	if (PlacementPreview) PlacementPreview->Destroy(); PlacementPreview=nullptr;
	for (const auto& Actor:ThumbnailActors) if (IsValid(Actor)) Actor->Destroy();
	ThumbnailActors.Reset(); Thumbnails.Reset();
	Super::NativeDestruct();
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
