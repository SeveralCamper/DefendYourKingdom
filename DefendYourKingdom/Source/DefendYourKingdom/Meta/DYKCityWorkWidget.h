#pragma once
#include "../UI/DYKScreenWidget.h"
#include "DYKCityWorkWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class ADYKCityBuildingPreviewActor;

UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityWorkWidget : public UDYKScreenWidget
{
	GENERATED_BODY()
public:
	UFUNCTION() void Refresh();
	UFUNCTION() void ToggleClicked();
	UFUNCTION() void PreviousClicked();
	UFUNCTION() void NextClicked();
	UFUNCTION() void BuildClicked();
	UFUNCTION() void UpgradeClicked();
	UFUNCTION() void CancelClicked();
	UPROPERTY() TObjectPtr<UTextBlock> Details;
	UPROPERTY() TObjectPtr<UVerticalBox> Menu;
	UPROPERTY() TObjectPtr<UButton> BuildButton;
	UPROPERTY() TObjectPtr<UButton> UpgradeButton;
	UPROPERTY() TObjectPtr<UButton> CancelWorkButton;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
private:
	int32 CatalogIndex=0;
	bool bOpen=false;
	FString WorkMessage;
	FTimerHandle WorkTimer;
	UPROPERTY() TObjectPtr<ADYKCityBuildingPreviewActor> SitePreview;
	FName SitePreviewType;
};
