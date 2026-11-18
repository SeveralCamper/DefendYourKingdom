#include "DYKResourceBuildingWidget.h"
#include "DYKCityHUDArt.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

UDYKResourceHUDDefinition* UDYKResourceBuildingWidget::FindDefinition(FName Building)
{
	const auto* Catalog=LoadObject<UDYKResourceHUDCatalog>(nullptr,TEXT("/Game/DefendYourKingdom/Art/ResourceHUD/Catalog.Catalog"));
	if(Catalog) for(const auto& Soft:Catalog->Definitions) if(auto* Content=Soft.LoadSynchronous();Content && Content->BuildingId==Building) return Content;
	return nullptr;
}
void UDYKResourceHUDButton::Invoke() {if(Screen) Screen->Activate(Action,Index,Generation);}
void UDYKResourceBuildingWidget::Configure(UDYKResourceHUDDefinition* Content,bool Interior)
{Definition=Content; bInterior=Interior; if(Canvas) Build();}
TSharedRef<SWidget> UDYKResourceBuildingWidget::RebuildWidget()
{
	if(!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this);
	auto* Scale=WidgetTree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit); WidgetTree->RootWidget=Scale;
	auto* Design=WidgetTree->ConstructWidget<USizeBox>();
	const FVector2D Size=Definition ? (bInterior ? Definition->InteriorSize : Definition->FirstSize) : FVector2D(1122,1402);
	Design->SetWidthOverride(Size.X); Design->SetHeightOverride(Size.Y); Scale->AddChild(Design);
	Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); Design->AddChild(Canvas); Build(); return Super::RebuildWidget();
}
void UDYKResourceBuildingWidget::Build()
{
	++Generation; Buttons.Reset(); QueueIcons.Reset(); Canvas->ClearChildren(); StoredText=nullptr; OrderText=nullptr; TimerText=nullptr;
	if(!Definition) return;
	for(const auto& Part:Definition->Parts)
	{
		if(Part.bInterior!=bInterior) continue;
		if(Part.Action==EDYKResourceAction::None)
			DYKCityHUDArt::Texture(WidgetTree,Canvas,Part.Texture,Part.Position,Part.Size);
		else
		{
			auto* Button=WidgetTree->ConstructWidget<UDYKResourceHUDButton>(); Button->Screen=this; Button->Action=Part.Action; Button->Index=Part.Index; Button->Generation=Generation;
			const auto Brush=DYKCityHUDArt::TextureBrush(Part.Texture);
			FButtonStyle Style; Style.SetNormal(Brush); Style.SetHovered(Brush); Style.SetPressed(Brush); Style.SetDisabled(Brush);
			// Preserve the author's button pixels in every state; enabled state governs input.
			Style.SetNormalPadding(FMargin(0)); Style.SetPressedPadding(FMargin(0));
			Button->SetStyle(Style); Button->OnClicked.AddDynamic(Button,&UDYKResourceHUDButton::Invoke);
			DYKCityHUDArt::Place(Canvas,Button,Part.Position,Part.Size); Buttons.Add(Button);
		}
	}
	const FLinearColor Yellow(1,.82f,.04f,1);
	if(bInterior)
	{
		OrderText=DYKCityHUDArt::Label(WidgetTree,Canvas,FText::GetEmpty(),{970,798},{80,48},27,true);
		for(int32 I=0;I<3;++I) QueueIcons.Add(DYKCityHUDArt::Texture(WidgetTree,Canvas,Definition->ResourceIcon,{1062.+I*56,808},{36,27}));
	}
	else
	{
		DYKCityHUDArt::Texture(WidgetTree,Canvas,Definition->ResourceIcon,{393,795},{108,78});
		DYKCityHUDArt::Texture(WidgetTree,Canvas,Definition->ResourceIcon,{174,689},{73,55});
		StoredText=DYKCityHUDArt::Label(WidgetTree,Canvas,FText::GetEmpty(),{367,694},{75,43},27,true);
		OrderText=DYKCityHUDArt::Label(WidgetTree,Canvas,FText::GetEmpty(),{644,694},{72,43},27,true);
		TimerText=DYKCityHUDArt::Label(WidgetTree,Canvas,FText::GetEmpty(),{800,694},{159,43},26,true);
	}
	for(auto* Text:{StoredText.Get(),OrderText.Get(),TimerText.Get()}) if(Text) Text->SetColorAndOpacity(Yellow);
	UpdateView(View);
}
void UDYKResourceBuildingWidget::UpdateView(const FDYKResourceBuildingView& Value)
{
	View=Value;
	if(StoredText) StoredText->SetText(FText::AsNumber(View.Stored,&FNumberFormattingOptions::DefaultNoGrouping()));
	if(OrderText) OrderText->SetText(FText::FromString(FString::Printf(TEXT("%d / 3"),View.Orders)));
	for(int32 I=0;I<QueueIcons.Num();++I) QueueIcons[I]->SetVisibility(I<View.Orders ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if(TimerText) TimerText->SetText(FText::FromString(View.bWorking ? FString::Printf(TEXT("%lld мин %02lld с"),FMath::Max<int64>(0,View.Seconds)/60,FMath::Max<int64>(0,View.Seconds)%60) : TEXT("—")));
	for(const auto& Button:Buttons)
	{
		bool Enabled=true;
		switch(Button->Action)
		{
		case EDYKResourceAction::Collect: Enabled=View.bCanCollect; break;
		case EDYKResourceAction::Upgrade: Enabled=View.bCanUpgrade; break;
		case EDYKResourceAction::CancelWork: Enabled=View.bCanCancel; break;
		case EDYKResourceAction::Order: Enabled=View.Orders<3; break;
		case EDYKResourceAction::Visit: Enabled=View.bCanVisit; break;
		default: break;
		}
		Button->SetIsEnabled(Enabled);
	}
}
UDYKResourceHUDButton* UDYKResourceBuildingWidget::FindAction(EDYKResourceAction Action,int32 Index) const
{for(const auto& Button:Buttons) if(Button->Action==Action && Button->Index==Index) return Button.Get(); return nullptr;}
void UDYKResourceBuildingWidget::Activate(EDYKResourceAction Action,int32 Index,int32 ExpectedGeneration)
{if(Generation==ExpectedGeneration) if(const auto* Button=FindAction(Action,Index);Button && Button->GetIsEnabled()) OnAction.Broadcast(Action,Index);}
FVector2D UDYKResourceBuildingWidget::PopupPosition(FVector2D Anchor,FVector2D Viewport,FVector2D Popup)
{
	return {FMath::Clamp(Anchor.X-Popup.X*.5,8.,FMath::Max(8.,Viewport.X-Popup.X-8)),FMath::Clamp(Anchor.Y-Popup.Y*.5,8.,FMath::Max(8.,Viewport.Y-Popup.Y-8))};
}
