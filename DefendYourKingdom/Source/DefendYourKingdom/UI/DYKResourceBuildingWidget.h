#pragma once
#include "Blueprint/UserWidget.h"
#include "Engine/DataAsset.h"
#include "Components/Button.h"
#include "DYKResourceBuildingWidget.generated.h"
class UCanvasPanel;
class UTextBlock;
class UTexture2D;
class UImage;

UENUM(BlueprintType)
enum class EDYKResourceAction:uint8 {None,Close,Back,Collect,Upgrade,CancelWork,Visit,Rotate,Move,Order};
USTRUCT(BlueprintType)
struct FDYKResourceHUDPart
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Key;
	UPROPERTY(EditAnywhere) TObjectPtr<UTexture2D> Texture;
	UPROPERTY(EditAnywhere) FVector2D Position=FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere) FVector2D Size=FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere) bool bInterior=false;
	UPROPERTY(EditAnywhere) EDYKResourceAction Action=EDYKResourceAction::None;
	UPROPERTY(EditAnywhere) int32 Index=0;
};
UCLASS(BlueprintType)
class DEFENDYOURKINGDOM_API UDYKResourceHUDDefinition:public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) FName BuildingId;
	UPROPERTY(EditAnywhere) TArray<FDYKResourceHUDPart> Parts;
	UPROPERTY(EditAnywhere) FVector2D FirstSize=FVector2D(1122,1402);
	UPROPERTY(EditAnywhere) FVector2D InteriorSize=FVector2D(1448,1086);
	UPROPERTY(EditAnywhere) TObjectPtr<UTexture2D> ResourceIcon;
};
UCLASS()
class DEFENDYOURKINGDOM_API UDYKResourceHUDCatalog:public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) TArray<TSoftObjectPtr<UDYKResourceHUDDefinition>> Definitions;
};
struct FDYKResourceBuildingView
{
	FGuid Instance;
	int64 Stored=0;
	int64 Capacity=0;
	int32 Orders=0;
	int64 Seconds=0;
	bool bWorking=false;
	bool bCanCollect=false;
	bool bCanUpgrade=false;
	bool bCanCancel=false;
	bool bCanVisit=true;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDYKResourceActionEvent,EDYKResourceAction,Action,int32,Index);
class UDYKResourceBuildingWidget;
UCLASS()
class UDYKResourceHUDButton:public UButton
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UDYKResourceBuildingWidget> Screen;
	EDYKResourceAction Action=EDYKResourceAction::None;
	int32 Index=0;
	int32 Generation=0;
	UFUNCTION() void Invoke();
};
UCLASS()
class DEFENDYOURKINGDOM_API UDYKResourceBuildingWidget:public UUserWidget
{
	GENERATED_BODY()
public:
	static UDYKResourceHUDDefinition* FindDefinition(FName Building);
	void Configure(UDYKResourceHUDDefinition* Content,bool Interior);
	void UpdateView(const FDYKResourceBuildingView& Value);
	void Activate(EDYKResourceAction Action,int32 Index,int32 ExpectedGeneration);
	void Deactivate() {++Generation; OnAction.Clear();}
	UDYKResourceHUDButton* FindAction(EDYKResourceAction Action,int32 Index=0) const;
	static FVector2D PopupPosition(FVector2D Anchor,FVector2D Viewport,FVector2D Popup);
	FDYKResourceActionEvent OnAction;
	const FDYKResourceBuildingView& GetView() const {return View;}
	bool IsInterior() const {return bInterior;}
	UPROPERTY() TObjectPtr<UDYKResourceHUDDefinition> Definition;
	UPROPERTY() TArray<TObjectPtr<UDYKResourceHUDButton>> Buttons;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	void Build();
	FDYKResourceBuildingView View;
	bool bInterior=false;
	int32 Generation=0;
	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY() TObjectPtr<UTextBlock> StoredText;
	UPROPERTY() TObjectPtr<UTextBlock> OrderText;
	UPROPERTY() TObjectPtr<UTextBlock> TimerText;
	UPROPERTY() TArray<TObjectPtr<UImage>> QueueIcons;
};
