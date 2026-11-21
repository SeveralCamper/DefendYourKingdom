#include "DYKHeroPortraitWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UDYKHeroPortraitWidget::RebuildWidget()
{
	if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this);
	if(!WidgetTree->RootWidget)
	{
		auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
		const FLinearColor Ink(.17f,.14f,.11f,1),Light(.8f,.76f,.68f,1);
		const auto Shape=[this,Canvas](FAnchors Bounds,FLinearColor Color)
		{
			auto* Part=WidgetTree->ConstructWidget<UImage>(); Part->SetBrush(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
			Part->SetColorAndOpacity(Color); Part->SetVisibility(ESlateVisibility::HitTestInvisible);
			auto* Layout=Canvas->AddChildToCanvas(Part); Layout->SetAnchors(Bounds); Layout->SetOffsets(FMargin(0));
		};
		// Shoulders, neck and a closed helmet with a cross-shaped face guard.
		Shape({.18f,.75f,.82f,.94f},Ink); Shape({.27f,.66f,.73f,.8f},Ink); Shape({.43f,.55f,.57f,.74f},Ink);
		Shape({.34f,.14f,.66f,.6f},Ink); Shape({.39f,.09f,.61f,.2f},Ink);
		Shape({.375f,.32f,.625f,.36f},Light); Shape({.48f,.23f,.52f,.56f},Light);
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget();
}
