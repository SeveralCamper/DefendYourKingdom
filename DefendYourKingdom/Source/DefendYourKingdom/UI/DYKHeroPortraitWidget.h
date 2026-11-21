#pragma once
#include "Blueprint/UserWidget.h"
#include "DYKHeroPortraitWidget.generated.h"

// Replaceable monochrome helmet silhouette; no external texture or per-frame work.
UCLASS()
class DEFENDYOURKINGDOM_API UDYKHeroPortraitWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
