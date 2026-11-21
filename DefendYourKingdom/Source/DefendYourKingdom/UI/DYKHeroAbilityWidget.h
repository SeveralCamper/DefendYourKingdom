#pragma once
#include "Blueprint/UserWidget.h"
#include "DYKHeroAbilityWidget.generated.h"
class ADYKDefenderActor;
class UButton;

UCLASS()
class DEFENDYOURKINGDOM_API UDYKHeroAbilityWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Configure(ADYKDefenderActor* Hero);
	void Refresh();
	UFUNCTION() void Activate();
	UPROPERTY() TObjectPtr<UButton> Button;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	TWeakObjectPtr<ADYKDefenderActor> Unit;
	FTimerHandle Timer;
};
