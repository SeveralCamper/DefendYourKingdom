#include "DYKCityWorkWidget.h"
#include "DYKHeroes.h"
#include "DYKCityEconomy.h"
#include "../Core/Content/DYKUnitCatalog.h"
#include "../UI/DYKCityHUDArt.h"
#include "../UI/DYKHeroPortraitWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

void UDYKCityWorkWidget::BuildPaladinDetail(const FDYKPlayerProfileSaveData& P)
{
	const auto Text=[](const FString& S){return FText::FromString(S);};
	DYKCityHUDArt::Frame(WidgetTree,DesignCanvas,TEXT("WorkshopHUD"),Text(TEXT("Паладин")),Text(Message),false,true);
	ActionButton(EDYKCityUIAction::Close,0,Text(TEXT("×")),{1240,76},{110,110});
	ActionButton(EDYKCityUIAction::Back,0,Text(TEXT("Назад")),{235,215},{180,48});
	const bool Owned=DYKHeroes::Owns(P);
	const auto Stats=DYKCityResearch::EffectiveStats(P,*DYKUnitCatalog::Find(DYKUnitIds::Paladin()));
	DYKCityHUDArt::Label(WidgetTree,DesignCanvas,Text(FString::Printf(TEXT("Уровень %d / 10    Золото: %lld\nHP %d    Урон %d    Броня %d / %d\nУдар молотом: %d урона, радиус 10"),
		DYKHeroes::Level(P),DYKCityEconomy::Balance(P,FDYKResourceId(TEXT("RES-GOLD"))),Stats.MaxHP.Value,Stats.Attack.DamagePerHit.Value,
		Stats.PhysicalArmor.Value,Stats.MagicalArmor.Value,DYKHeroes::Ability(&P).Damage.Value)),{470,255},{650,125},25,false);
	if(bHeroBiography)
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,Text(TEXT("Паладин защищает рубеж и обрушивает молот на врагов перед собой.")),{230,330},{230,290},24,false);
	else if(!DYKCityHUDArt::Illustration(WidgetTree,DesignCanvas,TEXT("CITY-TAVERN-0"),{235,330},{220,230},true))
		DYKCityHUDArt::Place(DesignCanvas,WidgetTree->ConstructWidget<UDYKHeroPortraitWidget>(),{235,330},{220,230});
	ActionButton(EDYKCityUIAction::HeroBiography,0,Text(bHeroBiography ? TEXT("Портрет") : TEXT("Описание")),{235,635},{220,52});
	if(!Owned)
	{
		const FString Reason=DYKHeroes::HireBlockReason(P);
		DYKCityHUDArt::Label(WidgetTree,DesignCanvas,Text(Reason),{490,410},{650,110},26,false);
		ActionButton(EDYKCityUIAction::HeroHire,0,Text(FString::Printf(TEXT("Нанять · %lld золота"),DYKHeroes::HireGoldDEV_PLACEHOLDER())),{490,550},{650,65},NAME_None,Reason.IsEmpty());
		return;
	}
	const TCHAR* Branches[]={TEXT("LEVEL"),TEXT("WEAPON"),TEXT("ARMOR"),TEXT("ABILITY")};
	const TCHAR* Names[]={TEXT("Уровень"),TEXT("Оружие"),TEXT("Броня"),TEXT("Способность")};
	for(int32 I=0;I<4;++I)
	{
		const FName Next=DYKHeroes::NextResearch(P,Branches[I]);
		const auto* D=DYKCityResearch::Find(Next);
		FString Label=FString::Printf(TEXT("%s · максимум"),Names[I]);
		if(D)
		{
			const int32 RequiredLevel=DYKHeroes::RequiredLevelDEV_PLACEHOLDER(DYKHeroes::BranchLevel(P,Branches[I])+1);
			Label=FString::Printf(TEXT("%s · %lld золота · %lld с%s"),Names[I],D->Costs[0].Amount,D->DurationSeconds,
				I>0 && DYKHeroes::Level(P)<RequiredLevel ? *FString::Printf(TEXT(" · нужен уровень %d"),RequiredLevel) : TEXT(""));
		}
		ActionButton(EDYKCityUIAction::HeroResearch,I,Text(Label),{490,415.+I*72},{650,60},Next,D && DYKCityResearch::CanEnqueue(P,Next));
	}
	int32 Row=0;
	for(const auto& W:P.CityEconomy.ResearchQueue)
	{
		const auto* D=DYKCityResearch::Find(W.ResearchId); if(!D || D->Building!=TEXT("CITY-TAVERN")) continue;
		if(Row>=2) break;
		const FString Status=W.StartedAtUnix>0 ? FString::Printf(TEXT("%lld с"),FMath::Max<int64>(0,W.CompletesAtUnix-FDateTime::UtcNow().ToUnixTimestamp())) : TEXT("ожидает ячейку");
		ActionButton(EDYKCityUIAction::CancelResearch,Row,Text(D->DisplayName+TEXT(" · ")+Status+TEXT(" · отменить")),{235,740.+Row*65},{905,56},FName(*W.WorkId.ToString()));
		++Row;
	}
}
