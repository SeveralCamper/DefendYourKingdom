#pragma once
#include "Styling/SlateBrush.h"
class UCanvasPanel;
class UImage;
class UTextBlock;
class UWidget;
class UWidgetTree;
namespace DYKCityHUDArt
{
	DEFENDYOURKINGDOM_API FSlateBrush Brush(FName Asset,FBox2D Region=FBox2D(FVector2D::ZeroVector,FVector2D(1448,1086)));
	DEFENDYOURKINGDOM_API void Place(UCanvasPanel* Canvas,UWidget* Widget,FVector2D Position,FVector2D Size);
	DEFENDYOURKINGDOM_API UImage* Image(UWidgetTree* Tree,UCanvasPanel* Canvas,FName Asset,FBox2D Source,FVector2D Position,FVector2D Size);
	DEFENDYOURKINGDOM_API void Paper(UWidgetTree* Tree,UCanvasPanel* Canvas,FVector2D Position,FVector2D Size);
	DEFENDYOURKINGDOM_API UTextBlock* Label(UWidgetTree* Tree,UCanvasPanel* Canvas,const FText& Text,FVector2D Position,FVector2D Size,int32 Font=28,bool Light=false);
	DEFENDYOURKINGDOM_API void Frame(UWidgetTree* Tree,UCanvasPanel* Canvas,FName Asset,const FText& Title,const FText& Hint,bool ClearBody=true,bool FooterHint=true);
}
