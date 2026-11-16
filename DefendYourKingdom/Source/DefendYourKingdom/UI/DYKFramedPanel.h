#pragma once
#include "Components/Border.h"
#include "DYKFramedPanel.generated.h"

// Decorative metal rim, wooden frame and parchment content; no input/game rules.
UCLASS()
class UDYKFramedPanel : public UBorder
{
	GENERATED_BODY()
public:
	UPROPERTY() FSlateBrush WoodFrame;
	UPROPERTY() FSlateBrush MetalRim;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
