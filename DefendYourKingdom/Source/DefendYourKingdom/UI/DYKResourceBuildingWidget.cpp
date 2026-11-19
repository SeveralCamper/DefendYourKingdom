#include "DYKResourceBuildingWidget.h"
#include "DYKCityHUDArt.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

#include "Widgets/Input/SButton.h"
#include "Rendering/DrawElements.h"
#include "Components/ButtonSlot.h"

namespace
{
// Paint-only feedback: preserve source artwork, layout and hit area.
class SResourceButton final: public SButton
{
public:
 bool bRound=false;
 virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& WidgetTint,bool ParentEnabled) const override
 {
  const int32 Top=SButton::OnPaint(Args,Geometry,Culling,Elements,Layer,WidgetTint,ParentEnabled);
  if(!ParentEnabled || !IsEnabled() || !(IsHovered() || HasKeyboardFocus())) return Top;
  const FVector2D Size=Geometry.GetLocalSize();
  TArray<FVector2D> Points;
  if(bRound)
  {
   const FVector2D Center=Size*.5;
   for(int32 I=0;I<=48;++I) {const double A=2*PI*I/48.; Points.Add(Center+FVector2D(FMath::Cos(A)*Size.X*.43,FMath::Sin(A)*Size.Y*.43));}
  }
  else
  {
   const double Inset=7, Cut=FMath::Min(19.,Size.Y*.22);
   Points={{Inset+Cut,Inset},{Size.X-Inset-Cut,Inset},{Size.X-Inset,Inset+Cut},{Size.X-Inset,Size.Y-Inset-Cut},{Size.X-Inset-Cut,Size.Y-Inset},{Inset+Cut,Size.Y-Inset},{Inset,Size.Y-Inset-Cut},{Inset,Inset+Cut},{Inset+Cut,Inset}};
  }
  const float Opacity=WidgetTint.GetColorAndOpacityTint().A;
  for(int32 Band=0;Band<3;++Band)
   FSlateDrawElement::MakeLines(Elements,Top+1,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,FLinearColor(1,.65f,.06f,Opacity*(Band==0?.10f:Band==1?.22f:.8f)),true,Band==0?12.f:Band==1?7.f:2.f);
  return Top+1;
 }
};
}
TSharedRef<SWidget> UDYKResourceHUDButton::RebuildWidget()
{
 auto Native=SNew(SResourceButton)
  .OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked,SlateHandleClicked))
  .OnPressed(BIND_UOBJECT_DELEGATE(FSimpleDelegate,SlateHandlePressed))
  .OnReleased(BIND_UOBJECT_DELEGATE(FSimpleDelegate,SlateHandleReleased))
  .OnHovered_UObject(this,&UDYKResourceHUDButton::SlateHandleHovered)
  .OnUnhovered_UObject(this,&UDYKResourceHUDButton::SlateHandleUnhovered)
  .ButtonStyle(&GetStyle()).ClickMethod(GetClickMethod()).TouchMethod(GetTouchMethod()).PressMethod(GetPressMethod()).IsFocusable(GetIsFocusable());
 Native->bRound=Action==EDYKResourceAction::Close || Action==EDYKResourceAction::Back;
 MyButton=Native;
 if(GetChildrenCount()>0) CastChecked<UButtonSlot>(GetContentSlot())->BuildSlot(MyButton.ToSharedRef());
 return Native;
}
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
			// Source pixels stay unchanged; SResourceButton adds hover/focus contour feedback.
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

FVector2D UDYKResourceBuildingWidget::PopupSize(FVector2D Viewport)
{
 const FVector2D Preferred(506,632.5); // 15% larger than 440 x 550; source aspect preserved.
 const double Scale=FMath::Clamp(FMath::Min((Viewport.X-16)/Preferred.X,(Viewport.Y-16)/Preferred.Y),0.,1.);
 return Preferred*Scale;
}
