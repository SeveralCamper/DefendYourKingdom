#include "DYKCityCatalog.h"

namespace
{
	const FDYKResourceId ResourceIds[] = {
		FDYKResourceId(TEXT("RES-FOOD")), FDYKResourceId(TEXT("RES-WOOD")),
		FDYKResourceId(TEXT("RES-STONE")), FDYKResourceId(TEXT("RES-GOLD"))};
	const FDYKCityBuildingDefinition Definitions[] = {
		{FDYKBuildingId(TEXT("CITY-CASTLE")), TEXT("Замок"), {}, 0, 0, 0, {0, 0}},
		// Level 1 basic storage = 12 hours, house = one cycle (approved rules).
		{FDYKBuildingId(TEXT("CITY-SAWMILL")), TEXT("Лесопилка"), ResourceIds[1], 2, 60, 720, {-2, 0}},
		{FDYKBuildingId(TEXT("CITY-QUARRY")), TEXT("Каменоломня"), ResourceIds[2], 1, 60, 720, {2, 0}},
		{FDYKBuildingId(TEXT("CITY-MILL")), TEXT("Мельница"), ResourceIds[0], 3, 60, 720, {0, -2}},
		// One gold per cycle at level 1 is approved; 300 seconds is DEV_PLACEHOLDER.
		{FDYKBuildingId(TEXT("CITY-HOUSE")), TEXT("Жилой дом"), ResourceIds[3], 1, 300, 1, {0, 2}}};
}

TConstArrayView<FDYKCityBuildingDefinition> DYKCityCatalog::Buildings() { return Definitions; }
TConstArrayView<FDYKResourceId> DYKCityCatalog::Resources() { return ResourceIds; }
const FDYKCityBuildingDefinition* DYKCityCatalog::Find(const FDYKContentId& Id)
{
	for (const auto& Def : Definitions) { if (Def.Id.ToContentId() == Id) { return &Def; } }
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
