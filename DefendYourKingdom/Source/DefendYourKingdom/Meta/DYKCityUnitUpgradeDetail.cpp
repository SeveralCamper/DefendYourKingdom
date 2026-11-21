#include "DYKCityWorkWidget.h"
#include "DYKCityEconomy.h"
#include "DYKCityResearch.h"
#include "../Core/Content/DYKUnitCatalog.h"
#include "../UI/DYKCityHUDArt.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateNoResource.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	const FVector2D AtlasSize(1448,1086);
	const FVector2D BoardSize(1448,1170);
	const FLinearColor Gold(1.f,.82f,.12f);

	FBox2D AtlasRegion(FVector2D Start,FVector2D Size)
	{
		return {Start/AtlasSize,(Start+Size)/AtlasSize};
	}

	struct FUpgradeRow
	{
		int32 FDYKResearchBonus::* Stat;
		int32 BaseValue;
		double Y;
	};

	const FDYKCityResearchWorkSaveData* QueuedResearch(const FDYKPlayerProfileSaveData& Profile,FName Research)
	{
		return Profile.CityEconomy.ResearchQueue.FindByPredicate([Research](const auto& Work)
		{
			return Work.ResearchId==Research;
		});
	}

}

void UDYKCityActionButton::BindHoverContent(UWidget* Content)
{
	HoverContent=Content;
	HideHoverContent();
	OnHovered.AddDynamic(this,&ThisClass::ShowHoverContent);
	OnUnhovered.AddDynamic(this,&ThisClass::HideHoverContent);
}

void UDYKCityActionButton::ShowHoverContent()
{
	if(HoverContent) HoverContent->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UDYKCityActionButton::HideHoverContent()
{
	if(HoverContent) HoverContent->SetVisibility(ESlateVisibility::Collapsed);
}

bool UDYKCityWorkWidget::BuildUnitUpgradeDetail(const FDYKPlayerProfileSaveData& Profile,const FDYKCityInteriorItem& Item)
{
	const auto* Root=DYKCityResearch::Find(Item.ResearchRoot);
	const auto* Unit=Root ? DYKUnitCatalog::Find(Root->Unlock) : nullptr;
	if(!Unit) return false;
	auto* Base=LoadObject<UTexture2D>(nullptr,TEXT("/Game/DefendYourKingdom/Art/CityHUD/UnitUpgrade/UnitUpgradeBase.UnitUpgradeBase"));
	auto* Parts=LoadObject<UTexture2D>(nullptr,TEXT("/Game/DefendYourKingdom/Art/CityHUD/UnitUpgrade/UnitUpgradeParts.UnitUpgradeParts"));
	if(!Base || !Parts) return false;

	DesignSize->SetHeightOverride(BoardSize.Y);
	if(auto* Layout=Cast<UCanvasPanelSlot>(ModalRoot->Slot))
	{
		Layout->SetAnchors(FAnchors(.125f,.096f,.875f,.904f));
		Layout->SetOffsets(FMargin(0));
	}
	DYKCityHUDArt::Texture(WidgetTree,DesignCanvas,Base,{0,0},BoardSize);
	const auto Picture=[this,Parts](UCanvasPanel* Panel,FVector2D Source,FVector2D Size,FVector2D Position,FVector2D TargetSize)
	{
		DYKCityHUDArt::Texture(WidgetTree,Panel,Parts,Position,TargetSize,AtlasRegion(Source,Size));
	};
	const auto Label=[this](const FText& Text,FVector2D Pos,FVector2D Size,int32 Font=32,bool Light=true)
	{
		auto* Block=DYKCityHUDArt::Label(WidgetTree,DesignCanvas,Text,Pos,Size,Font,Light);
		Block->SetWrapTextAt(0);
		return Block;
	};
	const bool Locked=DYKCityBuildingUI::IsLocked(Profile,Item);
	Label(FText::FromString(Item.Name),{420,106},{608,76},58);
	const auto Portrait=FVector2D(225,205),PortraitSize=FVector2D(401,312);
	if(!Locked)
	{
		if(Unit->Id.GetName()==TEXT("DYK-PEASANT"))
			Picture(DesignCanvas,Portrait,PortraitSize,Portrait,PortraitSize);
		else if(!DYKCityHUDArt::Illustration(WidgetTree,DesignCanvas,Item.Key,{251,238},{344,251}))
			DYKCityHUDArt::Image(WidgetTree,DesignCanvas,Item.Art,Item.ImageRegion,{251,238},{344,251});
	}
	for(const auto Action:{EDYKCityUIAction::Close,EDYKCityUIAction::Back})
	{
		const bool Close=Action==EDYKCityUIAction::Close;
		auto* Button=ActionButton(Action,0,FText::GetEmpty(),Close ? FVector2D(1259,75) : FVector2D(1273,197),
			Close ? FVector2D(123,121) : FVector2D(97,84),NAME_None,true,true);
		FButtonStyle Transparent;
		const FSlateNoResource Empty;
		Transparent.SetNormal(Empty).SetHovered(Empty).SetPressed(Empty).SetDisabled(Empty);
		Button->SetStyle(Transparent);
		Button->SetToolTipText(Close ? NSLOCTEXT("DYKCity","Close","Закрыть") : NSLOCTEXT("DYKCity","Back","Назад"));
	}

	const FVector2D FooterOrigin(191,824);
	const FVector2D FooterSize(1067,188);
	struct FResourcePriceSlot {FName Id; FVector2D Position; FVector2D Size;};
	const FResourcePriceSlot PriceSlots[]={
		{TEXT("RES-WOOD"),{96,90},{106,50}}, {TEXT("RES-STONE"),{306,90},{105,50}},
		{TEXT("RES-IRON"),{526,90},{99,50}}, {TEXT("RES-GOLD"),{704,90},{133,50}},
		{TEXT("RES-FOOD"),{949,90},{103,50}}, {TEXT("RES-MEDALS"),{529,153},{104,31}}};
	Label(NSLOCTEXT("DYKCity","UnitUpgradeMedals","Медали"),{599,977},{120,31},22,false);
	const auto BindCost=[&](UDYKCityActionButton* Button,const FDYKResearchDefinition& Research)
	{
		auto* Panel=WidgetTree->ConstructWidget<UCanvasPanel>();
		DYKCityHUDArt::Place(DesignCanvas,Panel,FooterOrigin,FooterSize);
		for(const auto& Slot:PriceSlots)
		{
			const auto* Cost=Research.Costs.FindByPredicate([&Slot](const auto& Entry)
			{
				return Entry.ResourceId.Name==Slot.Id;
			});
			if(!Cost) continue;
			const int64 Owned=DYKCityEconomy::Balance(Profile,FDYKResourceId(Slot.Id));
			const auto Amount=FText::FromString(FString::Printf(TEXT("%lld / %lld"),Owned,Cost->Amount));
			const bool OnPlate=Slot.Id!=TEXT("RES-MEDALS");
			auto* Value=DYKCityHUDArt::Label(WidgetTree,Panel,Amount,Slot.Position,Slot.Size,OnPlate ? 28 : 22,OnPlate);
			Value->SetColorAndOpacity(Owned>=Cost->Amount ? (OnPlate ? Gold : FLinearColor(.23f,.13f,.06f))
				: FLinearColor(.84f,.20f,.11f));
		}
		Button->BindHoverContent(Panel);
	};

	const auto ResearchAction=[&](const FDYKResearchDefinition* Research,int32 Index,FVector2D Pos,FVector2D Size,const FText& Caption)
	{
		const auto* Job=Research ? QueuedResearch(Profile,Research->Id) : nullptr;
		const bool Available=Research && !Job && DYKCityResearch::CanEnqueue(Profile,Research->Id);
		const FName Key=Research ? Research->Id : NAME_None;
		auto* Button=ActionButton(EDYKCityUIAction::Research,Index,
			FText::GetEmpty(),Pos,Size,Key,Available,true,Research && !Job);
		FButtonStyle Style;
		const auto Brush=DYKCityHUDArt::TextureBrush(Parts,AtlasRegion({762,529},{264,62}));
		auto Muted=Brush;
		Muted.TintColor=FLinearColor(.60f,.63f,.55f);
		Style.SetNormal(Available ? Brush : Muted).SetHovered(Available ? Brush : Muted).SetPressed(Muted).SetDisabled(Muted);
		Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
		Button->SetStyle(Style);
		if(Research && !Job && !Research->Costs.IsEmpty()) BindCost(Button,*Research);
		Label(Job ? NSLOCTEXT("DYKCity","UnavailableWhileQueued","Недоступно") : Caption,
			Pos+FVector2D(Size.X*.29,2),{Size.X*.67,Size.Y-4},30);
	};

	const auto Options=Locked ? TArray<FName>() : ResearchOptions(Profile,Item);
	const auto& Definitions=DYKCityResearch::DEV_PLACEHOLDER().Definitions;
	const auto Stats=DYKCityResearch::EffectiveStats(Profile,*Unit);
	const FUpgradeRow Rows[]={
		{&FDYKResearchBonus::Damage,Stats.Attack.DamagePerHit.Value,529},
		{&FDYKResearchBonus::HP,Stats.MaxHP.Value,622},
		{&FDYKResearchBonus::PhysicalArmor,Stats.PhysicalArmor.Value,729}};
	const FVector2D UpgradeSize(264,62),CellSize(58,56);
	TSet<FName> StatResearch;
	for(int32 Index=0;Index<UE_ARRAY_COUNT(Rows);++Index)
	{
		const auto& Row=Rows[Index];
		for(int32 CellIndex=0;CellIndex<3;++CellIndex)
		{
			const FVector2D Cell(1054+61*CellIndex,Row.Y+3);
			Picture(DesignCanvas,{1113,532},CellSize,Cell,CellSize);
		}
		const FDYKResearchDefinition* Next=nullptr;
		int32 Stage=0;
		for(const auto& Research:Definitions)
		{
			if(Research.TargetType!=Unit->Id || Research.Bonus.*Row.Stat==0) continue;
			StatResearch.Add(Research.Id);
			if(!Next && Options.Contains(Research.Id)) Next=&Research;
			if(Stage<3)
			{
				const FVector2D Cell(1054+61*Stage,Row.Y+3);
				if(Profile.CityEconomy.CompletedResearch.Contains(Research.Id))
					Picture(DesignCanvas,{1054,532},CellSize,Cell,CellSize);
				else if(QueuedResearch(Profile,Research.Id))
					Label(NSLOCTEXT("DYKCity","QueuedUpgradeSlot","…"),Cell,CellSize,32)->SetColorAndOpacity(Gold);
			}
			++Stage;
		}
		const auto DrawStat=[&](int32 Current,int32 Delta,double X,double Y)
		{
			Label(Locked ? FText::FromString(TEXT("—")) : FText::AsNumber(Current),{X,Y},{54,40},33);
			if(Delta==0) return;
			Label(FText::FromString(TEXT("»")),{X+51,Y},{40,40},33)->SetColorAndOpacity(Gold);
			Label(FText::AsNumber(Current+Delta),{X+91,Y},{61,40},33)->SetColorAndOpacity(Gold);
		};
		DrawStat(Row.BaseValue,Next ? Next->Bonus.*Row.Stat : 0,Index==2 ? 609. : 565.,Index==2 ? 716. : Row.Y+9);
		if(Index==2) DrawStat(Stats.MagicalArmor.Value,0,609,756);
		ResearchAction(Next,Index,{762,Row.Y},UpgradeSize,
			Next ? NSLOCTEXT("DYKCity","ImproveUnitStat","Улучшить") : NSLOCTEXT("DYKCity","UnitUpgradeUnavailable","Недоступно"));
	}
	for(const FName Key:Options)
	{
		if(StatResearch.Contains(Key)) continue;
		const auto* Research=DYKCityResearch::Find(Key);
		if(!Research) continue;
		ResearchAction(Research,3,{951,421},{276,44},Research->Unlock.IsValid()
			? NSLOCTEXT("DYKCity","LearnUnit","Изучить бойца") : NSLOCTEXT("DYKCity","ImproveUnitStat","Улучшить"));
		break;
	}
	const FDYKCityResearchWorkSaveData* DisplayedJob=nullptr;
	for(const auto& Work:Profile.CityEconomy.ResearchQueue)
	{
		if(!Options.Contains(Work.ResearchId)) continue;
		if(Work.StartedAtUnix>0 || !DisplayedJob) DisplayedJob=&Work;
		if(Work.StartedAtUnix>0) break;
	}
	if(DisplayedJob)
	{
		auto* Cancel=ActionButton(EDYKCityUIAction::CancelResearch,0,FText::GetEmpty(),{333,1019},{365,76},
			FName(*DisplayedJob->WorkId.ToString()),true,true);
		const auto Normal=DYKCityHUDArt::TextureBrush(Parts,AtlasRegion({0,0},{422,88}));
		auto Hover=Normal;
		Hover.TintColor=FLinearColor(1.25f,1.13f,.9f);
		FButtonStyle Style;
		Style.SetNormal(Normal).SetHovered(Hover).SetPressed(Hover).SetDisabled(Normal);
		Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
		Cancel->SetStyle(Style);
		Cancel->SetToolTipText(NSLOCTEXT("DYKCity","CancelUnitUpgrade","Отменить работу"));
		LiveResearch=DisplayedJob->ResearchId;
		LiveStatus=Label(ResearchTimerText(Profile,*DisplayedJob,FDateTime::UtcNow().ToUnixTimestamp()),{717,1027},{397,60},32,false);
	}
	return true;
}
