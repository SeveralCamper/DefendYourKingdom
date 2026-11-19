#pragma once

#include "CoreMinimal.h"
#include "../UI/DYKScreenWidget.h"
#include "DYKCityWidgets.generated.h"

class UTextBlock;
class UButton;

UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityResourceWidget : public UDYKScreenWidget
{
	GENERATED_BODY()
public:
	UFUNCTION() void Refresh();
	UFUNCTION() void CollectRewardsClicked();
	UFUNCTION() void MissionClicked();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> MissionButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextBlock> ResourceText;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> CollectRewardsButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextBlock> RewardText;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	FTimerHandle RefreshTimer;
	FGuid RewardCommand;
	bool bRewardSaveFailed = false;
};
