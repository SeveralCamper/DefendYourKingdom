#include "DYKCityGameMode.h"
#include "DYKCityBuildingActor.h"
#include "DYKCityEconomy.h"
#include "DYKCityTerritory.h"
#include "DYKCityTerritoryActor.h"
#include "DYKCityTerritoryWidget.h"
#include "DYKCityWidgets.h"
#include "DYKCityEditorWidget.h"
#include "EngineUtils.h"
#include "Components/WidgetComponent.h"
#include "../UI/DYKStartWidget.h"
#include "../Core/DYKGameInstance.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "../DefendYourKingdom.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void ADYKCityGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (!StartupError.IsEmpty()) { return; }
	auto* Save = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	const auto* P = Save->GetProfileSnapshot();
	bool Ready = false;
	if (P && (P->CityEconomy.Version == 0 || P->CityEconomy.TerritoryVersion == 0))
	{
		const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
		Ready = Save->ApplyProfileCommand(FGuid::NewGuid(), [Now](FDYKPlayerProfileSaveData& Candidate)
		{
			return DYKCityEconomy::Initialize(Candidate, Now)
				&& DYKCityTerritory::Initialize(Candidate, DYKCityTerritory::DEV_PLACEHOLDER());
		});
	}
	else if (P) { Ready = DYKCityEconomy::Validate(*P) && DYKCityTerritory::Validate(*P,DYKCityTerritory::DEV_PLACEHOLDER()); }
	if (!Ready)
	{
		StartupError = ErrorMessage = TEXT("City state initialization failed; existing profile was preserved.");
		SpectatorClass = nullptr;
		UE_LOG(LogDYK, Error, TEXT("%s"), *StartupError);
	}
}

void ADYKCityGameMode::StartPlay()
{
	if (!StartupError.IsEmpty()) { return; }
	const auto* P = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	GetWorld()->SpawnActor<ADYKCityTerritoryActor>();
	for (const auto& State : P->CityEconomy.Instances)
	{
		const auto& Building = P->Buildings[State.BuildingIndex];
		const FVector Location = DYKCityTerritory::BuildingLocation(Building,DYKCityTerritory::DEV_PLACEHOLDER());
		const FTransform Transform(FRotator(0., Building.RotationQuarterTurns * 90., 0.), Location);
		auto* Actor = GetWorld()->SpawnActorDeferred<ADYKCityBuildingActor>(ADYKCityBuildingActor::StaticClass(), Transform);
		if (Actor)
		{
			Actor->InstanceId = State.InstanceId;
			Actor->BuildingId = Building.BuildingId;
			Actor->FinishSpawning(Transform);
		}
	}
	Super::StartPlay();
	if (auto* PC = GetWorld()->GetFirstPlayerController(); PC && PC->IsLocalController())
	{
		if (auto* Territory = CreateWidget<UDYKCityTerritoryWidget>(PC)) Territory->AddToViewport(10);
		if (auto* Editor = CreateWidget<UDYKCityEditorWidget>(PC)) Editor->AddToViewport(20);
		if (auto* HUD = CreateWidget<UDYKCityResourceWidget>(PC, DYKUI::ScreenClass(UDYKCityResourceWidget::StaticClass(), TEXT("WBP_City"))))
		{
			HUD->AddToViewport();
		}
		if (auto* GI = GetGameInstance<UDYKGameInstance>(); GI && !GI->bHasEnteredCity)
			if (auto* Title = CreateWidget<UDYKStartWidget>(PC, DYKUI::ScreenClass(UDYKStartWidget::StaticClass(), TEXT("WBP_Start")))) Title->AddToViewport(100);
	}
}

bool ADYKCityGameMode::SelectBuilding(FGuid Instance)
{
	const auto* P = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	const auto* State = P ? P->CityEconomy.Instances.FindByPredicate([Instance](const auto& S) { return S.InstanceId == Instance; }) : nullptr;
	if (!State) return false;
	SelectedBuilding = Instance;
	PlacementTurns = P->Buildings[State->BuildingIndex].RotationQuarterTurns;
	if (const auto* Entry = LayoutEditor.Entries().FindByPredicate([Instance](const auto& E) { return E.InstanceId == Instance; }))
		PlacementTurns = Entry->QuarterTurns;
	LayoutMessage = FText::GetEmpty();
	return true;
}

bool ADYKCityGameMode::BeginLayoutEdit()
{
	const auto* P = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	if (!SelectedBuilding.IsValid() || !P || !LayoutEditor.Begin(*P)) return false;
	LayoutMessage = NSLOCTEXT("DYKCityEditor","Hint","Выберите место на сетке. Все изменения сохраняются вместе.");
	SyncLayoutVisuals();
	return true;
}

bool ADYKCityGameMode::StoreSelected()
{
	if (!LayoutEditor.Store(SelectedBuilding)) return false;
	LayoutMessage = NSLOCTEXT("DYKCityEditor","Buffered","Здание в буфере. Выберите место, чтобы вернуть его на карту.");
	SyncLayoutVisuals();
	return true;
}

bool ADYKCityGameMode::PlaceSelected(FIntPoint Cell)
{
	const bool Placed = LayoutEditor.Place(SelectedBuilding,Cell,PlacementTurns);
	LayoutMessage = Placed ? NSLOCTEXT("DYKCityEditor","Placed","Место выбрано. Нажмите «Применить» для сохранения.")
		: NSLOCTEXT("DYKCityEditor","Illegal","Здесь нельзя разместить здание: проверьте свободное место и открытую землю.");
	if (Placed) SyncLayoutVisuals();
	return Placed;
}

bool ADYKCityGameMode::RotateSelected()
{
	if (!LayoutEditor.IsActive() || !SelectedBuilding.IsValid()) return false;
	PlacementTurns = (PlacementTurns+1)%4;
	LayoutMessage = NSLOCTEXT("DYKCityEditor","Rotated","Поворот выбран. Нажмите на сетку, чтобы разместить здание.");
	return true;
}

bool ADYKCityGameMode::CommitLayoutEdit()
{
	auto* Save = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	if (!LayoutEditor.Commit(*Save))
	{
		LayoutMessage = NSLOCTEXT("DYKCityEditor","CommitFailed","Не удалось применить. Разместите все здания на свободной открытой земле. При ошибке записи повторите сохранение.");
		return false;
	}
	LayoutMessage = FText::GetEmpty();
	SelectedBuilding.Invalidate();
	SyncLayoutVisuals();
	return true;
}

void ADYKCityGameMode::CancelLayoutEdit()
{
	LayoutEditor.Cancel();
	SelectedBuilding.Invalidate();
	LayoutMessage = FText::GetEmpty();
	SyncLayoutVisuals();
}

void ADYKCityGameMode::SyncLayoutVisuals()
{
	const auto* P = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	if (!P) return;
	for (TActorIterator<ADYKCityBuildingActor> It(GetWorld()); It; ++It)
	{
		const auto* State = P->CityEconomy.Instances.FindByPredicate([&](const auto& S) { return S.InstanceId == It->InstanceId; });
		if (!State) continue;
		auto B = P->Buildings[State->BuildingIndex];
		const auto* Entry = LayoutEditor.Entries().FindByPredicate([&](const auto& E) { return E.InstanceId == It->InstanceId; });
		const bool Hidden = Entry && !Entry->bPlaced;
		It->SetActorHiddenInGame(Hidden); It->SetActorEnableCollision(!Hidden);
		// Collection is available again after exit. Production remains clock-driven in the profile.
		It->CollectionWidget->SetVisibility(!LayoutEditor.IsActive());
		if (Entry) { B.CellOrigin=Entry->Origin; B.RotationQuarterTurns=Entry->QuarterTurns; }
		if (!Hidden) It->SetActorLocationAndRotation(DYKCityTerritory::BuildingLocation(B,DYKCityTerritory::DEV_PLACEHOLDER()),FRotator(0,B.RotationQuarterTurns*90.,0));
	}
	for (TObjectIterator<UDYKCityEditorWidget> It; It; ++It) if (It->GetWorld()==GetWorld()) It->Refresh();
}
