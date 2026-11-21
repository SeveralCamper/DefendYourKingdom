#include "DYKCityWorkWidget.h"
#include "DYKCityResearch.h"
#include "../Core/Content/DYKUnitCatalog.h"
#include "../UI/DYKCityHUDArt.h"
#include "../UI/DYKHeroPortraitWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateNoResource.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	const FVector2D Board(1448,1086);

	FSlateBrush CatalogBrush(UTexture2D* Atlas,FVector2D Source,FVector2D Size)
	{
		return DYKCityHUDArt::TextureBrush(Atlas,{Source/Board,(Source+Size)/Board});
	}

	void CatalogButtonStyle(UButton& Button,const FSlateBrush& Normal,const FSlateBrush& Hover)
	{
		FButtonStyle Style;
		Style.SetNormal(Normal).SetHovered(Hover).SetPressed(Hover).SetDisabled(Normal);
		Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
		Button.SetStyle(Style);
	}

	int32 CompletedStage(const FDYKPlayerProfileSaveData& Profile,const FDYKCityInteriorItem& Item)
	{
		const auto* Root=DYKCityResearch::Find(Item.ResearchRoot);
		if(!Root || !Profile.CityEconomy.CompletedResearch.Contains(Root->Id)) return 0;
		if(const auto* Unit=DYKUnitCatalog::Find(Root->Unlock)) return Unit->Tier;
		int32 Stage=0;
		FName Previous;
		for(const auto& Research:DYKCityResearch::DEV_PLACEHOLDER().Definitions)
		{
			if(Research.Id==Root->Id || (!Previous.IsNone() && Research.Prerequisites.Contains(Previous) && Research.Building==Root->Building))
			{
				if(!Profile.CityEconomy.CompletedResearch.Contains(Research.Id)) break;
				Previous=Research.Id;
				++Stage;
			}
		}
		return Stage;
	}
}

bool UDYKCityWorkWidget::BuildRecruitmentCatalog(const FDYKPlayerProfileSaveData& Profile,const FText& Title,
	TConstArrayView<FDYKCityInteriorItem> Items)
{
	auto* Background=LoadObject<UTexture2D>(nullptr,TEXT("/Game/DefendYourKingdom/Art/CityHUD/Recruitment/RecruitmentBase.RecruitmentBase"));
	auto* Parts=LoadObject<UTexture2D>(nullptr,TEXT("/Game/DefendYourKingdom/Art/CityHUD/Recruitment/RecruitmentParts.RecruitmentParts"));
	if(!Background || !Parts) return false;

	if(auto* Layout=Cast<UCanvasPanelSlot>(ModalRoot->Slot))
	{
		Layout->SetAnchors(FAnchors(.0833f,.0833f,.9167f,.9167f));
		Layout->SetOffsets(FMargin(0));
	}
	DYKCityHUDArt::Texture(WidgetTree,DesignCanvas,Background,{0,0},Board);
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,Title,{451,101},{546,77},62,true);
	const auto Sprite=[this,Parts](FVector2D Source,FVector2D Size,FVector2D Position)
	{
		return DYKCityHUDArt::Texture(WidgetTree,DesignCanvas,Parts,Position,Size,{Source/Board,(Source+Size)/Board});
	};

	const FSlateNoResource Empty;
	auto* Close=ActionButton(EDYKCityUIAction::Close,0,FText::GetEmpty(),{1252,84},{96,96},NAME_None,true,true);
	CatalogButtonStyle(*Close,Empty,Empty);
	Close->SetToolTipText(NSLOCTEXT("DYKCity","Close","Закрыть"));
	State.TurnPage(0,Items.Num());
	constexpr int32 CardsPerPage=6;
	const FVector2D CardSize(328,302);
	const double Columns[]={217,560,905};
	for(int32 CardIndex=0;CardIndex<CardsPerPage;++CardIndex)
	{
		const FVector2D Position(Columns[CardIndex%3],CardIndex<3 ? 244. : 574.);
		const int32 ItemIndex=State.Page*CardsPerPage+CardIndex;
		const auto* Item=Items.IsValidIndex(ItemIndex) ? &Items[ItemIndex] : nullptr;
		const bool Locked=!Item || DYKCityBuildingUI::IsLocked(Profile,*Item);
		if(Locked)
		{
			Sprite({560,574},CardSize,Position);
		}
		else
		{
			const FVector2D PortraitPosition=Position+FVector2D(10,9);
			const FVector2D PortraitSize(309,235);
			if(!DYKCityHUDArt::Illustration(WidgetTree,DesignCanvas,Item->Key,PortraitPosition,PortraitSize,true))
			{
				if(Item->Key==TEXT("CITY-TAVERN-0"))
				{
					auto* Portrait=WidgetTree->ConstructWidget<UDYKHeroPortraitWidget>();
					DYKCityHUDArt::Place(DesignCanvas,Portrait,PortraitPosition,PortraitSize);
				}
				else DYKCityHUDArt::Image(WidgetTree,DesignCanvas,Item->Art,Item->ImageRegion,PortraitPosition,PortraitSize);
			}
			Sprite({218,574},CardSize,Position);
			Sprite({230,482},{310,59},Position+FVector2D(10,240));
			DYKCityHUDArt::Label(WidgetTree,DesignCanvas,FText::FromString(Item->Name),Position+FVector2D(17,245),{294,43},33,true);
		}
		if(!Item) continue;
		auto* Hit=ActionButton(EDYKCityUIAction::Item,ItemIndex,FText::GetEmpty(),Position,CardSize,NAME_None,true,true);
		auto Highlight=CatalogBrush(Parts,{218,574},CardSize);
		Highlight.TintColor=FLinearColor(2.5f,1.65f,.46f);
		CatalogButtonStyle(*Hit,Empty,Highlight);
		if(Locked) continue;
		if(DYKCityBuildingUI::ShowNew(Profile,*Item))
			Sprite({214,236},{152,72},Position+FVector2D(-4,-7));
		else if(const int32 Stage=CompletedStage(Profile,*Item);Stage>0)
		{
			Sprite({569,227},{79,87},Position+FVector2D(8,-16));
			DYKCityHUDArt::Label(WidgetTree,DesignCanvas,FText::AsNumber(Stage),Position+FVector2D(23,-5),{48,54},41,true);
		}
	}

	const int32 Pages=FMath::Max(1,FMath::DivideAndRoundUp(Items.Num(),CardsPerPage));
	for(const bool Previous:{true,false})
	{
		const FVector2D Position=Previous ? FVector2D(548,894) : FVector2D(823,894);
		const FVector2D Size(79,70);
		const auto Action=Previous ? EDYKCityUIAction::Previous : EDYKCityUIAction::Next;
		auto* Button=ActionButton(Action,0,FText::GetEmpty(),Position,Size,NAME_None,
			Previous ? State.Page>0 : State.Page+1<Pages,true);
		const auto Normal=CatalogBrush(Parts,Position,Size);
		auto Hover=Normal;
		Hover.TintColor=FLinearColor(1.35f,1.16f,.88f);
		CatalogButtonStyle(*Button,Normal,Hover);
		Button->SetToolTipText(Previous ? NSLOCTEXT("DYKCity","PreviousCatalogPage","Предыдущая страница")
			: NSLOCTEXT("DYKCity","NextCatalogPage","Следующая страница"));
	}
	for(int32 Index=0;Index<Pages;++Index)
	{
		const bool Selected=Index==State.Page;
		Sprite(Selected ? FVector2D(655,908) : FVector2D(706,910),
			Selected ? FVector2D(38,39) : FVector2D(38,37),{705.+(Index-(Pages-1)*.5)*50,Selected ? 908. : 910.});
	}
	return true;
}
