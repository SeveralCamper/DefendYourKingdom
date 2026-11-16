#include "DYKCityCatalog.h"

namespace
{
	const FDYKResourceId ResourceIds[] = {
		FDYKResourceId(TEXT("RES-FOOD")), FDYKResourceId(TEXT("RES-WOOD")),
		FDYKResourceId(TEXT("RES-STONE")), FDYKResourceId(TEXT("RES-GOLD"))};
	const FDYKCityBuildingDefinition CityBuildingDefinitions[] = {
		{FDYKBuildingId(TEXT("CITY-CASTLE")), TEXT("Замок"), {}, 0, 0, 0, {0, 0}, EDYKCityBuildingCategory::Castle, true, {10,10}},
		// Level 1 basic storage = 12 hours, house = one cycle (approved rules).
		{FDYKBuildingId(TEXT("CITY-SAWMILL")), TEXT("Лесопилка"), ResourceIds[1], 2, 60, 720, {-2, 0}, EDYKCityBuildingCategory::Production, true, {3,2}},
		{FDYKBuildingId(TEXT("CITY-QUARRY")), TEXT("Каменоломня"), ResourceIds[2], 1, 60, 720, {2, 0}, EDYKCityBuildingCategory::Production, true, {3,3}},
		{FDYKBuildingId(TEXT("CITY-MILL")), TEXT("Мельница"), ResourceIds[0], 3, 60, 720, {0, -2}, EDYKCityBuildingCategory::Production, true, {3,2}},
		// One gold per cycle at level 1 is approved; 300 seconds is DEV_PLACEHOLDER.
		{FDYKBuildingId(TEXT("CITY-HOUSE")), TEXT("Жилой дом"), ResourceIds[3], 1, 300, 1, {0, 2}, EDYKCityBuildingCategory::House, true, {2,2}},
		{FDYKBuildingId(TEXT("CITY-WAREHOUSE")), TEXT("Склад"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {3,3}},
		{FDYKBuildingId(TEXT("CITY-BARRACKS")), TEXT("Казармы"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {3,3}},
		{FDYKBuildingId(TEXT("CITY-WORKSHOP")), TEXT("Мастерская"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {3,3}},
		{FDYKBuildingId(TEXT("CITY-WIZARD-TOWER")), TEXT("Башня волшебника"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {2,2}},
		{FDYKBuildingId(TEXT("CITY-TAVERN")), TEXT("Таверна"), {}, 0, 0, 0, {}, EDYKCityBuildingCategory::Unique, false, {3,2}},
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
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ResourceIds); ++Index)
	{
		if (Id == ResourceIds[Index]) { return Index == 3 ? MAX_uint32 : 100; }
	}
	return 0;
}
const TCHAR* DYKCityCatalog::ResourceName(FDYKResourceId Id)
{
	const TCHAR* Names[] = {TEXT("Пища"), TEXT("Древесина"), TEXT("Камень"), TEXT("Золото")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ResourceIds); ++Index)
	{
		if (Id == ResourceIds[Index]) { return Names[Index]; }
	}
	return TEXT("");
}
