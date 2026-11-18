#include "DYKCityHUDArt.h"
#include "DYKScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
FSlateBrush DYKCityHUDArt::Brush(FName Asset,FBox2D Region)
{
	FSlateBrush Result;
	const FString Path=FString::Printf(TEXT("/Game/DefendYourKingdom/Art/CityHUD/%s.%s"),*Asset.ToString(),*Asset.ToString());
	Result.SetResourceObject(LoadObject<UTexture2D>(nullptr,*Path)); Result.DrawAs=ESlateBrushDrawType::Image;
	Result.SetUVRegion(FBox2D(Region.Min/FVector2D(1448,1086),Region.Max/FVector2D(1448,1086))); Result.ImageSize=Region.GetSize(); return Result;
}
void DYKCityHUDArt::Place(UCanvasPanel* Canvas,UWidget* Widget,FVector2D Position,FVector2D Size)
{ auto* ArtSlot=Canvas->AddChildToCanvas(Widget); ArtSlot->SetPosition(Position); ArtSlot->SetSize(Size); }
UImage* DYKCityHUDArt::Image(UWidgetTree* Tree,UCanvasPanel* Canvas,FName Asset,FBox2D Source,FVector2D Position,FVector2D Size)
{
	auto* ImageWidget=Tree->ConstructWidget<UImage>(); ImageWidget->SetBrush(Brush(Asset,Source));
	ImageWidget->SetVisibility(ESlateVisibility::HitTestInvisible); Place(Canvas,ImageWidget,Position,Size); return ImageWidget;
}
void DYKCityHUDArt::Paper(UWidgetTree* Tree,UCanvasPanel* Canvas,FVector2D Position,FVector2D Size)
{ Image(Tree,Canvas,TEXT("WorkshopHUD"),FBox2D(FVector2D(248,797),FVector2D(540,829)),Position,Size); }
UTextBlock* DYKCityHUDArt::Label(UWidgetTree* Tree,UCanvasPanel* Canvas,const FText& Text,FVector2D Position,FVector2D Size,int32 Font,bool Light)
{
	auto* LabelWidget=DYKUI::Text(Tree,Text,Font); LabelWidget->SetAutoWrapText(true);
	LabelWidget->SetColorAndOpacity(Light ? FLinearColor(1,.91f,.70f,1) : FLinearColor(.18f,.085f,.025f,1));
	Place(Canvas,LabelWidget,Position,Size); return LabelWidget;
}
void DYKCityHUDArt::Frame(UWidgetTree* Tree,UCanvasPanel* Canvas,FName Asset,const FText& Title,const FText& Hint,bool ClearBody,bool FooterHint)
{
	Image(Tree,Canvas,Asset,FBox2D(FVector2D(0,0),FVector2D(1448,1086)),{0,0},{1448,1086});
	if(ClearBody) Paper(Tree,Canvas,{207,229},{1040,654});
	Image(Tree,Canvas,TEXT("WorkshopHUD"),FBox2D(FVector2D(455,171),FVector2D(970,189)),{440,100},{570,90});
	Label(Tree,Canvas,Title,{430,104},{590,83},52,true);
	if(FooterHint)
	{
		Image(Tree,Canvas,TEXT("WorkshopHUD"),FBox2D(FVector2D(452,945),FVector2D(1050,960)),{340,904},{840,48});
		Label(Tree,Canvas,Hint,{350,906},{820,47},22,true);
	}
}
