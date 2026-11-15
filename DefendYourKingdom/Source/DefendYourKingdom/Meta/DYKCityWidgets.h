#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Core/Content/DYKContentId.h"
#include "DYKCityWidgets.generated.h"

class UTextBlock;
class UButton;

UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityCollectWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Configure(FGuid Instance, FDYKContentId Building);
	UFUNCTION() void CollectClicked();
	UFUNCTION() void Refresh();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> CollectButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(VisibleAnywhere) FGuid InstanceId;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	FDYKContentId BuildingId;
	FGuid PendingCommand;
	bool bSaveFailed = false;
	FTimerHandle RefreshTimer;
};

UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityResourceWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION() void Refresh();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextBlock> ResourceText;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	FTimerHandle RefreshTimer;
};
