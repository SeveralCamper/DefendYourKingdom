#pragma once
#include "DYKCityResearch.h"

enum class EDYKCityBuildTab : uint8 { Resources, Special, City, Decoration };
enum class EDYKCityInterior : uint8 { None, Production, Workshop, Barracks, Library, Tavern, Magic, Warehouse };
enum class EDYKCityWindow : uint8 { Closed, Construction, Building, Interior, Detail };

struct FDYKCityInteriorItem
{
	FName Key;
	FString Name;
	FString Description;
	FName ResearchRoot;
	FName Art;
	FBox2D ImageRegion;
	bool bFuture=false;
};

struct FDYKCityWindowState
{
	EDYKCityWindow Window=EDYKCityWindow::Closed;
	EDYKCityBuildTab BuildTab=EDYKCityBuildTab::Resources;
	FGuid Instance;
	FName Building;
	int32 Page=0;
	int32 LibraryTab=0;
	int32 SelectedItem=INDEX_NONE;
	void OpenConstruction() {Window=EDYKCityWindow::Construction; Page=0; SelectedItem=INDEX_NONE;}
	void Select(FGuid Id,FName Type) {Instance=Id; Building=Type; Window=EDYKCityWindow::Building; Page=0; SelectedItem=INDEX_NONE;}
	void Visit() {Window=EDYKCityWindow::Interior; Page=0; SelectedItem=INDEX_NONE;}
	void Close() {Window=EDYKCityWindow::Closed; SelectedItem=INDEX_NONE;}
	void TurnPage(int32 Delta,int32 Count) {Page=FMath::Clamp(Page+Delta,0,FMath::Max(0,(Count-1)/6)); SelectedItem=INDEX_NONE;}
	bool IsModal() const {return Window!=EDYKCityWindow::Closed;}
};

namespace DYKCityBuildingUI
{
	DEFENDYOURKINGDOM_API EDYKCityBuildTab Tab(FName Building);
	DEFENDYOURKINGDOM_API EDYKCityInterior Interior(FName Building);
	DEFENDYOURKINGDOM_API FString Description(FName Building);
	DEFENDYOURKINGDOM_API FName Art(FName Building,bool Detail=false);
	DEFENDYOURKINGDOM_API TArray<FDYKCityInteriorItem> Items(FName Building,int32 LibraryTab=0);
	DEFENDYOURKINGDOM_API TArray<FName> ConstructionTypes(EDYKCityBuildTab Tab);
}
