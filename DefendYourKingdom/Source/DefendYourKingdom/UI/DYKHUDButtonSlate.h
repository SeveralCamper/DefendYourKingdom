#pragma once
#include "Widgets/Input/SButton.h"
#include "Rendering/DrawElements.h"
// Paint-only feedback: preserve source artwork, layout and hit area.
class SDYKHUDButton final: public SButton
{
public:
 bool bRound=false;
 FLinearColor HighlightColor=FLinearColor(1,.65f,.06f,1);
 FVector2D CircleCenter{.5,.48};
 double CircleRadius=.41;
 TArray<FVector2D> HoverOutline;
 virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& WidgetTint,bool ParentEnabled) const override
 {
  const int32 Top=SButton::OnPaint(Args,Geometry,Culling,Elements,Layer,WidgetTint,ParentEnabled);
  if(!ParentEnabled || !IsEnabled() || !(IsHovered() || HasKeyboardFocus())) return Top;
  const FVector2D Size=Geometry.GetLocalSize();
  TArray<FVector2D> Points;
  if(!HoverOutline.IsEmpty())
  {
   Points.Reserve(HoverOutline.Num());
   for(const FVector2D Point:HoverOutline) Points.Add(Point*Size);
  }
  else if(bRound)
  {
   const FVector2D Center=Size*CircleCenter;
   for(int32 I=0;I<=48;++I) {const double A=2*PI*I/48.; Points.Add(Center+FVector2D(FMath::Cos(A)*Size.X*CircleRadius,FMath::Sin(A)*Size.Y*CircleRadius));}
  }
  else
  {
   const double Inset=7, Cut=FMath::Min(19.,Size.Y*.22);
   Points={{Inset+Cut,Inset},{Size.X-Inset-Cut,Inset},{Size.X-Inset,Inset+Cut},{Size.X-Inset,Size.Y-Inset-Cut},{Size.X-Inset-Cut,Size.Y-Inset},{Inset+Cut,Size.Y-Inset},{Inset,Size.Y-Inset-Cut},{Inset,Inset+Cut},{Inset+Cut,Inset}};
  }
  const float Opacity=WidgetTint.GetColorAndOpacityTint().A;
  for(int32 Band=0;Band<3;++Band)
   FSlateDrawElement::MakeLines(Elements,Top+1,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,FLinearColor(HighlightColor.R,HighlightColor.G,HighlightColor.B,Opacity*(Band==0?.10f:Band==1?.22f:.8f)),true,Band==0?12.f:Band==1?7.f:2.f);
  return Top+1;
 }
};
