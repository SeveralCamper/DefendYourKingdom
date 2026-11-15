#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DYKPlacementWidget.generated.h"

class UButton;
class UTextBlock;
class ADYKUnitSlot;
struct FDYKWorldSelection;

// Small player-facing placement controls; the complete mission HUD follows in M1.10.
UCLASS()
class DEFENDYOURKINGDOM_API UDYKPlacementWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION() void PlaceClicked();
	UFUNCTION() void SellClicked();
	UFUNCTION() void ResurrectClicked();
	UFUNCTION() void StartClicked();
	UFUNCTION() void ReturnClicked();
	UFUNCTION() void Refresh();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> PlaceButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> SellButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> ResurrectButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> StartButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> ReturnButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextBlock> StatusText;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
private:
	void SelectionChanged(const FDYKWorldSelection& Selection);
	TWeakObjectPtr<ADYKUnitSlot> SelectedSlot;
	FGuid DisplayedPreparation;
	FTimerHandle RefreshTimer;
	FString ReturnError;
};
