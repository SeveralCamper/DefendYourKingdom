#include "../UI/DYKCityHUDArt.h"
#include "../UI/DYKCityPresentation.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontProviderInterface.h"
#include "Fonts/FontCache.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "UObject/StrongObjectPtr.h"

constexpr auto CitySlateContractFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityFontShapingTest,"DYK.CityHUD.SlateContracts.CyrillicUsesRealGlyphs",CitySlateContractFlags)
bool FDYKCityFontShapingTest::RunTest(const FString& Parameters)
{
	const auto* Skin=DYKCityHUDArt::Presentation(); if(!TestNotNull(TEXT("Presentation"),Skin)) return false;
	const auto* Provider=Cast<IFontProviderInterface>(Skin->Typeface.Get());
	TestNotNull(TEXT("Configured font supplies a composite font instead of LastResort"),Provider);
	if(Provider) TestTrue(TEXT("Runtime typeface has real entries"),Provider->GetCompositeFont() && !Provider->GetCompositeFont()->DefaultTypeface.Fonts.IsEmpty());
	TStrongObjectPtr<UWidgetTree> Tree(NewObject<UWidgetTree>());
	auto* Canvas=Tree->ConstructWidget<UCanvasPanel>(); Tree->RootWidget=Canvas;
	auto* Label=DYKCityHUDArt::Label(Tree.Get(),Canvas,FText::FromString(TEXT("Лесопилка")),{0,0},{600,80},52);
	auto Info=Label->GetFont(); Info.FontFallback=EFontFallback::FF_NoFallback;
	const FString Alphabet=TEXT("АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя0123456789");
	const auto Sequence=FSlateApplication::Get().GetRenderer()->GetFontCache()->ShapeUnidirectionalText(Alphabet,Info,1.f,TextBiDi::ETextDirection::LeftToRight,ETextShapingMethod::FullShaping);
	TSet<uint32> Distinct;
	for(const auto& Glyph:Sequence->GetGlyphsToRender())
	{
		TestTrue(TEXT("Cyrillic/digit glyph exists without fallback"),Glyph.GlyphIndex!=0);
		Distinct.Add(Glyph.GlyphIndex);
	}
	TestTrue(TEXT("Real letters, not one repeated substitute box"),Distinct.Num()>=70);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDYKCityPlateGeometryTest,"DYK.CityHUD.SlateContracts.PanelInteriorCannotCollapse",CitySlateContractFlags)
bool FDYKCityPlateGeometryTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWidgetTree> Tree(NewObject<UWidgetTree>());
	auto* Canvas=Tree->ConstructWidget<UCanvasPanel>(); Tree->RootWidget=Canvas;
	for(const auto Size:{FVector2D(180,48),FVector2D(226,56),FVector2D(355,64),FVector2D(940,118)})
	{
		DYKCityHUDArt::Plate(Tree.Get(),Canvas,{0,0},Size);
		const auto* Art=Cast<UImage>(Canvas->GetChildAt(Canvas->GetChildrenCount()-1));
		if(!TestNotNull(TEXT("Native panel image"),Art)) return false;
		const auto& B=Art->GetBrush(); const auto* Texture=Cast<UTexture2D>(B.GetResourceObject());
		if(!TestNotNull(TEXT("Actual texture, not only requested brush size"),Texture)) return false;
		// NullRHI may have no compiled platform mips: zero GetSizeX/Y cannot validate this.
		FVector2D SourceSize(Texture->GetSizeX(),Texture->GetSizeY());
#if WITH_EDITORONLY_DATA
		SourceSize=FVector2D(Texture->Source.GetSizeX(),Texture->Source.GetSizeY());
#endif
		TestTrue(TEXT("Real source dimensions are available"),SourceSize.X>0 && SourceSize.Y>0);
		AddInfo(FString::Printf(TEXT("Source %.0fx%.0f; panel %.0fx%.0f"),SourceSize.X,SourceSize.Y,Size.X,Size.Y));
		// Slate's box batcher uses the resource's ActualSize, ignoring Brush.ImageSize.
		// A border sum >= LocalSize collapses the center to a line (the reported regression).
		if(B.DrawAs!=ESlateBrushDrawType::Image)
		{
			TestTrue(TEXT("At least half of panel width remains interior"),SourceSize.X*(B.Margin.Left+B.Margin.Right)<Size.X*.5);
			TestTrue(TEXT("At least half of panel height remains interior"),SourceSize.Y*(B.Margin.Top+B.Margin.Bottom)<Size.Y*.5);
		}
		TestTrue(TEXT("Old .25 texture-height margins would collapse this panel"),SourceSize.Y*.5>=Size.Y);
		const FBox2D UV=B.GetUVRegion();
		TestTrue(TEXT("Texture sampling never has a zero-area region"),!UV.bIsValid || (UV.GetSize().X>0 && UV.GetSize().Y>0));
	}
	return true;
}
#endif
