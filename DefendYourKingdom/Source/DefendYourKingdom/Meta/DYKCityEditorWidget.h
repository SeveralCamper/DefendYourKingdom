#pragma once

#include "../UI/DYKScreenWidget.h"
#include "Components/Button.h"
#include "DYKCityEditorWidget.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UVerticalBox;
class UScrollBox;
class UTextureRenderTarget2D;
class ADYKCityBuildingPreviewActor;
class UDYKCityEditorWidget;

UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityBufferButton : public UButton
{
	GENERATED_BODY()
public:
	UPROPERTY() FGuid InstanceId;
	UPROPERTY() TObjectPtr<UDYKCityEditorWidget> Editor;
	UFUNCTION() void SelectBuffered();
};

UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityEditorWidget : public UDYKScreenWidget
{
	GENERATED_BODY()
public:
	void Refresh();
	bool BlocksWorldInput() const;
	void BufferClicked(FGuid Instance);
	UButton* FindBufferButton(FGuid Instance) const;
	UTextureRenderTarget2D* FindThumbnail(FName BuildingType) const;
	ADYKCityBuildingPreviewActor* GetPlacementPreview() const { return PlacementPreview; }
	int32 BufferedCardCount() const { return BufferButtons.Num(); }
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
	UPROPERTY() TObjectPtr<UBorder> BufferPanel;
	UPROPERTY() TObjectPtr<UScrollBox> BufferScroll;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual void NativeDestruct() override;
private:
	void RefreshBuffer();
	UTextureRenderTarget2D* EnsureThumbnail(FName BuildingType);
	UPROPERTY() TArray<TObjectPtr<UDYKCityBufferButton>> BufferButtons;
	UPROPERTY() TMap<FName,TObjectPtr<UTextureRenderTarget2D>> Thumbnails;
	UPROPERTY() TArray<TObjectPtr<ADYKCityBuildingPreviewActor>> ThumbnailActors;
	UPROPERTY() TObjectPtr<ADYKCityBuildingPreviewActor> PlacementPreview;
	FName PreviewType;
};

namespace DYKCityEditorInput
{
	DEFENDYOURKINGDOM_API bool CursorCell(APlayerController* PC, FIntPoint& Cell);
}
