#pragma once

#include "CoreMinimal.h"
#include "../UI/DYKScreenWidget.h"
#include "DYKPlacementWidget.generated.h"

class UButton;
class UTextBlock;
class ADYKUnitSlot;
struct FDYKWorldSelection;

// Native mission commands with a replaceable Widget Blueprint skin.
UCLASS()
class DEFENDYOURKINGDOM_API UDYKPlacementWidget : public UDYKScreenWidget
{
	GENERATED_BODY()
public:
	UFUNCTION() void PlaceClicked();
	UFUNCTION() void SellClicked();
	UFUNCTION() void ResurrectClicked();
	UFUNCTION() void StartClicked();
	UFUNCTION() void ReturnClicked();
	UFUNCTION() void Refresh();
	UFUNCTION() void SpeedOneClicked();
	UFUNCTION() void SpeedTwoClicked();
	UFUNCTION() void SpeedThreeClicked();
	UFUNCTION() void PauseClicked();
	UFUNCTION() void LeaveClicked();
	UFUNCTION() void ConfirmLeaveClicked();
	UFUNCTION() void CancelLeaveClicked();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> SpeedOneButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> SpeedTwoButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> SpeedThreeButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> PauseButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> LeaveButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> ConfirmLeaveButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UButton> CancelLeaveButton;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextBlock> HUDText;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextBlock> RewardText;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWidget> ConfirmationPanel;
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
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
private:
	void SetSpeed(float Speed);
	void SelectionChanged(const FDYKWorldSelection& Selection);
	TWeakObjectPtr<ADYKUnitSlot> SelectedSlot;
	FGuid DisplayedPreparation;
	FTimerHandle RefreshTimer;
	FString ReturnError;
};
