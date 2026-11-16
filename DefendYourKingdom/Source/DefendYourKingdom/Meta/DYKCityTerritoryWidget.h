#pragma once
#include "../UI/DYKScreenWidget.h"
#include "Components/Button.h"
#include "DYKCityTerritoryWidget.generated.h"

class UTextBlock;

UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityParcelButton : public UButton
{
	GENERATED_BODY()
public:
	FIntPoint Coordinate;
	UFUNCTION() void SelectParcel();
};

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
	UPROPERTY() TArray<TObjectPtr<UDYKCityParcelButton>> ParcelButtons;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	FIntPoint Selected={0,1};
	FGuid PendingCommand;
	bool bFailed=false;
};
