#pragma once
#include "../UI/DYKScreenWidget.h"
#include "DYKCityTerritoryWidget.generated.h"

class UTextBlock;
class UButton;
class UBorder;
class UCanvasPanelSlot;

UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityTerritoryWidget : public UDYKScreenWidget
{
	GENERATED_BODY()
public:
	void Select(FIntPoint Parcel);
	void Refresh();
	UFUNCTION() void UnlockClicked();
	UPROPERTY() TObjectPtr<UButton> UnlockButton;
	UPROPERTY() TObjectPtr<UTextBlock> Details;
	UPROPERTY() TObjectPtr<UBorder> Popup;
	FIntPoint GetSelectedParcel() const { return Selected; }
	bool IsPopupVisible() const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
	bool PositionPopup(const FGeometry& Geometry);
	UPROPERTY() TObjectPtr<UCanvasPanelSlot> PopupSlot;
	FIntPoint Selected = FIntPoint(-1,-1);
	FGuid PendingCommand;
	bool bFailed = false;
	float AwaySeconds = 0.f;
	FVector2D PopupPosition = FVector2D::ZeroVector;
	FVector2D PopupSize = FVector2D::ZeroVector;
};
