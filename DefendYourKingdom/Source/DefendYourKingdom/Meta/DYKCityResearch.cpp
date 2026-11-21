#include "DYKCityResearch.h"
#include "DYKCityEconomy.h"
#include "DYKCityOrders.h"
#include "DYKHeroes.h"
#include "../Core/Content/DYKUnitCatalog.h"
#include "../Saving/DYKProfileService.h"

namespace
{
	TArray<FDYKResourceSaveData> ResearchCosts(int64 FoodCost,int64 WoodCost,int64 StoneCost)
	{
		return {
			{FDYKResourceId(TEXT("RES-FOOD")).ToContentId(),FoodCost},
			{FDYKResourceId(TEXT("RES-WOOD")).ToContentId(),WoodCost},
			{FDYKResourceId(TEXT("RES-STONE")).ToContentId(),StoneCost}};
	}
	bool ResearchReady(const FDYKPlayerProfileSaveData& P,const FDYKResearchDefinition& D,const FDYKResearchConfig& C)
	{
		if(!D.Building.IsNone() && !P.Buildings.ContainsByPredicate([&D](const auto& B)
			{return B.BuildingId.Name==D.Building && !B.bUnderConstruction;})) return false;
		for(const auto Key:D.Prerequisites) if(!P.CityEconomy.CompletedResearch.Contains(Key)) return false;
		for(const auto Key:D.Conditions)
		{
			const auto* Check=C.Conditions.Find(Key);
			if(!Check || !*Check || !(*Check)(P)) return false;
		}
		return true;
	}
	bool ResearchBoundary(FDYKPlayerProfileSaveData& P,int64 Time,const FDYKResearchConfig& C)
	{
		for(int32 I=P.CityEconomy.ResearchQueue.Num()-1;I>=0;--I)
		{
			const auto& W=P.CityEconomy.ResearchQueue[I];
			if(W.StartedAtUnix==0 || W.CompletesAtUnix>Time) continue;
			const auto* D=DYKCityResearch::Find(W.ResearchId,C); if(!D) return false;
			P.CityEconomy.CompletedResearch.AddUnique(D->Id);
			if(D->Unlock.IsValid())
			{ P.ResearchedContent.AddUnique(D->Unlock); P.UnlockedContent.AddUnique(D->Unlock); }
			P.CityEconomy.ResearchQueue.RemoveAt(I);
		}
		const bool Active=P.CityEconomy.ResearchQueue.ContainsByPredicate([](const auto& W){return W.StartedAtUnix>0;});
		if(!Active) for(auto& W:P.CityEconomy.ResearchQueue)
		{
			const auto* D=DYKCityResearch::Find(W.ResearchId,C);
			if(!D || !ResearchReady(P,*D,C)) continue;
			if(D->DurationSeconds>MAX_int64-Time) return false;
			if(!DYKCityWork::Pay(P,D->Costs,W.Paid)) continue;
			W.StartedAtUnix=Time; W.CompletesAtUnix=Time+D->DurationSeconds; break;
		}
		P.CityEconomy.ResearchUpdatedAtUnix=Time;
		return true;
	}
}

const FDYKResearchConfig& DYKCityResearch::DEV_PLACEHOLDER()
{
	static const auto Config=[]
	{
		FDYKResearchConfig C;
		// Prices, bonuses and rank order are author data from spec/10.
		// Positive durations remain DEV_PLACEHOLDER.
		const TCHAR* Types[][3]={{TEXT("DYK-PEASANT"),TEXT("DYK-GUARD"),TEXT("DYK-ROYAL-GUARD")},
			{TEXT("DYK-ARCHER"),TEXT("DYK-VETERAN-ARCHER"),TEXT("DYK-CROSSBOWMAN")}};
		const TCHAR* Names[][3]={{TEXT("Крестьянин"),TEXT("Страж"),TEXT("Гвардеец")},
			{TEXT("Лучник"),TEXT("Опытный лучник"),TEXT("Арбалетчик")}};
		const TCHAR* LineKeys[]={TEXT("INFANTRY"),TEXT("RANGED")};
		const TCHAR* StatKeys[]={TEXT("HP"),TEXT("ARMOR"),TEXT("DAMAGE")};
		const TCHAR* StatNames[]={TEXT("Улучшенное снаряжение"),TEXT("Стальные заплатки"),TEXT("Мурштра")};
		const FDYKResearchBonus PeasantBonuses[]={{5,0,0},{0,1,0},{0,0,2}};
		for(int32 LineIndex=0;LineIndex<2;++LineIndex)
		{
			const auto Line=LineIndex==0 ? EDYKUnitLine::Infantry : EDYKUnitLine::Ranged;
			FName Previous;
			for(int32 Tier=1;Tier<=3;++Tier)
			{
				FDYKResearchDefinition D; D.Id=FName(*FString::Printf(TEXT("RESEARCH-%s-T%d"),LineKeys[LineIndex],Tier));
				D.Building=TEXT("CITY-BARRACKS");
				D.DisplayName=Names[LineIndex][Tier-1]; D.Unlock=FDYKUnitTypeId(Types[LineIndex][Tier-1]).ToContentId();
				if(!Previous.IsNone()) D.Prerequisites.Add(Previous);
				// T1 remains the existing free initial rank. The node supplies the explicit graph root.
				D.Costs=Tier==1 ? TArray<FDYKResourceSaveData>() : Tier==2 ? ResearchCosts(500,300,200) : ResearchCosts(1400,700,500);
				Previous=D.Id; C.Definitions.Add(D);
			}
			for(int32 Stat=0;Stat<3;++Stat)
			{
				FDYKResearchDefinition D; D.Id=FName(*FString::Printf(TEXT("RESEARCH-%s-%s"),LineKeys[LineIndex],StatKeys[Stat]));
				D.Building=TEXT("CITY-LIBRARY");
				D.DisplayName=FString::Printf(TEXT("%s: %s"),LineIndex==0 ? TEXT("Пехота") : TEXT("Стрелки"),StatNames[Stat]);
				D.Prerequisites.Add(FName(*FString::Printf(TEXT("RESEARCH-%s-T1"),LineKeys[LineIndex])));
				D.TargetLine=Line; D.Bonus=Stat==0 ? FDYKResearchBonus{10,0,0} : Stat==1 ? FDYKResearchBonus{0,1,0} : FDYKResearchBonus{0,0,2};
				D.Costs=Stat==1 ? ResearchCosts(1000,900,700) : ResearchCosts(800,300,500);
				C.Definitions.Add(D);
				for(int32 Tier=1;Tier<=3;++Tier)
				{
					const bool bPeasant=Line==EDYKUnitLine::Infantry && Tier==1;
					const int32 Steps=bPeasant ? 1 : Tier-1;
					FName Prior;
					for(int32 Level=1;Level<=Steps;++Level)
					{
						auto Personal=D;
						Personal.Building=TEXT("CITY-BARRACKS");
						Personal.Id=FName(*FString::Printf(TEXT("RESEARCH-%s-T%d-%s-%d"),LineKeys[LineIndex],Tier,StatKeys[Stat],Level));
						Personal.DisplayName=FString::Printf(TEXT("%s: %s %d"),Names[LineIndex][Tier-1],StatNames[Stat],Level);
						Personal.Prerequisites={FName(*FString::Printf(TEXT("RESEARCH-%s-T%d"),LineKeys[LineIndex],Tier))};
						if(!Prior.IsNone()) Personal.Prerequisites.Add(Prior);
						Personal.TargetLine=EDYKUnitLine::None; Personal.TargetType=FDYKUnitTypeId(Types[LineIndex][Tier-1]);
						if(bPeasant)
						{
							Personal.DurationSeconds=20;
							Personal.Bonus=PeasantBonuses[Stat];
							Personal.Costs=ResearchCosts(20,20,10); // DEV_PLACEHOLDER until the author sets prices.
						}
						Prior=Personal.Id; C.Definitions.Add(MoveTemp(Personal));
					}
				}
			}
			FDYKResearchDefinition Ability;
			Ability.Building=TEXT("CITY-BARRACKS");
			Ability.Id=FName(*FString::Printf(TEXT("RESEARCH-%s-T3-ABILITY"),LineKeys[LineIndex]));
			Ability.DisplayName=FString::Printf(TEXT("%s: уникальное умение"),Names[LineIndex][2]);
			Ability.Prerequisites={FName(*FString::Printf(TEXT("RESEARCH-%s-T3"),LineKeys[LineIndex]))};
			Ability.TargetType=FDYKUnitTypeId(Types[LineIndex][2]);
			// Unspecified price: replaceable DEV_PLACEHOLDER, no invented final ability rules.
			Ability.Costs={{FDYKResourceId(TEXT("RES-GOLD")).ToContentId(),20}};
			C.Definitions.Add(MoveTemp(Ability));
		}
		const TCHAR* TrapKeys[]={TEXT("SPIKES"),TEXT("DEATH-CLAWS"),TEXT("LOGS"),TEXT("PAIN-AVALANCHE")};
		const TCHAR* TrapNames[]={TEXT("Шипы"),TEXT("Когти смерти"),TEXT("Бревна"),TEXT("Лавина боли")};
		for(int32 Type=0;Type<4;++Type)
		{
			FName PreviousTrap;
			for(int32 Level=1;Level<=3;++Level)
			{
				FDYKResearchDefinition D;
				D.Id=FName(*FString::Printf(TEXT("RESEARCH-TRAP-%s-L%d"),TrapKeys[Type],Level));
				D.Building=TEXT("CITY-WORKSHOP"); D.DisplayName=FString::Printf(TEXT("%s · уровень %d"),TrapNames[Type],Level);
				if(!PreviousTrap.IsNone()) D.Prerequisites.Add(PreviousTrap);
				if(Level==1) D.Unlock=FDYKTrapId(FName(*FString::Printf(TEXT("DYK-TRAP-%s"),TrapKeys[Type]))).ToContentId();
				// Unspecified trap research prices/level count: replaceable DEV_PLACEHOLDER.
				D.Costs=ResearchCosts(10*Level,10*Level,5*Level);
				PreviousTrap=D.Id; C.Definitions.Add(MoveTemp(D));
			}
		}
		DYKHeroes::AppendResearchDEV_PLACEHOLDER(C);
		return C;
	}();
	return Config;
}

const FDYKResearchDefinition* DYKCityResearch::Find(FName Id,const FDYKResearchConfig& C)
{
	return C.Definitions.FindByPredicate([Id](const auto& D){return D.Id==Id;});
}

bool DYKCityResearch::ValidateConfig(const FDYKResearchConfig& C)
{
	TSet<FName> All;
	for(const auto& D:C.Definitions)
	{
		if(D.Id.IsNone() || All.Contains(D.Id) || D.DurationSeconds<=0 || D.Bonus.HP<0 || D.Bonus.PhysicalArmor<0 || D.Bonus.Damage<0 || D.Bonus.MagicalArmor<0
			|| (D.TargetLine!=EDYKUnitLine::None && D.TargetType.IsValid())) return false;
		All.Add(D.Id);
		TSet<FName> Costs;
		for(const auto& Cost:D.Costs)
		{
			if(!FDYKResourceId::FromContentId(Cost.ResourceId) || Cost.Amount<=0 || Costs.Contains(Cost.ResourceId.Name)) return false;
			Costs.Add(Cost.ResourceId.Name);
		}
		for(const auto Key:D.Conditions) if(!C.Conditions.Contains(Key) || !C.Conditions.FindChecked(Key)) return false;
	}
	// Topological validation also rejects dangling prerequisites and cycles.
	TSet<FName> Reached;
	while(Reached.Num()<All.Num())
	{
		const int32 Before=Reached.Num();
		for(const auto& D:C.Definitions)
		{
			bool Ready=true; TSet<FName> Dependencies;
			for(const auto Key:D.Prerequisites)
			{ if(Dependencies.Contains(Key)) return false; Dependencies.Add(Key); Ready&=Reached.Contains(Key); }
			if(Ready) Reached.Add(D.Id);
		}
		if(Reached.Num()==Before) return false;
	}
	return true;
}

bool DYKCityResearch::Validate(const FDYKPlayerProfileSaveData& P,const FDYKResearchConfig& C)
{
	if(!ValidateConfig(C) || !DYKCityWork::Validate(P) || P.CityEconomy.ResearchUpdatedAtUnix<0) return false;
	TSet<FName> Completed,Queued; TSet<FGuid> Ids; int32 Active=0;
	for(const auto Key:P.CityEconomy.CompletedResearch)
	{
		if(!Find(Key,C) || Completed.Contains(Key)) return false;
		Completed.Add(Key);
	}
	for(const auto Key:Completed) for(const auto Dependency:Find(Key,C)->Prerequisites) if(!Completed.Contains(Dependency)) return false;
	for(const auto& W:P.CityEconomy.ResearchQueue)
	{
		if(!W.WorkId.IsValid() || !Find(W.ResearchId,C) || Completed.Contains(W.ResearchId) || Queued.Contains(W.ResearchId)
			|| Ids.Contains(W.WorkId) || W.EnqueuedAtUnix<=0 || W.StartedAtUnix<0 || W.CompletesAtUnix<0) return false;
		Queued.Add(W.ResearchId); Ids.Add(W.WorkId);
		for(const auto Key:Find(W.ResearchId,C)->Prerequisites) if(!Completed.Contains(Key)) return false;
		if(W.StartedAtUnix>0)
		{ ++Active; if(W.StartedAtUnix<W.EnqueuedAtUnix || W.CompletesAtUnix<=W.StartedAtUnix) return false; }
		else if(W.CompletesAtUnix!=0 || !W.Paid.IsEmpty()) return false;
		TSet<FName> Costs;
		for(const auto& Cost:W.Paid)
		{ if(Cost.Amount<=0 || Costs.Contains(Cost.Resource)) return false; Costs.Add(Cost.Resource); }
	}
	return Active<=1;
}

bool DYKCityResearch::CanEnqueue(const FDYKPlayerProfileSaveData& P,FName Research,const FDYKResearchConfig& C)
{
	const auto* D=Find(Research,C);
	return D && Validate(P,C) && !P.CityEconomy.CompletedResearch.Contains(Research)
		&& !P.CityEconomy.ResearchQueue.ContainsByPredicate([Research](const auto& W){return W.ResearchId==Research;})
		&& ResearchReady(P,*D,C) && DYKCityWork::CanPay(P,D->Costs);
}

bool DYKCityResearch::Advance(FDYKPlayerProfileSaveData& P,int64 Now,const FDYKResearchConfig& C)
{
	if(Now<=0 || !Validate(P,C)) return false;
	// M1 already grants both initial ranks. Migrate their graph roots once, without
	// charging or researching the same starting units a second time.
	for(const auto Key:{FName(TEXT("RESEARCH-INFANTRY-T1")),FName(TEXT("RESEARCH-RANGED-T1"))})
	{
		const auto* Root=Find(Key,C);
		if(Root && Root->Prerequisites.IsEmpty() && Root->Costs.IsEmpty())
		{ P.CityEconomy.CompletedResearch.AddUnique(Key); P.ResearchedContent.AddUnique(Root->Unlock); }
	}
	Now=FMath::Max(Now,FMath::Max(P.CityEconomy.WorkUpdatedAtUnix,P.CityEconomy.ResearchUpdatedAtUnix));
	for(;;)
	{
		int64 Step=Now;
		for(const auto& W:P.CityEconomy.Work) if(W.StartedAtUnix>0) Step=FMath::Min(Step,W.CompletesAtUnix);
		for(const auto& W:P.CityEconomy.ResearchQueue) if(W.StartedAtUnix>0) Step=FMath::Min(Step,W.CompletesAtUnix);
		if(!DYKCityOrders::Advance(P,Step) || !DYKCityWork::Advance(P,Step) || !ResearchBoundary(P,Step,C)
			|| !DYKCityOrders::Advance(P,Step)) return false;
		if(Step==Now) break;
	}
	return Validate(P,C);
}

bool DYKCityResearch::Enqueue(IDYKProfileService& Service,FGuid Command,FName Research,int64 Now,const FDYKResearchConfig& C)
{
	return Service.ApplyProfileCommand(Command,[Research,Now,&C,Command](auto& P)
	{
		if(!Advance(P,Now,C) || !CanEnqueue(P,Research,C)) return false;
		FDYKCityResearchWorkSaveData W; W.WorkId=Command; W.ResearchId=Research;
		W.EnqueuedAtUnix=FMath::Max(Now,P.CityEconomy.ResearchUpdatedAtUnix); P.CityEconomy.ResearchQueue.Add(W);
		return Advance(P,Now,C);
	});
}

bool DYKCityResearch::Cancel(IDYKProfileService& Service,FGuid Command,FGuid Work,int64 Now,const FDYKResearchConfig& C)
{
	return Service.ApplyProfileCommand(Command,[Work,Now,&C](auto& P)
	{
		if(!Advance(P,Now,C)) return false;
		const int32 Index=P.CityEconomy.ResearchQueue.IndexOfByPredicate([Work](const auto& W){return W.WorkId==Work;});
		if(Index==INDEX_NONE || !DYKCityWork::Refund(P,P.CityEconomy.ResearchQueue[Index].Paid)) return false;
		P.CityEconomy.ResearchQueue.RemoveAt(Index);
		return Advance(P,Now,C);
	});
}

bool DYKCityResearch::IsUnlocked(const FDYKPlayerProfileSaveData& P,FDYKUnitTypeId Type)
{
	return Type==DYKUnitIds::Peasant() || Type==DYKUnitIds::Archer()
		|| P.ResearchedContent.Contains(Type.ToContentId()) || P.UnlockedContent.Contains(Type.ToContentId());
}

FDYKResearchBonus DYKCityResearch::Bonus(const FDYKPlayerProfileSaveData& P,const FDYKUnitDefinition& Unit,EDYKResearchScope Scope,const FDYKResearchConfig& C)
{
	FDYKResearchBonus Result;
	if(Unit.Side!=EDYKUnitSide::Defender) return Result;
	for(const auto Key:P.CityEconomy.CompletedResearch)
	{
		const auto* D=Find(Key,C);
		if(!D) continue;
		const bool Matches=Scope==EDYKResearchScope::Line
			? D->TargetLine!=EDYKUnitLine::None && D->TargetLine==Unit.Line
			: D->TargetType.IsValid() && D->TargetType==Unit.Id;
		if(!Matches) continue;
		Result.HP+=D->Bonus.HP; Result.PhysicalArmor+=D->Bonus.PhysicalArmor; Result.Damage+=D->Bonus.Damage;
		Result.MagicalArmor+=D->Bonus.MagicalArmor;
	}
	return Result;
}

FDYKUnitBaseStats DYKCityResearch::EffectiveStats(const FDYKPlayerProfileSaveData& P,const FDYKUnitDefinition& Unit)
{
	FDYKUnitBaseStats Stats=Unit.Base;
	const auto LineBonus=Bonus(P,Unit,EDYKResearchScope::Line);
	const auto PersonalBonus=Bonus(P,Unit,EDYKResearchScope::Type);
	Stats.MaxHP.Value+=LineBonus.HP+PersonalBonus.HP;
	Stats.PhysicalArmor.Value+=LineBonus.PhysicalArmor+PersonalBonus.PhysicalArmor;
	Stats.MagicalArmor.Value+=LineBonus.MagicalArmor+PersonalBonus.MagicalArmor;
	Stats.Attack.DamagePerHit.Value+=LineBonus.Damage+PersonalBonus.Damage;
	return Stats;
}
