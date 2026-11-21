#pragma once
#include "../UI/DYKScreenWidget.h"
#include "Components/Button.h"
#include "DYKCityBuildingUI.h"
#include "../UI/DYKResourceBuildingWidget.h"
#include "DYKCityWorkWidget.generated.h"
class UCanvasPanel;
class UTextBlock;
class USizeBox;
class ADYKCityBuildingPreviewActor;
class UDYKCityWorkWidget;
UENUM()
enum class EDYKCityUIAction : uint8
{Close,Back,BuildTab,LibraryTab,Build,Visit,Move,Rotate,Upgrade,CancelWork,Collect,Order,Item,Previous,Next,Research,CancelResearch,ResearchPrevious,ResearchNext};
UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityActionButton : public UButton
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UDYKCityWorkWidget> Screen;
	UPROPERTY() EDYKCityUIAction Action=EDYKCityUIAction::Close;
	UPROPERTY() int32 Index=0;
	UPROPERTY() FName Key;
	UPROPERTY() int64 Generation=0;
	void BindHoverContent(UWidget* Content);
	bool bHoverOnly=false;
	UFUNCTION() void Invoke();
private:
	UPROPERTY() TObjectPtr<UWidget> HoverContent;
	UFUNCTION() void ShowHoverContent();
	UFUNCTION() void HideHoverContent();
};
UCLASS()
class DEFENDYOURKINGDOM_API UDYKCityWorkWidget : public UDYKScreenWidget
{
	GENERATED_BODY()
public:
	UFUNCTION() void Refresh();
	UFUNCTION() void ToggleClicked();
	UFUNCTION() void CloseClicked();
	void OpenBuilding(FGuid Instance,FName Type);
	void Dispatch(EDYKCityUIAction Action,int32 Index=0,FName Key=NAME_None);
	bool BlocksWorldInput() const;
	const FDYKCityWindowState& GetWindowState() const {return State;}
	int64 GetRenderGeneration() const {return RenderGeneration;}
	UDYKCityActionButton* FindAction(EDYKCityUIAction Action,int32 Index=0) const;
	UPROPERTY() TObjectPtr<UButton> DevelopmentButton;
	UPROPERTY() TObjectPtr<UCanvasPanel> DesignCanvas;
	UPROPERTY() TObjectPtr<UWidget> ModalRoot;
	UPROPERTY() TObjectPtr<UButton> ModalBlocker;
	UPROPERTY() TArray<TObjectPtr<UDYKCityActionButton>> Actions;
	UPROPERTY() TObjectPtr<UDYKResourceBuildingWidget> ResourceScreen;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
private:
	void RebuildModal();
	void BuildConstruction(const FDYKPlayerProfileSaveData& P);
	void BuildBuilding(const FDYKPlayerProfileSaveData& P);
	void BuildInterior(const FDYKPlayerProfileSaveData& P);
	bool BuildRecruitmentCatalog(const FDYKPlayerProfileSaveData& P,const FText& Title,TConstArrayView<FDYKCityInteriorItem> Items);
	void BuildDetail(const FDYKPlayerProfileSaveData& P);
	bool BuildUnitUpgradeDetail(const FDYKPlayerProfileSaveData& P,const FDYKCityInteriorItem& Item);
	UFUNCTION() void ResourceAction(EDYKResourceAction Action,int32 Index);
	FDYKResourceBuildingView ResourceView(const FDYKPlayerProfileSaveData& P) const;
	void PositionResourceWindow();
	TArray<FName> ResearchOptions(const FDYKPlayerProfileSaveData& P,const FDYKCityInteriorItem& Item) const;
	UDYKCityActionButton* ActionButton(EDYKCityUIAction Action,int32 Index,const FText& Label,FVector2D Position,FVector2D Size,FName Key=NAME_None,bool Enabled=true,bool Transparent=false);
	FDYKCityWindowState State;
	int64 RenderGeneration=0;
	int32 ResearchChoice=0;
	FString Message;
	FTimerHandle WorkTimer;
	FGuid PendingCollect;
	FName LiveResearch;
	UPROPERTY() TObjectPtr<UTextBlock> LiveStatus;
	UPROPERTY() TObjectPtr<ADYKCityBuildingPreviewActor> SitePreview;
	UPROPERTY() TObjectPtr<USizeBox> DesignSize;
	FName SitePreviewType;
};
