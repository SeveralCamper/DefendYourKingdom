#include "DYKCityBuildingUI.h"
#include "../Saving/DYKProfileService.h"

EDYKCityBuildTab DYKCityBuildingUI::Tab(FName Type)
{
	const auto* D=DYKCityCatalog::Find(FDYKBuildingId(Type).ToContentId());
	if(!D) return EDYKCityBuildTab::Special;
	if(D->Category==EDYKCityBuildingCategory::Production || Type==TEXT("CITY-WAREHOUSE")) return EDYKCityBuildTab::Resources;
	if(D->Category==EDYKCityBuildingCategory::House) return EDYKCityBuildTab::City;
	if(D->Category==EDYKCityBuildingCategory::Decoration) return EDYKCityBuildTab::Decoration;
	return EDYKCityBuildTab::Special;
}

EDYKCityInterior DYKCityBuildingUI::Interior(FName Type)
{
	if(Type==TEXT("CITY-WORKSHOP")) return EDYKCityInterior::Workshop;
	if(Type==TEXT("CITY-BARRACKS")) return EDYKCityInterior::Barracks;
	if(Type==TEXT("CITY-LIBRARY")) return EDYKCityInterior::Library;
	if(Type==TEXT("CITY-TAVERN")) return EDYKCityInterior::Tavern;
	if(Type==TEXT("CITY-WIZARD-TOWER")) return EDYKCityInterior::Magic;
	if(Type==TEXT("CITY-WAREHOUSE")) return EDYKCityInterior::Warehouse;
	const auto* D=DYKCityCatalog::Find(FDYKBuildingId(Type).ToContentId());
	return D && D->Resource.IsValid() ? EDYKCityInterior::Production : EDYKCityInterior::None;
}

FString DYKCityBuildingUI::Description(FName Type)
{
	if(Type==TEXT("CITY-SAWMILL")) return TEXT("Производит древесину по вашим заказам. Готовая древесина хранится здесь до сбора.");
	if(Type==TEXT("CITY-QUARRY")) return TEXT("Выполняет заказы на добычу камня. Собирайте готовые ресурсы, когда в складе есть место.");
	if(Type==TEXT("CITY-MILL")) return TEXT("Производит хлеб по очереди заказов. Заказы продолжаются, пока вы отсутствуете.");
	if(Type==TEXT("CITY-HOUSE")) return TEXT("Жители накапливают золото. Соберите монеты после завершения цикла.");
	if(Type==TEXT("CITY-WORKSHOP")) return TEXT("Изучение новых ловушек и улучшение уже открытых типов обороны.");
	if(Type==TEXT("CITY-BARRACKS")) return TEXT("Открытие обычных бойцов, их рангов и персональных улучшений.");
	if(Type==TEXT("CITY-LIBRARY")) return TEXT("Общие исследования развития города, военного дела и обороны.");
	if(Type==TEXT("CITY-TAVERN")) return TEXT("Найм новых героев и постоянное развитие принадлежащих вам героев.");
	if(Type==TEXT("CITY-WIZARD-TOWER")) return TEXT("Изучение и улучшение магии, подготовка набора заклинаний для боя.");
	if(Type==TEXT("CITY-WAREHOUSE")) return TEXT("Хранит базовые ресурсы. Вместимость для каждого ресурса учитывается отдельно.");
	if(Type==TEXT("CITY-CASTLE")) return TEXT("Центр вашего города. Уровень замка открывает новые ступени развития и лимиты построек.");
	return TEXT("Украшает город. Можно свободно поворачивать и перемещать.");
}

FName DYKCityBuildingUI::Art(FName Type,bool Detail)
{
	switch(Interior(Type))
	{
	case EDYKCityInterior::Workshop: return Detail ? TEXT("AllTrapHUD") : TEXT("WorkshopHUD");
	case EDYKCityInterior::Barracks: return Detail ? TEXT("AllUnitHUD") : TEXT("BarraksHUD");
	case EDYKCityInterior::Library: return TEXT("LibraryHUD");
	default: break;
	}
	if(Type==TEXT("CITY-SAWMILL")) return TEXT("SawmillHUD");
	if(Type==TEXT("CITY-QUARRY")) return TEXT("QuarryHUD");
	if(Type==TEXT("CITY-MILL")) return TEXT("MillHUD");
	return TEXT("WorkshopHUD");
}

TArray<FName> DYKCityBuildingUI::ConstructionTypes(EDYKCityBuildTab Section)
{
	TArray<FName> Result;
	for(const auto& D:DYKCityCatalog::Buildings())
		if(D.Category!=EDYKCityBuildingCategory::Castle && Tab(D.Id.GetName())==Section) Result.Add(D.Id.GetName());
	return Result;
}
TArray<FDYKCityInteriorItem> DYKCityBuildingUI::Items(FName Type,int32 LibraryTab)
{
	TArray<FDYKCityInteriorItem> Result;
	const auto Kind=Interior(Type);
	const auto Region=[](int32 Slot)
	{
		const int32 X=Slot%3,Y=Slot/3;
		return FBox2D(FVector2D(247+335*X,272+282*Y),FVector2D(540+335*X,451+282*Y));
	};
	if(Kind==EDYKCityInterior::Workshop)
	{
		const TCHAR* Keys[]={TEXT("SPIKES"),TEXT("DEATH-CLAWS"),TEXT("LOGS"),TEXT("PAIN-AVALANCHE")};
		const TCHAR* Names[]={TEXT("Шипы"),TEXT("Когти смерти"),TEXT("Бревна"),TEXT("Лавина боли")};
		for(int32 I=0;I<18;++I)
		{
			FDYKCityInteriorItem Item;
			Item.Key=FName(*FString::Printf(TEXT("TRAP-CARD-%d"),I));
			Item.Name=I<4 ? Names[I] : TEXT("Неизвестная ловушка");
			Item.Description=I<4 ? TEXT("Автоматическая неуязвимая ловушка с индивидуальным запасом срабатываний.") : TEXT("Эта ловушка пока недоступна.");
			if(I<4) Item.ResearchRoot=FName(*FString::Printf(TEXT("RESEARCH-TRAP-%s-L1"),Keys[I]));
			Item.Art=TEXT("WorkshopHUD"); Item.ImageRegion=Region(I<4 ? I : 4); Item.bFuture=I>=4;
			Result.Add(MoveTemp(Item));
		}
	}
	else if(Kind==EDYKCityInterior::Barracks)
	{
		const TCHAR* Names[]={TEXT("Крестьянин"),TEXT("Страж"),TEXT("Гвардеец"),TEXT("Лучник"),TEXT("Опытный лучник"),TEXT("Арбалетчик")};
		for(int32 I=0;I<18;++I)
		{
			FDYKCityInteriorItem Item;
			Item.Key=FName(*FString::Printf(TEXT("UNIT-CARD-%d"),I));
			if(I>=6) {Item.bFuture=true; Item.Name=TEXT("Неизвестный боец"); Item.Description=TEXT("Этот боец пока недоступен."); Result.Add(Item); continue;}
			Item.Name=Names[I];
			Item.Description=I<3 ? TEXT("Пехотинец. Во время волны остаётся на выбранном месте обороны.") : TEXT("Стрелок. Атакует противников в пределах своей дальности.");
			Item.ResearchRoot=FName(*FString::Printf(TEXT("RESEARCH-%s-T%d"),I<3 ? TEXT("INFANTRY") : TEXT("RANGED"),I%3+1));
			Item.Art=TEXT("BarraksHUD"); Item.ImageRegion=Region(I==0 ? 0 : I==3 ? 2 : 1);
			Result.Add(MoveTemp(Item));
		}
	}
	else if(Kind==EDYKCityInterior::Library)
	{
		for(const auto& D:DYKCityResearch::DEV_PLACEHOLDER().Definitions)
			if(D.Building==Type && LibraryTab==1)
			{
				FDYKCityInteriorItem Item; Item.Key=D.Id; Item.Name=D.DisplayName; Item.ResearchRoot=D.Id;
				Item.Description=TEXT("Постоянное общее исследование. Бонус применяется только к указанной линии.");
				Item.Art=TEXT("LibraryHUD"); Item.ImageRegion=FBox2D(FVector2D(610,510),FVector2D(835,625)); Result.Add(Item);
			}
		if(Result.IsEmpty()) for(int32 I=0;I<6;++I)
		{
			FDYKCityInteriorItem Item; Item.Key=FName(*FString::Printf(TEXT("LIBRARY-FUTURE-%d-%d"),LibraryTab,I));
			Item.Name=TEXT("Будущее исследование"); Item.Description=TEXT("Это исследование пока недоступно.");
			Item.Art=TEXT("WorkshopHUD"); Item.ImageRegion=Region(4); Item.bFuture=true; Result.Add(Item);
		}
	}
	else if(Kind==EDYKCityInterior::Tavern || Kind==EDYKCityInterior::Magic)
	{
		const TCHAR* Spells[]={TEXT("Огненный шар"),TEXT("Молния"),TEXT("Зелье лечения"),TEXT("Замедление"),TEXT("Сила")};
		for(int32 I=0;I<(Kind==EDYKCityInterior::Tavern ? 1 : 5);++I)
		{
			FDYKCityInteriorItem Item; Item.Key=FName(*FString::Printf(TEXT("%s-%d"),*Type.ToString(),I));
			Item.Name=Kind==EDYKCityInterior::Tavern ? TEXT("Паладин") : Spells[I];
			Item.Description=TEXT("Пока недоступно. Условия открытия появятся в этой карточке.");
			Item.Art=TEXT("WorkshopHUD"); Item.ImageRegion=Region(4); Item.bFuture=true; Result.Add(Item);
		}
	}
	return Result;
}

bool DYKCityBuildingUI::IsLocked(const FDYKPlayerProfileSaveData& P,const FDYKCityInteriorItem& Item)
{
	if(Item.bFuture) return true;
	const auto* Root=DYKCityResearch::Find(Item.ResearchRoot);
	if(!Root) return true;
	for(const auto Prerequisite:Root->Prerequisites) if(!P.CityEconomy.CompletedResearch.Contains(Prerequisite)) return true;
	return false;
}

bool DYKCityBuildingUI::ShowNew(const FDYKPlayerProfileSaveData& P,const FDYKCityInteriorItem& Item)
{return !IsLocked(P,Item) && !P.CityEconomy.VisitedCards.Contains(Item.Key);}

bool DYKCityBuildingUI::VisitCard(IDYKProfileService& Service,FGuid Command,FName Building,int32 LibraryTab,int32 Index)
{
	const auto Cards=Items(Building,LibraryTab);
	if(!Cards.IsValidIndex(Index)) return false;
	const auto Item=Cards[Index];
	return Service.ApplyProfileCommand(Command,[=](auto& P)
	{
		if(IsLocked(P,Item) || !P.Buildings.ContainsByPredicate([Building](const auto& B){return B.BuildingId.Name==Building && !B.bUnderConstruction;})) return false;
		P.CityEconomy.VisitedCards.AddUnique(Item.Key); return true;
	});
}
