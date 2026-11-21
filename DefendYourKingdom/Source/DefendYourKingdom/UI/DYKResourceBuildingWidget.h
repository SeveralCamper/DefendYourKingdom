#pragma once
#include "Blueprint/UserWidget.h"
#include "Engine/DataAsset.h"
#include "Components/Button.h"
#include "DYKResourceBuildingWidget.generated.h"
class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;

UENUM(BlueprintType)
enum class EDYKResourceAction:uint8 {None,Close,Back,Collect,Upgrade,CancelWork,Visit,Rotate,Move,Order};
USTRUCT(BlueprintType)
struct FDYKResourceHUDPart
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Key;
	UPROPERTY(EditAnywhere) TObjectPtr<UTexture2D> Texture;
	UPROPERTY(EditAnywhere) TObjectPtr<UTexture2D> UnavailableTexture;
	// Closed contour in normalized coordinates within the button's hit area.
	UPROPERTY(EditAnywhere) TArray<FVector2D> HoverOutline;
	UPROPERTY(EditAnywhere) FVector2D Position=FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere) FVector2D Size=FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere) bool bInterior=false;
	UPROPERTY(EditAnywhere) EDYKResourceAction Action=EDYKResourceAction::None;
	UPROPERTY(EditAnywhere) int32 Index=0;
};
// Shared layout fields; the values come from a building view, never from an ID branch.
UENUM(BlueprintType)
enum class EDYKProductionField:uint8
{BuildingName,Description,Stored,Queue,WorkTime,ProductName,RecipeAmount,RecipeDuration,Speed,QueueSlot,WorkName};
USTRUCT(BlueprintType)
struct FDYKProductionTextSlot
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) EDYKProductionField Field=EDYKProductionField::Stored;
 UPROPERTY(EditAnywhere) int32 Index=0;
 UPROPERTY(EditAnywhere) bool bInterior=false;
 UPROPERTY(EditAnywhere) FVector2D Position=FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere) FVector2D Size=FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere) int32 FontSize=28;
 UPROPERTY(EditAnywhere) FLinearColor Color=FLinearColor::White;
};
USTRUCT(BlueprintType)
struct FDYKProductionIllustrationSlot
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FVector2D Position=FVector2D::ZeroVector;
	UPROPERTY(EditAnywhere) FVector2D Size=FVector2D::ZeroVector;
};
USTRUCT(BlueprintType)
struct FDYKProductionRecipeImages
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) TArray<TObjectPtr<UTexture2D>> Images;
};
UCLASS(BlueprintType)
class DEFENDYOURKINGDOM_API UDYKResourceHUDDefinition:public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) FName BuildingId;
 // A single immutable template serves every Production entry in the city catalogue.
 UPROPERTY(EditAnywhere) bool bProductionTemplate=false;
 UPROPERTY(EditAnywhere) TArray<FDYKProductionTextSlot> TextSlots;
	UPROPERTY(EditAnywhere) TArray<FDYKProductionIllustrationSlot> InteriorIllustrationSlots;
	UPROPERTY(EditAnywhere) TMap<FName, FDYKProductionRecipeImages> InteriorIllustrations;
 UPROPERTY(EditAnywhere) FText DisplayName;
 UPROPERTY(EditAnywhere) FText Description;
 UPROPERTY(EditAnywhere) bool bFirstOnly=false;
 bool UsesAuthoredLayout() const {return bProductionTemplate || !TextSlots.IsEmpty();}
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
struct FDYKProductionRecipeView
{
	int64 Amount=0;
	int64 Seconds=0;
	TWeakObjectPtr<UTexture2D> Illustration{};
};
struct FDYKResourceBuildingView
{
	FGuid Instance;
 FText BuildingName;
 FText ResourceName;
 FText Description;
 TArray<FDYKProductionRecipeView> Recipes;
 int32 QueueCapacity=3;
 int32 SpeedPercent=0;
 bool bCanOrder=true;
 int32 Level=1;
 FString Process;
 FText WorkName;
 FText WorkTime;
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
	TArray<FVector2D> HoverOutline;
 bool bActionAvailable=true;
 FString UnavailableReason;
	void SetAvailability(bool Available,const FString& Reason);
 UFUNCTION() void Invoke();
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
};
UCLASS()
class DEFENDYOURKINGDOM_API UDYKResourceBuildingWidget:public UUserWidget
{
	GENERATED_BODY()
public:
	static UDYKResourceHUDDefinition* FindDefinition(FName Building);
 static FString FormatWorkTime(int64 Seconds);
 const UTextBlock* FindField(EDYKProductionField Field,int32 Index=0) const;
	void Configure(UDYKResourceHUDDefinition* Content,bool Interior);
	void UpdateView(const FDYKResourceBuildingView& Value);
 void SetFeedback(const FString& Text);
	void Activate(EDYKResourceAction Action,int32 Index,int32 ExpectedGeneration);
	void Deactivate() {++Generation; OnAction.Clear();}
	UDYKResourceHUDButton* FindAction(EDYKResourceAction Action,int32 Index=0) const;
	static FVector2D PopupSize(FVector2D Viewport,FVector2D DesignSize,double PreferredWidth=506.);
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
 void BuildProductionLabels();
 FText ProductionText(const FDYKProductionTextSlot& Slot) const;
 void UpdateButtons();
 UPROPERTY() TArray<TObjectPtr<UTextBlock>> ProductionLabels;
 TArray<int32> ProductionSlots;
	FDYKResourceBuildingView View;
	bool bInterior=false;
	int32 Generation=0;
	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY() TArray<TObjectPtr<UImage>> RecipeIllustrations;
	UPROPERTY() TObjectPtr<UTextBlock> Feedback;
 UPROPERTY() TObjectPtr<UTextBlock> ProcessText;
 UPROPERTY() TObjectPtr<UTextBlock> WorkText;
};
