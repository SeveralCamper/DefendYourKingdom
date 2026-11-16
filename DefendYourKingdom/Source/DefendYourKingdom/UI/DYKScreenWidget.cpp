#include "DYKScreenWidget.h"
#include "DYKFramedPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

namespace
{
	FSlateBrush Paper(UTexture2D* Texture, FLinearColor Tint)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.DrawAs = Texture ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
		Brush.Margin = FMargin(.18f);
		Brush.TintColor = Tint;
		return Brush;
	}
}

UBorder* DYKUI::Panel(UWidgetTree* Tree, const UDYKScreenWidget* Skin)
{
	auto* Border = Tree->ConstructWidget<UDYKFramedPanel>();
	Border->WoodFrame = Paper(LoadObject<UTexture2D>(nullptr,TEXT("/Game/DefendYourKingdom/Art/UI/panel_brown.panel_brown")),FLinearColor::White);
	Border->MetalRim = Paper(LoadObject<UTexture2D>(nullptr,TEXT("/Game/DefendYourKingdom/Art/UI/buttonLong_grey.buttonLong_grey")),FLinearColor::White);
	Border->SetBrush(Paper(Skin->PanelTexture, Skin->PanelTexture ? FLinearColor::White : FLinearColor(.85f,.73f,.51f,1.f)));
	Border->SetPadding(FMargin(24.f, 18.f));
	return Border;
}

UTextBlock* DYKUI::Text(UWidgetTree* Tree, const FText& Label, int32 Size)
{
	auto* Text = Tree->ConstructWidget<UTextBlock>();
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
	Text->SetColorAndOpacity(FLinearColor(.12f,.075f,.035f,1.f));
	Text->SetText(Label);
	Text->SetJustification(ETextJustify::Center);
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Text;
}

UButton* DYKUI::Button(UWidgetTree* Tree, const UDYKScreenWidget* Skin, const FText& Label)
{
	auto* Button = Tree->ConstructWidget<UButton>();
	FButtonStyle Style = Button->GetStyle();
	Style.SetNormal(Paper(Skin->ButtonTexture, FLinearColor::White));
	Style.SetHovered(Paper(Skin->ButtonTexture, FLinearColor(1.15f,1.1f,.9f,1.f)));
	Style.SetPressed(Paper(Skin->ButtonTexture, FLinearColor(.72f,.69f,.55f,1.f)));
	Style.SetNormalPadding(FMargin(18.f, 10.f));
	Style.SetPressedPadding(FMargin(18.f, 11.f, 18.f, 9.f));
	Button->SetStyle(Style);
	auto* Text = DYKUI::Text(Tree, Label);
	Text->SetColorAndOpacity(Skin->Ink);
	Button->AddChild(Text);
	Button->SetToolTipText(Label);
	return Button;
}

UClass* DYKUI::ScreenClass(UClass* NativeClass, const TCHAR* BlueprintName)
{
	const FString Path = FString::Printf(TEXT("/Game/DefendYourKingdom/UI/%s.%s_C"), BlueprintName, BlueprintName);
	auto* Class = LoadClass<UUserWidget>(nullptr, *Path);
	return Class && Class->IsChildOf(NativeClass) ? Class : NativeClass;
}
