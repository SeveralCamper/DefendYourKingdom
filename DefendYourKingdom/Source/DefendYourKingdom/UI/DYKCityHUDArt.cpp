#include "DYKCityHUDArt.h"
#include "DYKCityPresentation.h"
#include "DYKScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ScaleBox.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"

UDYKCityPresentation* DYKCityHUDArt::Presentation()
{return LoadObject<UDYKCityPresentation>(nullptr,TEXT("/Game/DefendYourKingdom/Art/CityHUD/CityPresentation.CityPresentation"));}

FSlateBrush DYKCityHUDArt::TextureBrush(UTexture2D* Texture,FBox2D UV)
{
	FSlateBrush Result; Result.SetResourceObject(Texture); Result.DrawAs=ESlateBrushDrawType::Image;
	if(Texture) Result.ImageSize=FVector2D(Texture->GetSizeX(),Texture->GetSizeY())*UV.GetSize();
	Result.SetUVRegion(UV); return Result;
}
FSlateBrush DYKCityHUDArt::Brush(FName Asset,FBox2D Region)
{
	const FString Path=FString::Printf(TEXT("/Game/DefendYourKingdom/Art/CityHUD/%s.%s"),*Asset.ToString(),*Asset.ToString());
	return TextureBrush(LoadObject<UTexture2D>(nullptr,*Path),FBox2D(Region.Min/FVector2D(1448,1086),Region.Max/FVector2D(1448,1086)));
}
void DYKCityHUDArt::Place(UCanvasPanel* Canvas,UWidget* Widget,FVector2D Position,FVector2D Size)
{auto* ArtSlot=Canvas->AddChildToCanvas(Widget); ArtSlot->SetPosition(Position); ArtSlot->SetSize(Size);}
UImage* DYKCityHUDArt::Texture(UWidgetTree* Tree,UCanvasPanel* Canvas,UTexture2D* Source,FVector2D Position,FVector2D Size,FBox2D UV)
{
	auto* Art=Tree->ConstructWidget<UImage>(); Art->SetBrush(TextureBrush(Source,UV));
	Art->SetVisibility(ESlateVisibility::HitTestInvisible); Place(Canvas,Art,Position,Size); return Art;
}
UImage* DYKCityHUDArt::Image(UWidgetTree* Tree,UCanvasPanel* Canvas,FName Asset,FBox2D Source,FVector2D Position,FVector2D Size)
{
	auto* Art=Tree->ConstructWidget<UImage>(); Art->SetBrush(Brush(Asset,Source));
	Art->SetVisibility(ESlateVisibility::HitTestInvisible); Place(Canvas,Art,Position,Size); return Art;
}
void DYKCityHUDArt::Plate(UWidgetTree* Tree,UCanvasPanel* Canvas,FVector2D Position,FVector2D Size)
{
	if(const auto* Skin=Presentation())
	{
		auto* Art=Texture(Tree,Canvas,Skin->ButtonPlate,Position,Size);
		auto B=Art->GetBrush(); B.DrawAs=ESlateBrushDrawType::Box; B.Margin=FMargin(.10f,.25f); B.ImageSize=FVector2D(240,72); B.SetUVRegion(FBox2D()); Art->SetBrush(B);
	}
}
void DYKCityHUDArt::Paper(UWidgetTree*,UCanvasPanel*,FVector2D,FVector2D)
{
	// Clean parchment belongs to the whole window, never stretched patch rectangles.
}
bool DYKCityHUDArt::Illustration(UWidgetTree* Tree,UCanvasPanel* Canvas,FName Key,FVector2D Position,FVector2D Size)
{
	const auto* Skin=Presentation(); const auto* Row=Skin ? Skin->Find(Key) : nullptr;
	if(!Row) return false;
	auto* Source=Row->Illustration.LoadSynchronous(); if(!Source) return false;
	auto* Art=Tree->ConstructWidget<UImage>(); Art->SetBrush(TextureBrush(Source,FBox2D(Row->UVMin,Row->UVMax)));
	auto* Fit=Tree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFill); Fit->SetClipping(EWidgetClipping::ClipToBounds);
	Fit->SetVisibility(ESlateVisibility::HitTestInvisible); Fit->AddChild(Art); Place(Canvas,Fit,Position,Size); return true;
}
UTextBlock* DYKCityHUDArt::Label(UWidgetTree* Tree,UCanvasPanel* Canvas,const FText& Text,FVector2D Position,FVector2D Size,int32 Font,bool Light)
{
	auto* LabelWidget=DYKUI::Text(Tree,Text,Font);
	if(const auto* Skin=Presentation(); Skin && Skin->Typeface) LabelWidget->SetFont(FSlateFontInfo(Skin->Typeface,Font));
	LabelWidget->SetAutoWrapText(false);
	if(Size.Y>Font*2.4) LabelWidget->SetWrapTextAt(Size.X-8);
	LabelWidget->SetColorAndOpacity(Light ? FLinearColor(1,.91f,.70f,1) : FLinearColor(.18f,.085f,.025f,1));
	LabelWidget->SetShadowOffset(Light ? FVector2D(0,1) : FVector2D::ZeroVector);
	LabelWidget->SetShadowColorAndOpacity(FLinearColor(.08f,.025f,.005f,.8f));
	auto* Fit=Tree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit); Fit->SetStretchDirection(EStretchDirection::DownOnly);
	Fit->SetVisibility(ESlateVisibility::HitTestInvisible); Fit->SetClipping(EWidgetClipping::ClipToBounds); Fit->AddChild(LabelWidget);
	Place(Canvas,Fit,Position,Size); return LabelWidget;
}
void DYKCityHUDArt::Frame(UWidgetTree* Tree,UCanvasPanel* Canvas,FName Asset,const FText& Title,const FText& Hint,bool,bool FooterHint)
{
	if(const auto* Skin=Presentation()) Texture(Tree,Canvas,Skin->Window,{0,0},{1448,1086});
	Image(Tree,Canvas,Asset,FBox2D({656,0},{793,120}),{656,0},{137,120});
	Label(Tree,Canvas,Title,{423,106},{600,78},52,true);
	if(FooterHint) Label(Tree,Canvas,Hint,{310,905},{850,47},25,true);
}
