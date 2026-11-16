#pragma once
#include "Blueprint/UserWidget.h"
#include "DYKUnitHealthWidget.generated.h"
class ADYKCombatUnit;
class UProgressBar;

UCLASS()
class DEFENDYOURKINGDOM_API UDYKUnitHealthWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Configure(ADYKCombatUnit* InUnit);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	void RefreshHealth();
	TWeakObjectPtr<ADYKCombatUnit> Unit;
	UPROPERTY() TObjectPtr<UProgressBar> Bar;
	FTimerHandle RefreshTimer;
};
