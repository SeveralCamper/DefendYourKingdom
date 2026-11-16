#include "DYKStartWidget.h"
#include "../Core/DYKGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "DYKStart"
TSharedRef<SWidget> UDYKStartWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Canvas;
		auto* Shade = WidgetTree->ConstructWidget<UBorder>();
		Shade->SetBrushColor(FLinearColor(.015f,.025f,.035f,.90f));
		auto* Full = Canvas->AddChildToCanvas(Shade);
		Full->SetAnchors(FAnchors(0,0,1,1)); Full->SetOffsets(FMargin(0));
		auto* Paper = DYKUI::Panel(WidgetTree, this);
		auto* PaperSlot = Canvas->AddChildToCanvas(Paper);
		PaperSlot->SetAnchors(FAnchors(.5f,.5f)); PaperSlot->SetAlignment(FVector2D(.5f,.5f)); PaperSlot->SetAutoSize(true);
		auto* Stack = WidgetTree->ConstructWidget<UVerticalBox>(); Paper->AddChild(Stack);
		Stack->AddChild(DYKUI::Text(WidgetTree, LOCTEXT("Title", "DEFEND YOUR KINGDOM"), 34));
		Stack->AddChild(DYKUI::Text(WidgetTree, LOCTEXT("Motto", "Постройте город. Подготовьте оборону. Защитите королевство."), 18));
		Stack->AddChild(DYKUI::Text(WidgetTree, LOCTEXT("Local", "Локальная игра • Прогресс сохраняется автоматически"), 16));
		EnterButton = DYKUI::Button(WidgetTree, this, LOCTEXT("Enter", "Войти в город"));
		EnterButton->OnClicked.AddDynamic(this, &ThisClass::EnterClicked); Stack->AddChild(EnterButton);
	}
	return Super::RebuildWidget();
}
void UDYKStartWidget::EnterClicked()
{
	if (auto* GI = GetGameInstance<UDYKGameInstance>()) GI->bHasEnteredCity = true;
	RemoveFromParent();
}
FReply UDYKStartWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return FReply::Handled();
}
#undef LOCTEXT_NAMESPACE
