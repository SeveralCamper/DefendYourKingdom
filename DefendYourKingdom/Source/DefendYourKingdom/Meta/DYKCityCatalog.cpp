#include "DYKCityCatalog.h"

namespace
{
	const FDYKResourceId ResourceIds[] = {
		FDYKResourceId(TEXT("RES-FOOD")), FDYKResourceId(TEXT("RES-WOOD")),
		FDYKResourceId(TEXT("RES-STONE")), FDYKResourceId(TEXT("RES-GOLD")), FDYKResourceId(TEXT("RES-IRON"))};
	const FDYKCityBuildingDefinition CityBuildingDefinitions[] = {
		{FDYKBuildingId(TEXT("CITY-CASTLE")), TEXT("Замок"), {}, 0, 0, 0, {0, 0}, EDYKCityBuildingCategory::Castle, true, {10,10}},
		// Level 1 basic storage = 12 hours, house = one cycle (approved rules).
		{FDYKBuildingId(TEXT("CITY-SAWMILL")), TEXT("Лесопилка"), ResourceIds[1], 2, 60, 720, {-2, 0}, EDYKCityBuildingCategory::Production, true, {3,2}, TEXT("Древесина")},
		{FDYKBuildingId(TEXT("CITY-QUARRY")), TEXT("Каменоломня"), ResourceIds[2], 1, 60, 720, {2, 0}, EDYKCityBuildingCategory::Production, true, {3,3}, TEXT("Камень")},
		// Stable save ID retained: former mill becomes the bakery, still producing food.
		{FDYKBuildingId(TEXT("CITY-MILL")), TEXT("Пекарня"), ResourceIds[0], 3, 60, 720, {0, -2}, EDYKCityBuildingCategory::Production, true, {3,2}, TEXT("Хлеб")},
		// Author-requested iron producer; numbers/footprint remain DEV_PLACEHOLDER.
        {FDYKBuildingId(TEXT("CITY-FORGE")), TEXT("Кузница"), ResourceIds[4], 1, 60, 720, {}, EDYKCityBuildingCategory::Production, false, {3,2}, TEXT("Железо")},
        // One gold per cycle at level 1 is approved; 300 seconds is DEV_PLACEHOLDER.
		{FDYKBuildingId(TEXT("CITY-HOUSE")), TEXT("Жилой дом"), ResourceIds[3], 1, 300, 1, {0, 2}, EDYKCityBuildingCategory::House, true, {2,2}},
		{FDYKBuildingId(TEXT("CITY-WAREHOUSE")), TEXT("Склад"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {3,3}},
		{FDYKBuildingId(TEXT("CITY-BARRACKS")), TEXT("Казармы"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {3,3}},
		{FDYKBuildingId(TEXT("CITY-WORKSHOP")), TEXT("Мастерская"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {3,3}},
		{FDYKBuildingId(TEXT("CITY-WIZARD-TOWER")), TEXT("Башня волшебника"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {2,2}},
		{FDYKBuildingId(TEXT("CITY-TAVERN")), TEXT("Таверна"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {3,2}},
		{FDYKBuildingId(TEXT("CITY-LIBRARY")), TEXT("Библиотека"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {3,3}},
		// Temporary decoration for placement/sale rules; no unspecified gameplay bonus.
		{FDYKBuildingId(TEXT("CITY-DECOR-DEV_PLACEHOLDER")), TEXT("Украшение"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Decoration, false, {1,1}}};
}

TConstArrayView<FDYKCityBuildingDefinition> DYKCityCatalog::Buildings() { return CityBuildingDefinitions; }
TConstArrayView<FDYKResourceId> DYKCityCatalog::Resources() { return ResourceIds; }
const FDYKCityBuildingDefinition* DYKCityCatalog::Find(const FDYKContentId& Id)
{
	for (const auto& Def : CityBuildingDefinitions) { if (Def.Id.ToContentId() == Id) { return &Def; } }
	return nullptr;
}
int64 DYKCityCatalog::ResourceCapacity(FDYKResourceId Id)
{
	if (Id.GetName()==TEXT("RES-MEDALS")) return MAX_uint32;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ResourceIds); ++Index)
	{
		if (Id == ResourceIds[Index]) { return Index == 3 ? MAX_uint32 : 100; }
	}
	return 0;
}
const TCHAR* DYKCityCatalog::ResourceName(FDYKResourceId Id)
{
	if (Id.GetName()==TEXT("RES-MEDALS")) return TEXT("Медали");
	const TCHAR* Names[] = {TEXT("Еда"), TEXT("Древесина"), TEXT("Камень"), TEXT("Золото"), TEXT("Железо")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ResourceIds); ++Index)
	{
		if (Id == ResourceIds[Index]) { return Names[Index]; }
	}
	return TEXT("");
}

FDYKCityProductionLevel DYKCityCatalog::Production(FDYKBuildingId Id, int32 Level)
{
	const auto* Def = Find(Id.ToContentId());
	if (!Def || Level < 1 || Level > 3 || !Def->Resource.IsValid()) return {};
	// Replaceable per-type/per-level rates and house cycle duration. Only the
	// 12/16/24-hour basic storage and one-cycle house storage are author rules.
 const int32 StorageHours[] = {12,16,24};
 FDYKCityProductionLevel Result;
 Result.UnitsPerCycle=Def->UnitsPerCycle*Level;
 Result.CycleSeconds=Def->CycleSeconds;
 Result.Capacity=Def->Category==EDYKCityBuildingCategory::House ? Result.UnitsPerCycle
  : int64(StorageHours[Level-1])*3600/Result.CycleSeconds*Result.UnitsPerCycle;
	return Result;
}

int64 DYKCityCatalog::WarehouseCapacityDEV_PLACEHOLDER(FDYKResourceId Id, int32 Level)
{
	if (ResourceCapacity(Id) == MAX_uint32) return MAX_uint32;
	if (ResourceCapacity(Id) == 0 || Level < 0 || Level > 4) return 0;
	// Level 0 is the starter storage available without a warehouse building.
	const int64 Food[] = {100,150,300,600,2400};
	const int64 Wood[] = {100,200,400,800,1600};
	const int64 Stone[] = {100,150,300,600,1600};
	return Id.GetName() == TEXT("RES-FOOD") ? Food[Level]
		: Id.GetName() == TEXT("RES-WOOD") ? Wood[Level] : Stone[Level];
}
