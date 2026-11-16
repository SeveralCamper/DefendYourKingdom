#include "DYKCityTerritoryWidget.h"
#include "DYKCityTerritory.h"
#include "DYKCityTerritoryActor.h"
#include "DYKCityCatalog.h"
#include "DYKCityEconomy.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetComponent.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	bool BlocksPointer(UWidget* Widget, const FVector2D& Pointer)
	{
		// SelfHitTestInvisible containers cover the viewport but do not own its input.
		if (Widget->GetVisibility()!=ESlateVisibility::Visible) return false;
		for (auto* Parent=Widget->GetParent(); Parent; Parent=Parent->GetParent())
		{
			const auto Visibility=Parent->GetVisibility();
			if (Visibility==ESlateVisibility::Hidden || Visibility==ESlateVisibility::Collapsed
				|| Visibility==ESlateVisibility::HitTestInvisible) return false;
		}
		const auto& Geometry=Widget->GetCachedGeometry();
		return Geometry.GetLocalSize().X>0 && Geometry.GetLocalSize().Y>0 && Geometry.IsUnderLocation(Pointer);
	}
	bool OtherHUDUnderPointer(const UDYKCityTerritoryWidget* Territory)
	{
		float MouseX=0.f,MouseY=0.f;
		auto* Player=Territory->GetOwningPlayer();
		if (!Player || !Player->GetMousePosition(MouseX,MouseY)) return false;
		// Use the same viewport pointer as world hover, then map it into widget geometry.
		const double Scale=FMath::Max(.01,double(UWidgetLayoutLibrary::GetViewportScale(Territory)));
		const auto Pointer=Territory->GetCachedGeometry().LocalToAbsolute(FVector2D(MouseX,MouseY)/Scale);
		TSet<UUserWidget*> AttachedWidgets;
		for (TObjectIterator<UWidgetComponent> It; It; ++It)
			if (It->GetWorld()==Territory->GetWorld() && It->IsRegistered() && It->IsVisible()
				&& It->IsWidgetVisible() && It->GetWidgetSpace()==EWidgetSpace::Screen
				&& It->GetOwner() && !It->GetOwner()->IsHidden())
			{
				FVector2D Screen;
				if (auto* PC=Territory->GetOwningPlayer(); PC && PC->ProjectWorldLocationToScreen(It->GetComponentLocation(),Screen))
				{
					int32 Width=0,Height=0; PC->GetViewportSize(Width,Height);
					const auto Margin=It->GetDrawSize();
					if (Screen.X>=-Margin.X && Screen.X<=Width+Margin.X && Screen.Y>=-Margin.Y && Screen.Y<=Height+Margin.Y)
						AttachedWidgets.Add(It->GetUserWidgetObject());
				}
			}
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			if (*It==Territory || It->GetWorld()!=Territory->GetWorld() || !It->WidgetTree
				|| (!It->IsInViewport() && !AttachedWidgets.Contains(*It))) continue;
			const auto Visibility=It->GetVisibility();
			if (Visibility==ESlateVisibility::Hidden || Visibility==ESlateVisibility::Collapsed
				|| Visibility==ESlateVisibility::HitTestInvisible) continue;
			bool Blocked=false;
			It->WidgetTree->ForEachWidget([&](UWidget* Widget) { Blocked |= BlocksPointer(Widget,Pointer); });
			if (Blocked) return true;
		}
		return false;
	}
}

TSharedRef<SWidget> UDYKCityTerritoryWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Canvas;
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Popup=DYKUI::Panel(WidgetTree,this);
		PopupSlot=Canvas->AddChildToCanvas(Popup);
		PopupSlot->SetAutoSize(true);
		auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Popup->AddChild(Box);
		auto* Wrap=WidgetTree->ConstructWidget<USizeBox>(); Wrap->SetWidthOverride(300.f);
		Details=DYKUI::Text(WidgetTree,FText::GetEmpty(),16);
		Details->SetAutoWrapText(true); Wrap->AddChild(Details); Box->AddChild(Wrap);
		UnlockButton=DYKUI::Button(WidgetTree,this,FText::FromString(TEXT("Открыть участок")));
		UnlockButton->OnClicked.AddDynamic(this,&ThisClass::UnlockClicked); Box->AddChild(UnlockButton);
		Popup->SetVisibility(ESlateVisibility::Hidden);
	}
	Refresh();
	return Super::RebuildWidget();
}

void UDYKCityTerritoryWidget::Select(FIntPoint Parcel)
{
	if (Selected!=Parcel) { PendingCommand.Invalidate(); bFailed=false; }
	Selected=Parcel; AwaySeconds=0.f; Refresh();
}

bool UDYKCityTerritoryWidget::IsPopupVisible() const
{
	return Popup && Popup->GetVisibility()==ESlateVisibility::Visible;
}

void UDYKCityTerritoryWidget::Refresh()
{
	const auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	const auto* P=Save ? Save->GetProfileSnapshot() : nullptr;
	if (!P || !Details || !UnlockButton || !Popup) return;
	const bool Inside=Selected.X>=0 && Selected.Y>=0 && Selected.X<5 && Selected.Y<5;
	if (!Inside || OtherHUDUnderPointer(this) || DYKCityTerritory::IsUnlocked(*P,Selected))
	{
		Popup->SetVisibility(ESlateVisibility::Hidden); UnlockButton->SetIsEnabled(false); return;
	}
	Popup->SetVisibility(ESlateVisibility::Visible);
	const auto& Config=DYKCityTerritory::DEV_PLACEHOLDER();
	FString Text=FString::Printf(TEXT("Участок %d:%d · 10×10 клеток"),Selected.X+1,Selected.Y+1);
	bool Adjacent=false;
	for (auto Delta : {FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
		Adjacent |= DYKCityTerritory::IsUnlocked(*P,Selected+Delta);
	Text+=Adjacent ? TEXT("\nСоседний открытый участок: есть") : TEXT("\nЗаблокировано: нет смежного открытого участка");
	if (const auto* Rule=Config.ParcelRules.Find(Selected))
	{
		if (Rule->Costs.IsEmpty()) Text+=TEXT("\nОткрытие бесплатно");
		for (const auto& Cost : Rule->Costs)
		{
			const auto Id=FDYKResourceId::FromContentId(Cost.ResourceId);
			if (!Id) { Text+=TEXT("\nЗаблокировано: неизвестная стоимость"); continue; }
			const int64 Have=DYKCityEconomy::Balance(*P,Id.GetValue());
			Text+=FString::Printf(TEXT("\n%s: %lld / %lld%s"),DYKCityCatalog::ResourceName(Id.GetValue()),Have,Cost.Amount,
				Have>=Cost.Amount ? TEXT(" · достаточно") : TEXT(" · не хватает"));
		}
		for (FName Key : Rule->Conditions)
		{
			const auto* Label=Config.ConditionLabels.Find(Key);
			const auto* Check=Config.Conditions.Find(Key);
			const bool Met=Check && bool(*Check) && (*Check)(*P);
			Text+=TEXT("\n")+(Label ? *Label : FString(TEXT("Дополнительное условие открытия")))
				+(Met ? TEXT(": выполнено") : TEXT(": не выполнено"));
		}
	}
	else Text+=TEXT("\nЗаблокировано: условия открытия не заданы");
	if (bFailed) Text+=TEXT("\nНе удалось сохранить открытие. Повторите попытку.");
	Details->SetText(FText::FromString(Text));
	UnlockButton->SetIsEnabled(DYKCityTerritory::CanUnlock(*P,Selected,Config));
}

bool UDYKCityTerritoryWidget::PositionPopup(const FGeometry& Geometry)
{
	auto* PC=GetOwningPlayer();
	if (!PC || !Popup || !PopupSlot || Popup->GetVisibility()!=ESlateVisibility::Visible) return true;
	const FIntPoint Cell=Selected*DYKCityTerritory::CellsPerParcel;
	float Bottom=-MAX_flt,Top=MAX_flt,Left=MAX_flt,Right=-MAX_flt;
	for (auto Offset : {FIntPoint(0,0),FIntPoint(10,0),FIntPoint(0,10),FIntPoint(10,10)})
	{
		FVector2D Projected;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,DYKCityTerritory::CellCorner(Cell+Offset),Projected,true))
		{
			Select(FIntPoint(-1,-1));
			return false;
		}
		Bottom=FMath::Max(Bottom,float(Projected.Y)); Top=FMath::Min(Top,float(Projected.Y));
		Left=FMath::Min(Left,float(Projected.X)); Right=FMath::Max(Right,float(Projected.X));
	}
	Popup->ForceLayoutPrepass(); PopupSize=Popup->GetDesiredSize();
	const FVector2D ViewSize=Geometry.GetLocalSize();
	if (Right<0 || Left>ViewSize.X || Bottom<0 || Top>ViewSize.Y)
	{
		Select(FIntPoint(-1,-1)); return false;
	}
	PopupPosition.X=FMath::Clamp((Left+Right)*.5-PopupSize.X*.5,8.,FMath::Max(8.,ViewSize.X-PopupSize.X-8.));
	PopupPosition.Y=FMath::Clamp(double(Bottom)+8.,8.,FMath::Max(8.,ViewSize.Y-PopupSize.Y-8.));
	PopupSlot->SetPosition(PopupPosition);
	return true;
}

void UDYKCityTerritoryWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
	Super::NativeTick(Geometry,DeltaTime);
	auto* PC=GetOwningPlayer();
	if (!PC || !Popup) return;
	// Foreign HUD wins even when this popup currently draws over that HUD.
	if (OtherHUDUnderPointer(this)) { Select(FIntPoint(-1,-1)); return; }
	if (!PositionPopup(Geometry)) return;
	float MouseX=0.f,MouseY=0.f;
	const bool HasMouse=PC->GetMousePosition(MouseX,MouseY);
	const double Scale=UWidgetLayoutLibrary::GetViewportScale(this);
	const FVector2D Mouse=FVector2D(MouseX,MouseY)/FMath::Max(.01,Scale);
	// Keep the popup stable while the pointer crosses the short gap to its button.
	const bool OverPopup=Popup->GetVisibility()==ESlateVisibility::Visible && HasMouse &&
		Mouse.X>=PopupPosition.X-8. && Mouse.X<=PopupPosition.X+PopupSize.X+8. &&
		Mouse.Y>=PopupPosition.Y-16. && Mouse.Y<=PopupPosition.Y+PopupSize.Y+8.;
	if (OverPopup || Popup->IsHovered()) { AwaySeconds=0.f; return; }
	FVector Origin,Direction;
	if (HasMouse && PC->DeprojectMousePositionToWorld(Origin,Direction) && Direction.Z < -KINDA_SMALL_NUMBER)
	{
		const FVector Ground=Origin-Direction*(Origin.Z/Direction.Z);
		const FVector Min=DYKCityTerritory::CellCorner(FIntPoint::ZeroValue);
		const double ParcelSize=DYKCityTerritory::CellSize*DYKCityTerritory::CellsPerParcel;
		const FIntPoint Parcel(FMath::FloorToInt((Ground.X-Min.X)/ParcelSize),FMath::FloorToInt((Ground.Y-Min.Y)/ParcelSize));
		const auto* Save=GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
		const auto* P=Save ? Save->GetProfileSnapshot() : nullptr;
		if (P && Parcel.X>=0 && Parcel.Y>=0 && Parcel.X<5 && Parcel.Y<5 && !DYKCityTerritory::IsUnlocked(*P,Parcel))
		{
			if (Selected!=Parcel || Popup->GetVisibility()!=ESlateVisibility::Visible) Select(Parcel);
			AwaySeconds=0.f; PositionPopup(Geometry); return;
		}
	}
	AwaySeconds+=DeltaTime;
	if (AwaySeconds>.25f && Selected!=FIntPoint(-1,-1)) Select(FIntPoint(-1,-1));
}

void UDYKCityTerritoryWidget::UnlockClicked()
{
	auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
	if (!Save || !Popup || Popup->GetVisibility()!=ESlateVisibility::Visible || OtherHUDUnderPointer(this)) return;
	if (!PendingCommand.IsValid()) PendingCommand=FGuid::NewGuid();
	bFailed=!DYKCityTerritory::Unlock(*Save,PendingCommand,Selected,DYKCityTerritory::DEV_PLACEHOLDER());
	if (!bFailed) PendingCommand.Invalidate();
	for (TActorIterator<ADYKCityTerritoryActor> It(GetWorld()); It; ++It) It->Refresh();
	Refresh();
}
