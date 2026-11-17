#pragma once

#include "Blueprint/UserWidget.h"
#include "DYKScreenWidget.generated.h"

class UTexture2D;
class UWidgetTree;
class UBorder;
class UButton;
class UTextBlock;

// Widget Blueprint children own replaceable presentation defaults, never game rules.
UCLASS()
class DEFENDYOURKINGDOM_API UDYKScreenWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UDYKScreenWidget(const FObjectInitializer& ObjectInitializer);
	UPROPERTY(EditDefaultsOnly, Category="Appearance") TObjectPtr<UTexture2D> PanelTexture;
	UPROPERTY(EditDefaultsOnly, Category="Appearance") TObjectPtr<UTexture2D> ButtonTexture;
	UPROPERTY(EditDefaultsOnly, Category="Appearance") FLinearColor Ink = FLinearColor(.12f, .075f, .035f, 1.f);
};

namespace DYKUI
{
	UBorder* Panel(UWidgetTree* Tree, const UDYKScreenWidget* Skin);
	UTextBlock* Text(UWidgetTree* Tree, const FText& Label, int32 Size = 18);
	UButton* Button(UWidgetTree* Tree, const UDYKScreenWidget* Skin, const FText& Label);
	UClass* ScreenClass(UClass* NativeClass, const TCHAR* BlueprintName);
}
