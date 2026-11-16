#include "DYKCityTerritoryWidget.h"
#include "DYKCityTerritory.h"
#include "DYKCityTerritoryActor.h"
#include "DYKCityCatalog.h"
#include "DYKCityWidgets.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

void UDYKCityParcelButton::SelectParcel()
{
	if (auto* Panel=GetTypedOuter<UDYKCityTerritoryWidget>()) Panel->Select(Coordinate);
}
TSharedRef<SWidget> UDYKCityTerritoryWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* Paper=DYKUI::Panel(WidgetTree,this);
		auto* PaperSlot=Canvas->AddChildToCanvas(Paper);
		PaperSlot->SetAnchors(FAnchors(0.f,1.f)); PaperSlot->SetAlignment(FVector2D(0.f,1.f));
		PaperSlot->SetPosition(FVector2D(16.f,-16.f)); PaperSlot->SetAutoSize(true);
		auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Paper->AddChild(Box);
		Box->AddChild(DYKUI::Text(WidgetTree,FText::FromString(TEXT("Территория города · 5×5")),18));
		auto* Grid=WidgetTree->ConstructWidget<UUniformGridPanel>(); Box->AddChild(Grid);
		Grid->SetSlotPadding(FMargin(2.f));
		for (int32 Y=0; Y<5; ++Y) for (int32 X=0; X<5; ++X)
		{
			auto* Button=WidgetTree->ConstructWidget<UDYKCityParcelButton>();
			Button->Coordinate={X,Y}; Button->OnClicked.AddDynamic(Button,&UDYKCityParcelButton::SelectParcel);
			auto* Size=WidgetTree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(48.f); Size->SetHeightOverride(30.f);
			Size->AddChild(Button); Grid->AddChildToUniformGrid(Size,Y,X);
			auto* Text=DYKUI::Text(WidgetTree,FText::FromString(FString::Printf(TEXT("%d:%d"),X+1,Y+1)),14);
			Text->SetJustification(ETextJustify::Center); Button->AddChild(Text); ParcelButtons.Add(Button);
		}
		auto* Wrap=WidgetTree->ConstructWidget<USizeBox>(); Wrap->SetWidthOverride(270.f);
		Details=DYKUI::Text(WidgetTree,FText::GetEmpty(),14); Details->SetAutoWrapText(true); Wrap->AddChild(Details); Box->AddChild(Wrap);
		UnlockButton=DYKUI::Button(WidgetTree,this,FText::FromString(TEXT("Открыть участок")));
		UnlockButton->OnClicked.AddDynamic(this,&ThisClass::UnlockClicked); Box->AddChild(UnlockButton);
	}
	Refresh();
	return Super::RebuildWidget();
}
void UDYKCityTerritoryWidget::Select(FIntPoint Parcel)
{
	if (Selected!=Parcel) PendingCommand.Invalidate();
	Selected=Parcel; bFailed=false; Refresh();
}
void UDYKCityTerritoryWidget::Refresh()
{
	const auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto* P=Save ? Save->GetProfileSnapshot() : nullptr;
	if (!P || !Details || !UnlockButton) return;
	const auto& Config=DYKCityTerritory::DEV_PLACEHOLDER();
	for (const auto& Button : ParcelButtons)
	{
		const bool Open=DYKCityTerritory::IsUnlocked(*P,Button->Coordinate);
		Button->SetBackgroundColor(Button->Coordinate==Selected ? FLinearColor(.95f,.65f,.15f)
			: Open ? FLinearColor(.25f,.65f,.25f) : FLinearColor(.24f,.24f,.24f));
		Button->SetToolTipText(FText::FromString(Open ? TEXT("Открыт · 10×10 клеток") : TEXT("Закрыт · выберите для условий открытия")));
	}
	const bool Open=DYKCityTerritory::IsUnlocked(*P,Selected);
	FString Text=FString::Printf(TEXT("Участок %d:%d · 10×10 клеток\n"),Selected.X+1,Selected.Y+1);
	if (Open) Text+=TEXT("Открыт для размещения");
	else if (const auto* Rule=Config.ParcelRules.Find(Selected))
	{
		Text+=TEXT("Нужен открытый сосед по стороне.");
		for (const auto& Cost : Rule->Costs)
		{
			const auto Id=FDYKResourceId::FromContentId(Cost.ResourceId);
			if (Id) Text+=FString::Printf(TEXT("\n%s: %lld"),DYKCityCatalog::ResourceName(Id.GetValue()),Cost.Amount);
		}
		for (FName Key : Rule->Conditions)
		{
			const auto* Label=Config.ConditionLabels.Find(Key);
			Text+=TEXT("\n") + (Label ? *Label : FString(TEXT("Дополнительное условие открытия")));
		}
	}
	if (bFailed) Text+=TEXT("\nНе удалось открыть. Проверьте условия и повторите.");
	Details->SetText(FText::FromString(Text));
	UnlockButton->SetIsEnabled(!Open && DYKCityTerritory::CanUnlock(*P,Selected,Config));
}
void UDYKCityTerritoryWidget::UnlockClicked()
{
	auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	if (!Save) return;
	if (!PendingCommand.IsValid()) PendingCommand=FGuid::NewGuid();
	bFailed=!DYKCityTerritory::Unlock(*Save,PendingCommand,Selected,DYKCityTerritory::DEV_PLACEHOLDER());
	if (!bFailed) PendingCommand.Invalidate();
	for (TActorIterator<ADYKCityTerritoryActor> It(GetWorld()); It; ++It) It->Refresh();
	Refresh();
}
