#include "DYKCityPresentation.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"

bool UDYKCityPresentation::ConfigureRuntimeFont(UFont* Font,UFontFace* Face)
{
	if(!Font || !Face || !Face->GetFontFaceData()->HasData()) return false;
	Font->FontCacheType=EFontCacheType::Runtime;
	Font->CompositeFont=FCompositeFont();
	FTypefaceEntry Entry; Entry.Name=TEXT("Regular"); Entry.Font=FFontData(Face);
	Font->CompositeFont.DefaultTypeface.Fonts.Add(MoveTemp(Entry));
#if WITH_EDITORONLY_DATA
	Font->CompositeFont.MakeDirty();
#endif
	Font->MarkPackageDirty(); return true;
}
