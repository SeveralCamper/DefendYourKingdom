#include "DYKCityGameMode.h"
#include "DYKCityBuildingActor.h"
#include "DYKCityEconomy.h"
#include "DYKCityWork.h"
#include "DYKCityOrders.h"
#include "DYKCityWorkWidget.h"
#include "DYKCityTerritory.h"
#include "DYKCityTerritoryActor.h"
#include "DYKCityTerritoryWidget.h"
#include "DYKCityWidgets.h"
#include "DYKCityEditorWidget.h"
#include "../Core/Interaction/DYKSelectionPlayerController.h"
#include "DYKCityDecorationActor.h"
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
	if (P && (P->CityEconomy.Version == 0 || P->CityEconomy.TerritoryVersion < 2))
	{
		const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
		Ready = Save->ApplyProfileCommand(FGuid::NewGuid(), [Now](FDYKPlayerProfileSaveData& Candidate)
		{
			return DYKCityEconomy::Initialize(Candidate, Now)
				&& DYKCityTerritory::Initialize(Candidate, DYKCityTerritory::DEV_PLACEHOLDER());
		});
	}
	else if (P) { Ready = DYKCityEconomy::Validate(*P) && DYKCityTerritory::Validate(*P,DYKCityTerritory::DEV_PLACEHOLDER()); }
	if (Ready) Ready=DYKCityWork::Update(*Save,FDateTime::UtcNow().ToUnixTimestamp());
	if (Ready)
	{
		const auto* Current=Save->GetProfileSnapshot();
		const bool LegacyProducer=Current->CityEconomy.Instances.ContainsByPredicate([Current](const auto& S)
			{return S.OrdersVersion==0 && DYKCityCatalog::Find(Current->Buildings[S.BuildingIndex].BuildingId)->Category==EDYKCityBuildingCategory::Production;});
		if(LegacyProducer) Ready=Save->ApplyProfileCommand(FGuid::NewGuid(),[](auto& State){return DYKCityOrders::Migrate(State,FDateTime::UtcNow().ToUnixTimestamp());});
	}
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
	if (auto* Decoration=GetWorld()->SpawnActor<ADYKCityDecorationActor>()) Decoration->Refresh(*P);
	Super::StartPlay();
	if (auto* PC = GetWorld()->GetFirstPlayerController(); PC && PC->IsLocalController())
	{
		if (auto* Territory = CreateWidget<UDYKCityTerritoryWidget>(PC)) Territory->AddToViewport(10);
		if (auto* Editor = CreateWidget<UDYKCityEditorWidget>(PC)) Editor->AddToViewport(20);
		if (auto* WorkUI = CreateWidget<UDYKCityWorkWidget>(PC)) WorkUI->AddToViewport(15);
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
	bPlacingBuilding=false;
	PlacementTurns = P->Buildings[State->BuildingIndex].RotationQuarterTurns;
	if (const auto* Entry = LayoutEditor.Entries().FindByPredicate([Instance](const auto& E) { return E.InstanceId == Instance; }))
	{
		PlacementTurns = Entry->QuarterTurns;
		bPlacingBuilding=!Entry->bPlaced;
	}
	LayoutMessage = FText::GetEmpty();
	if(!LayoutEditor.IsActive()) for(TObjectIterator<UDYKCityWorkWidget> It;It;++It)
		if(It->GetWorld()==GetWorld() && It->IsInViewport()) It->OpenBuilding(Instance,P->Buildings[State->BuildingIndex].BuildingId.Name);
	return true;
}

void ADYKCityGameMode::ClearBuildingSelection()
{
	SelectedBuilding.Invalidate(); PlacementTurns=0; bPlacingBuilding=false;
	if (auto* PC=Cast<ADYKSelectionPlayerController>(GetWorld()->GetFirstPlayerController())) PC->ClearWorldSelection();
	SyncLayoutVisuals();
}

bool ADYKCityGameMode::BeginLayoutEdit()
{
	CancelConstructionPlacement();
	const auto* P = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	if (!SelectedBuilding.IsValid() || !P || (!LayoutEditor.IsActive() && !LayoutEditor.Begin(*P))) return false;
	bPlacingBuilding=true;
	LayoutMessage = NSLOCTEXT("DYKCityEditor","Hint","Выберите место на сетке. Все изменения сохраняются вместе.");
	SyncLayoutVisuals();
	return true;
}

bool ADYKCityGameMode::StoreSelected()
{
	if (!LayoutEditor.Store(SelectedBuilding)) return false;
	LayoutMessage = NSLOCTEXT("DYKCityEditor","Buffered","Здание в хранилище. Выберите его изображение, чтобы разместить снова.");
	ClearBuildingSelection();
	return true;
}

bool ADYKCityGameMode::PlaceSelected(FIntPoint Cell)
{
	if (!bPlacingBuilding) return false;
	const bool Placed = LayoutEditor.Place(SelectedBuilding,Cell,PlacementTurns);
	LayoutMessage = Placed ? NSLOCTEXT("DYKCityEditor","Placed","Место выбрано. Нажмите «Закончить изменение» для сохранения.")
		: NSLOCTEXT("DYKCityEditor","Illegal","Здесь нельзя разместить здание: проверьте свободное место и открытую землю.");
	if (Placed) ClearBuildingSelection();
	return Placed;
}

bool ADYKCityGameMode::RotateSelected()
{
	if (!LayoutEditor.IsActive() || !SelectedBuilding.IsValid()) return false;
	const int32 NewTurns=(PlacementTurns+1)%4;
	const auto* Entry=LayoutEditor.Entries().FindByPredicate([this](const auto& E){return E.InstanceId==SelectedBuilding;});
	if (!Entry) return false;
	if (!Entry->bPlaced && !LayoutEditor.RotateStored(SelectedBuilding,NewTurns)) return false;
	if (Entry->bPlaced && !LayoutEditor.Place(SelectedBuilding,Entry->Origin,NewTurns))
	{
		LayoutMessage=NSLOCTEXT("DYKCityEditor","RotationBlocked","Поворот невозможен: не хватает свободного места для здания и дорожек.");
		return false;
	}
	PlacementTurns=NewTurns;
	LayoutMessage=FText::GetEmpty();
	SyncLayoutVisuals();
	return true;
}

bool ADYKCityGameMode::CommitLayoutEdit()
{
	auto* Save = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>();
	const auto* Current=Save->GetProfileSnapshot();
	if (LayoutEditor.IsActive() && Current && LayoutEditor.HasConflict(*Current))
	{
		LayoutMessage=NSLOCTEXT("DYKCityEditor","LayoutConflict","Расстановка или состав зданий изменились извне. Изменения не сохранены.");
		return false;
	}
	if (!LayoutEditor.Commit(*Save))
	{
		const bool Buffered=LayoutEditor.Entries().ContainsByPredicate([](const auto& E){return !E.bPlaced;});
		LayoutMessage = Buffered ? NSLOCTEXT("DYKCityEditor","BuildingsMissing","Не все здания выставлены. Верните здания из хранилища на карту.")
			: !LayoutEditor.CanFinish() ? NSLOCTEXT("DYKCityEditor","InvalidFinish","Нельзя закончить изменение: проверьте свободное место для всех зданий и дорожек.")
			: NSLOCTEXT("DYKCityEditor","WriteFailed","Не удалось записать расстановку. Повторите завершение изменения.");
		return false;
	}
	LayoutMessage = FText::GetEmpty();
	SelectedBuilding.Invalidate();
	bPlacingBuilding=false;
	SyncLayoutVisuals();
	return true;
}

void ADYKCityGameMode::CancelLayoutEdit()
{
	LayoutEditor.Cancel();
	SelectedBuilding.Invalidate();
	bPlacingBuilding=false;
	LayoutMessage = FText::GetEmpty();
	SyncLayoutVisuals();
}

void ADYKCityGameMode::SyncLayoutVisuals()
{
	const auto* P = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	if (!P) return;
	TSet<FGuid> Spawned;
	for (TActorIterator<ADYKCityBuildingActor> It(GetWorld()); It; ++It)
	{
		const auto* State = P->CityEconomy.Instances.FindByPredicate([&](const auto& S) { return S.InstanceId == It->InstanceId; });
		if (!State) { It->Destroy(); continue; }
		Spawned.Add(State->InstanceId);
		auto B = P->Buildings[State->BuildingIndex];
		const auto* Entry = LayoutEditor.Entries().FindByPredicate([&](const auto& E) { return E.InstanceId == It->InstanceId; });
		const bool Hidden = Entry && !Entry->bPlaced;
		It->SetActorHiddenInGame(Hidden); It->SetActorEnableCollision(!Hidden);
		// Collection is available again after exit. Production remains clock-driven in the profile.
		It->CollectionWidget->SetVisibility(It->IsHovered() && !LayoutEditor.IsActive());
		if (Entry) { B.CellOrigin=Entry->Origin; B.RotationQuarterTurns=Entry->QuarterTurns; }
		if (!Hidden) It->SetActorLocationAndRotation(DYKCityTerritory::BuildingLocation(B,DYKCityTerritory::DEV_PLACEHOLDER()),FRotator(0,B.RotationQuarterTurns*90.,0));
	}
	for (const auto& S:P->CityEconomy.Instances) if (!Spawned.Contains(S.InstanceId))
	{
		const auto& B=P->Buildings[S.BuildingIndex];
		const FTransform Transform(FRotator(0,B.RotationQuarterTurns*90.,0),DYKCityTerritory::BuildingLocation(B,DYKCityTerritory::DEV_PLACEHOLDER()));
		if (auto* Actor=GetWorld()->SpawnActorDeferred<ADYKCityBuildingActor>(ADYKCityBuildingActor::StaticClass(),Transform))
		{ Actor->InstanceId=S.InstanceId; Actor->BuildingId=B.BuildingId; Actor->FinishSpawning(Transform); }
	}
	for (TActorIterator<ADYKCityDecorationActor> It(GetWorld());It;++It) It->Refresh(*P,&LayoutEditor);
	for (TObjectIterator<UDYKCityEditorWidget> It; It; ++It) if (It->GetWorld()==GetWorld()) It->Refresh();
}

bool ADYKCityGameMode::BeginConstruction(FName Type)
{
	if(LayoutEditor.IsActive() || !DYKCityCatalog::Find(FDYKBuildingId(Type).ToContentId())) return false;
	ClearBuildingSelection(); ConstructionType=Type; return true;
}

bool ADYKCityGameMode::PlaceConstruction(FIntPoint Cell)
{
	if(ConstructionType.IsNone() || LayoutEditor.IsActive()) return false;
	FDYKCityBuildingWorkRequest R; R.CommandId=FGuid::NewGuid(); R.InstanceId=FGuid::NewGuid();
	R.BuildingId=FDYKBuildingId(ConstructionType); R.Origin=Cell;
	if(!DYKCityWork::Enqueue(*GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>(),R,FDateTime::UtcNow().ToUnixTimestamp())) return false;
	ConstructionType=NAME_None; SyncLayoutVisuals(); return true;
}
