#pragma once
#include "DYKScreenWidget.h"
#include "DYKStartWidget.generated.h"

UCLASS()
class DEFENDYOURKINGDOM_API UDYKStartWidget : public UDYKScreenWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> EnterButton;
	UFUNCTION() void EnterClicked();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
};
