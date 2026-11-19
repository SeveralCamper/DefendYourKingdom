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

#include "DYKHUDButtonSlate.h"
#include "../Meta/DYKCityCatalog.h"

namespace
{
 FString RecipeDuration(int64 Seconds)
 {
  if(Seconds<3600) return FString::Printf(TEXT("%lld минут"),FMath::Max<int64>(0,Seconds)/60);
  if(Seconds%3600!=0) return UDYKResourceBuildingWidget::FormatWorkTime(Seconds);
  const int64 Hours=Seconds/3600;
  const int64 Last=Hours%10, LastTwo=Hours%100;
  const TCHAR* Unit=LastTwo>=11 && LastTwo<=14 ? TEXT("часов") : Last==1 ? TEXT("час") : Last>=2 && Last<=4 ? TEXT("часа") : TEXT("часов");
  return FString::Printf(TEXT("%lld %s"),Hours,Unit);
 }
}
TSharedRef<SWidget> UDYKResourceHUDButton::RebuildWidget()
{
 auto Native=SNew(SDYKHUDButton)
  .OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked,SlateHandleClicked))
  .OnPressed(BIND_UOBJECT_DELEGATE(FSimpleDelegate,SlateHandlePressed))
  .OnReleased(BIND_UOBJECT_DELEGATE(FSimpleDelegate,SlateHandleReleased))
  .OnHovered_UObject(this,&UDYKResourceHUDButton::SlateHandleHovered)
  .OnUnhovered_UObject(this,&UDYKResourceHUDButton::SlateHandleUnhovered)
  .ButtonStyle(&GetStyle()).ClickMethod(GetClickMethod()).TouchMethod(GetTouchMethod()).PressMethod(GetPressMethod()).IsFocusable(GetIsFocusable());
 Native->bRound=Action==EDYKResourceAction::Close || Action==EDYKResourceAction::Back;
 if(Action==EDYKResourceAction::Back) Native->CircleCenter={.5,37./91.};
 Native->HoverOutline=HoverOutline;
 if(!bActionAvailable && (!Screen || !Screen->Definition || !Screen->Definition->bProductionTemplate))
  Native->HighlightColor=FLinearColor(.65f,.68f,.72f,1);
 MyButton=Native;
 if(GetChildrenCount()>0) CastChecked<UButtonSlot>(GetContentSlot())->BuildSlot(MyButton.ToSharedRef());
 return Native;
}
UDYKResourceHUDDefinition* UDYKResourceBuildingWidget::FindDefinition(FName Building)
{
	if(const auto* BuildingData=DYKCityCatalog::Find(FDYKBuildingId(Building).ToContentId());
    BuildingData && BuildingData->Category==EDYKCityBuildingCategory::Production)
  return LoadObject<UDYKResourceHUDDefinition>(nullptr,TEXT("/Game/DefendYourKingdom/Art/ResourceHUD/ProductionTemplate.ProductionTemplate"));
 const auto* Catalog=LoadObject<UDYKResourceHUDCatalog>(nullptr,TEXT("/Game/DefendYourKingdom/Art/ResourceHUD/Catalog.Catalog"));
	if(Catalog) for(const auto& Soft:Catalog->Definitions) if(auto* Content=Soft.LoadSynchronous();Content && Content->bFirstOnly && !Content->bProductionTemplate && Content->BuildingId==Building) return Content;
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
	++Generation; Buttons.Reset(); Canvas->ClearChildren(); ProcessText=nullptr; WorkText=nullptr; Feedback=nullptr;
	PrimaryIllustration=nullptr;
 ProductionLabels.Reset(); ProductionSlots.Reset();
	if(!Definition) return;
	for(const auto& Part:Definition->Parts)
	{
		if(Part.bInterior!=bInterior) continue;
		if(Part.Action==EDYKResourceAction::None)
			{if(Part.Texture) DYKCityHUDArt::Texture(WidgetTree,Canvas,Part.Texture,Part.Position,Part.Size);}
		else
		{
			auto* Button=WidgetTree->ConstructWidget<UDYKResourceHUDButton>(); Button->Screen=this; Button->Action=Part.Action; Button->Index=Part.Index; Button->Generation=Generation;
			Button->HoverOutline=Part.HoverOutline;
			auto Brush=DYKCityHUDArt::TextureBrush(Part.Texture);
            if(!Part.Texture) Brush.DrawAs=ESlateBrushDrawType::NoDrawType;
			FButtonStyle Style; Style.SetNormal(Brush); Style.SetHovered(Brush); Style.SetPressed(Brush); Style.SetDisabled(Brush);
			if(Definition->bProductionTemplate)
			{
				auto Disabled=DYKCityHUDArt::TextureBrush(Part.UnavailableTexture);
				// Controls without a disabled variant keep their original artwork.
				if(!Part.UnavailableTexture) Disabled.TintColor=FLinearColor::Transparent;
				Style.SetDisabled(Disabled);
			}
			// Source pixels stay unchanged; SResourceButton adds hover/focus contour feedback.
			Style.SetNormalPadding(FMargin(0)); Style.SetPressedPadding(FMargin(0));
			Button->SetStyle(Style); Button->OnClicked.AddDynamic(Button,&UDYKResourceHUDButton::Invoke);
			DYKCityHUDArt::Place(Canvas,Button,Part.Position,Part.Size); Buttons.Add(Button);
		}
	}
	if(Definition->bProductionTemplate)
    {
     if(!bInterior)
     {
      auto* Fit=WidgetTree->ConstructWidget<UScaleBox>();
      Fit->SetStretch(EStretch::ScaleToFit);
      Fit->SetVisibility(ESlateVisibility::HitTestInvisible);
      PrimaryIllustration=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("PrimaryIllustration"));
      Fit->AddChild(PrimaryIllustration);
      DYKCityHUDArt::Place(Canvas,Fit,Definition->PrimaryIllustrationPosition,Definition->PrimaryIllustrationSize);
     }
     BuildProductionLabels();
     UpdateView(View);
     return;
    }
    Feedback=DYKCityHUDArt::Label(WidgetTree,Canvas,FText::GetEmpty(),bInterior ? FVector2D(245,738) : FVector2D(195,1230),bInterior ? FVector2D(950,30) : FVector2D(730,50),22,false);
	if(!bInterior && !Definition->DisplayName.IsEmpty())
    {
     DYKCityHUDArt::Label(WidgetTree,Canvas,Definition->DisplayName,{335,140},{480,94},68,true);
     DYKCityHUDArt::Label(WidgetTree,Canvas,Definition->Description,{220,565},{680,76},30,false)->SetWrapTextAt(0);
    }
    ProcessText=DYKCityHUDArt::Label(WidgetTree,Canvas,FText::GetEmpty(),{190,685},{750,72},27,true);
    WorkText=DYKCityHUDArt::Label(WidgetTree,Canvas,FText::GetEmpty(),{314,800},{500,72},26,true);
    WorkText->SetWrapTextAt(0);
	UpdateView(View);
}
void UDYKResourceBuildingWidget::UpdateView(const FDYKResourceBuildingView& Value)
{
	View=Value;
    if(Definition && Definition->bProductionTemplate)
    {
     if(PrimaryIllustration)
     {
      UTexture2D* Picture=View.Illustration.Get();
      if(PrimaryIllustration->GetBrush().GetResourceObject()!=Picture)
       PrimaryIllustration->SetBrush(DYKCityHUDArt::TextureBrush(Picture));
      PrimaryIllustration->SetVisibility(Picture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
     }
     for(int32 I=0;I<ProductionLabels.Num();++I)
     {
      const auto& TextSlot=Definition->TextSlots[ProductionSlots[I]];
      ProductionLabels[I]->SetText(ProductionText(TextSlot));
      ProductionLabels[I]->GetParent()->SetVisibility(TextSlot.Field==EDYKProductionField::QueueSlot && TextSlot.Index>=View.Orders ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
     }
     UpdateButtons();
     return;
    }
    if(ProcessText) ProcessText->SetText(FText::FromString(FString::Printf(TEXT("Уровень %d  ·  Исследований: %d"),View.Level,View.Orders)));
    if(WorkText) WorkText->SetText(FText::FromString(View.Process));
 UpdateButtons();
}
void UDYKResourceBuildingWidget::UpdateButtons()
{
	for(const auto& Button:Buttons)
	{
		bool Enabled=true;
		switch(Button->Action)
		{
		case EDYKResourceAction::Collect: Enabled=View.bCanCollect; break;
		case EDYKResourceAction::Upgrade: Enabled=View.bCanUpgrade; break;
		case EDYKResourceAction::CancelWork: Enabled=View.bCanCancel; break;
		case EDYKResourceAction::Order: Enabled=View.bCanOrder && View.Orders<View.QueueCapacity; break;
		case EDYKResourceAction::Visit: Enabled=Definition && Definition->bProductionTemplate ? true : View.bCanVisit; break;
		default: break;
		}

        if(Definition->bProductionTemplate)
        {
            Button->SetAvailability(Enabled,FString());
            continue;
        }
        FString Reason;
        if(!Enabled) switch(Button->Action)
        {
        case EDYKResourceAction::Order: Reason=View.bCanOrder ? TEXT("Очередь заполнена: дождитесь завершения заказа.") : TEXT("Здание ещё строится."); break;
        case EDYKResourceAction::Collect: Reason=TEXT("Сбор: нужен готовый ресурс, место на складе и минута накопления."); break;
        case EDYKResourceAction::Upgrade: Reason=TEXT("Улучшение недоступно: проверьте уровень замка и текущие работы."); break;
        case EDYKResourceAction::CancelWork: Reason=TEXT("Нет работы для отмены."); break;
        default: Reason=TEXT("Здание ещё строится."); break;
        }
        Button->SetAvailability(Enabled,Reason);
        // Keep pointer feedback even when the game rules reject the command.
        Button->SetIsEnabled(true);
	}
}
UDYKResourceHUDButton* UDYKResourceBuildingWidget::FindAction(EDYKResourceAction Action,int32 Index) const
{for(const auto& Button:Buttons) if(Button->Action==Action && Button->Index==Index) return Button.Get(); return nullptr;}
void UDYKResourceBuildingWidget::Activate(EDYKResourceAction Action,int32 Index,int32 ExpectedGeneration)
{if(Generation==ExpectedGeneration) if(const auto* Button=FindAction(Action,Index);Button && Button->GetIsEnabled())
 {if(!Button->bActionAvailable) SetFeedback(Button->UnavailableReason); else {SetFeedback(FString()); OnAction.Broadcast(Action,Index);}}}
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

void UDYKResourceBuildingWidget::SetFeedback(const FString& Text)
{if(Feedback) Feedback->SetText(FText::FromString(Text));}

void UDYKResourceHUDButton::SetAvailability(bool Available,const FString& Reason)
{
 bActionAvailable=Available;
 if(Screen && Screen->Definition && Screen->Definition->bProductionTemplate)
 {
  SetIsEnabled(Available);
  return;
 }
 UnavailableReason=Reason;
 SetToolTipText(FText::FromString(Reason));
 if(MyButton.IsValid())
 {
  const auto Native=StaticCastSharedPtr<SDYKHUDButton>(MyButton);
  Native->HighlightColor=Available ? FLinearColor(1,.65f,.06f,1) : FLinearColor(.65f,.68f,.72f,1);
  Native->Invalidate(EInvalidateWidgetReason::Paint);
 }
}
FString UDYKResourceBuildingWidget::FormatWorkTime(int64 Seconds)
{
 Seconds=FMath::Max<int64>(0,Seconds);
 const int64 Minutes=Seconds/60+(Seconds%60>0 ? 1 : 0);
 return FString::Printf(TEXT("%lld ч %02lld мин"),Minutes/60,Minutes%60);
}
void UDYKResourceBuildingWidget::BuildProductionLabels()
{
 for(int32 I=0;I<Definition->TextSlots.Num();++I)
 {
  const auto& TextSlot=Definition->TextSlots[I];
  if(TextSlot.bInterior!=bInterior) continue;
  auto* Label=DYKCityHUDArt::Label(WidgetTree,Canvas,FText::GetEmpty(),TextSlot.Position,TextSlot.Size,TextSlot.FontSize,true);
  Label->SetColorAndOpacity(TextSlot.Color);
  Label->SetWrapTextAt(0);
  ProductionLabels.Add(Label); ProductionSlots.Add(I);
 }
}
FText UDYKResourceBuildingWidget::ProductionText(const FDYKProductionTextSlot& TextSlot) const
{
 const FDYKProductionRecipeView* Recipe=View.Recipes.IsValidIndex(TextSlot.Index) ? &View.Recipes[TextSlot.Index] : nullptr;
 switch(TextSlot.Field)
 {
 case EDYKProductionField::BuildingName:return View.BuildingName;
 case EDYKProductionField::Description:return View.Description;
 case EDYKProductionField::ProductName:return View.ResourceName;
 case EDYKProductionField::Stored:return FText::AsNumber(View.Stored,&FNumberFormattingOptions::DefaultNoGrouping());
 case EDYKProductionField::Queue:return FText::FromString(FString::Printf(TEXT("%d / %d"),View.Orders,View.QueueCapacity));
 case EDYKProductionField::WorkTime:return FText::FromString(FormatWorkTime(View.bWorking ? View.Seconds : 0));
 case EDYKProductionField::RecipeAmount:return Recipe ? FText::AsNumber(Recipe->Amount,&FNumberFormattingOptions::DefaultNoGrouping()) : FText::GetEmpty();
 case EDYKProductionField::RecipeDuration:return Recipe ? FText::FromString(RecipeDuration(Recipe->Seconds)) : FText::GetEmpty();
 case EDYKProductionField::Speed:return FText::FromString(FString::Printf(TEXT("%+d%%"),View.SpeedPercent));
 case EDYKProductionField::QueueSlot:return FText::AsNumber(TextSlot.Index+1);
 default:return FText::GetEmpty();
 }
}
const UTextBlock* UDYKResourceBuildingWidget::FindField(EDYKProductionField Field,int32 Index) const
{
 if(!Definition) return nullptr;
 for(int32 I=0;I<ProductionSlots.Num();++I)
 {const auto& TextSlot=Definition->TextSlots[ProductionSlots[I]];if(TextSlot.Field==Field && TextSlot.Index==Index)return ProductionLabels[I];}
 return nullptr;
}
