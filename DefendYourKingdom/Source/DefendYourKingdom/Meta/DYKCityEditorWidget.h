#pragma once

#include "../UI/DYKScreenWidget.h"
#include "DYKCityEditorWidget.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UVerticalBox;

UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityEditorWidget : public UDYKScreenWidget
{
	GENERATED_BODY()
public:
	void Refresh();
	bool BlocksWorldInput() const;
	UFUNCTION() void MoveClicked();
	UFUNCTION() void StoreClicked();
	UFUNCTION() void RotateClicked();
	UFUNCTION() void ApplyClicked();
	UFUNCTION() void CancelClicked();
	UFUNCTION() void NextClicked();
	UPROPERTY() TObjectPtr<UButton> MoveButton;
	UPROPERTY() TObjectPtr<UButton> StoreButton;
	UPROPERTY() TObjectPtr<UButton> RotateButton;
	UPROPERTY() TObjectPtr<UButton> ApplyButton;
	UPROPERTY() TObjectPtr<UButton> CancelButton;
	UPROPERTY() TObjectPtr<UButton> NextButton;
	UPROPERTY() TObjectPtr<UTextBlock> Details;
	UPROPERTY() TObjectPtr<UBorder> Panel;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
};

namespace DYKCityEditorInput
{
	DEFENDYOURKINGDOM_API bool CursorCell(APlayerController* PC, FIntPoint& Cell);
}
