#include "DYKHeroes.h"
#include "DYKCityEconomy.h"
#include "../Saving/DYKProfileService.h"
#include "../Core/Content/DYKUnitCatalog.h"

bool DYKHeroes::Owns(const FDYKPlayerProfileSaveData& P)
{
	return P.OwnedHeroes.Contains(FDYKHeroId(DYKUnitIds::Paladin().GetName()).ToContentId());
}

int64 DYKHeroes::HireGoldDEV_PLACEHOLDER() { return 100; }
int32 DYKHeroes::RequiredLevelDEV_PLACEHOLDER(int32 Tier) {return Tier<=1 ? 1 : Tier==2 ? 3 : 6;}

FString DYKHeroes::UnlockBlockReason(const FDYKPlayerProfileSaveData& P)
{
	if(Owns(P)) return {};
	// Replaceable composite acquisition condition, never a blanket unlock bypass.
	if(!P.Buildings.ContainsByPredicate([](const auto& B){return B.BuildingId.Name==TEXT("CITY-TAVERN") && !B.bUnderConstruction;}))
		return TEXT("Требуется завершённая таверна.");
	if(!P.Buildings.ContainsByPredicate([](const auto& B){return B.BuildingId.Name==TEXT("CITY-CASTLE") && B.Level>=2 && !B.bUnderConstruction;}))
		return TEXT("Требуется замок уровня 2.");
	return {};
}

FString DYKHeroes::HireBlockReason(const FDYKPlayerProfileSaveData& P)
{
	if(Owns(P)) return TEXT("Паладин уже нанят навсегда.");
	const FString Locked=UnlockBlockReason(P);
	if(!Locked.IsEmpty()) return Locked;
	if(DYKCityEconomy::Balance(P,FDYKResourceId(TEXT("RES-GOLD")))<HireGoldDEV_PLACEHOLDER()) return TEXT("Недостаточно золота.");
	return {};
}

bool DYKHeroes::Hire(IDYKProfileService& Service,FGuid Command,int64 Now)
{
	return Service.ApplyProfileCommand(Command,[Now](auto& P)
	{
		if(!DYKCityResearch::Advance(P,Now) || !HireBlockReason(P).IsEmpty()) return false;
		TArray<FDYKCityWorkCost> Paid;
		if(!DYKCityWork::Pay(P,{{FDYKResourceId(TEXT("RES-GOLD")).ToContentId(),HireGoldDEV_PLACEHOLDER()}},Paid)) return false;
		P.OwnedHeroes.Add(FDYKHeroId(DYKUnitIds::Paladin().GetName()).ToContentId()); return true;
	});
}

FName DYKHeroes::ResearchId(FName Branch,int32 Step)
{ return FName(*FString::Printf(TEXT("HERO-PALADIN-%s-%d"),*Branch.ToString(),Step)); }

int32 DYKHeroes::BranchLevel(const FDYKPlayerProfileSaveData& P,FName Branch)
{
	const int32 Cap=Branch==TEXT("LEVEL") ? 9 : 3;
	int32 Completed=0;
	while(Completed<Cap && P.CityEconomy.CompletedResearch.Contains(ResearchId(Branch,Completed+1))) ++Completed;
	return Completed;
}

int32 DYKHeroes::Level(const FDYKPlayerProfileSaveData& P) { return 1+BranchLevel(P,TEXT("LEVEL")); }

FName DYKHeroes::NextResearch(const FDYKPlayerProfileSaveData& P,FName Branch)
{
	const int32 Step=BranchLevel(P,Branch)+1;
	return Step>(Branch==TEXT("LEVEL") ? 9 : 3) ? NAME_None : ResearchId(Branch,Step);
}

void DYKHeroes::AppendResearchDEV_PLACEHOLDER(FDYKResearchConfig& C)
{
	// Author explicitly approved temporary progression data for M2.08 (21.11.2026).
	const FName Owned(TEXT("HERO-PALADIN-OWNED"));
	C.Conditions.Add(Owned,[](const auto& P){return Owns(P);});
	const TCHAR* Branches[]={TEXT("LEVEL"),TEXT("WEAPON"),TEXT("ARMOR"),TEXT("ABILITY")};
	const TCHAR* Names[]={TEXT("Уровень"),TEXT("Оружие"),TEXT("Броня"),TEXT("Способность")};
	for(int32 Branch=0;Branch<4;++Branch) for(int32 Step=1;Step<=(Branch==0 ? 9 : 3);++Step)
	{
		FDYKResearchDefinition D;
		D.Id=ResearchId(Branches[Branch],Step); D.Building=TEXT("CITY-TAVERN");
		D.DisplayName=FString::Printf(TEXT("Паладин · %s %d"),Names[Branch],Branch==0 ? Step+1 : Step);
		D.DurationSeconds=20; D.Costs={{FDYKResourceId(TEXT("RES-GOLD")).ToContentId(),int64(Branch==0 ? 25*(Step+1) : 50*Step)}};
		D.Conditions.Add(Owned);
		if(Step>1) D.Prerequisites.Add(ResearchId(Branches[Branch],Step-1));
		if(Branch>0)
		{
			const int32 RequiredLevel=RequiredLevelDEV_PLACEHOLDER(Step);
			const FName Gate(*FString::Printf(TEXT("HERO-PALADIN-LEVEL-GATE-%d"),RequiredLevel));
			C.Conditions.Add(Gate,[RequiredLevel](const auto& P){return Level(P)>=RequiredLevel;});
			D.Conditions.Add(Gate);
		}
		D.TargetType=DYKUnitIds::Paladin();
		if(Branch==0) D.Bonus={10,0,1};
		if(Branch==1) D.Bonus.Damage=2;
		if(Branch==2) { D.Bonus.PhysicalArmor=1; D.Bonus.MagicalArmor=1; }
		C.Definitions.Add(MoveTemp(D));
	}
}

FDYKHeroAbilityDefinition DYKHeroes::Ability(const FDYKPlayerProfileSaveData* P)
{
	FDYKHeroAbilityDefinition D;
	if(P && BranchLevel(*P,TEXT("ABILITY"))>0)
	{
		D.Damage.Value+=5*BranchLevel(*P,TEXT("ABILITY"));
		D.Damage.Source=EDYKDefinitionSource::DEV_PLACEHOLDER;
	}
	return D;
}

bool DYKHeroes::CanDeploy(FName Hero,TConstArrayView<FName> Player,TConstArrayView<FName> Story,bool bStory)
{
	if(Hero.IsNone()) return false;
	for(const auto Id:Player) if(Id==Hero) return false;
	for(const auto Id:Story) if(Id==Hero) return false;
	return bStory || Player.Num()<2;
}
