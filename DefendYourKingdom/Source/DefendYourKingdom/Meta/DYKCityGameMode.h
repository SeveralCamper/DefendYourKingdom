// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../Core/DYKLocalGameMode.h"
#include "DYKCityLayoutEditor.h"
#include "DYKCityGameMode.generated.h"

UCLASS()
class DEFENDYOURKINGDOM_API ADYKCityGameMode : public ADYKLocalGameMode
{
	GENERATED_BODY()

public:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	bool SelectBuilding(FGuid Instance);
	void ClearBuildingSelection();
	bool IsPlacingBuilding() const { return bPlacingBuilding; }
	bool BeginLayoutEdit();
	bool StoreSelected();
	bool PlaceSelected(FIntPoint Cell);
	bool RotateSelected();
	bool CommitLayoutEdit();
	void CancelLayoutEdit();
	void SyncLayoutVisuals();
	const FDYKCityLayoutEditor& GetLayoutEditor() const { return LayoutEditor; }
	FGuid GetSelectedBuilding() const { return SelectedBuilding; }
	int32 GetPlacementTurns() const { return PlacementTurns; }
	const FText& GetLayoutMessage() const { return LayoutMessage; }
private:
	FDYKCityLayoutEditor LayoutEditor;
	FGuid SelectedBuilding;
	int32 PlacementTurns = 0;
	bool bPlacingBuilding = false;
	FText LayoutMessage;
};
