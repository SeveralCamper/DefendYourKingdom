#include "DYKCityGameMode.h"
#include "DYKCityBuildingActor.h"
#include "DYKCityEconomy.h"
#include "DYKCityWidgets.h"
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
	if (P && P->CityEconomy.Version == 0)
	{
		const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
		Ready = Save->ApplyProfileCommand(FGuid::NewGuid(), [Now](FDYKPlayerProfileSaveData& Candidate)
		{
			return DYKCityEconomy::Initialize(Candidate, Now);
		});
	}
	else if (P) { Ready = DYKCityEconomy::Validate(*P); }
	if (!Ready)
	{
		StartupError = ErrorMessage = TEXT("City economy initialization failed; existing profile was preserved.");
		SpectatorClass = nullptr;
		UE_LOG(LogDYK, Error, TEXT("%s"), *StartupError);
	}
}

void ADYKCityGameMode::StartPlay()
{
	if (!StartupError.IsEmpty()) { return; }
	const auto* P = GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>()->GetProfileSnapshot();
	for (const auto& State : P->CityEconomy.Instances)
	{
		const auto& Building = P->Buildings[State.BuildingIndex];
		// DEV_PLACEHOLDER cell spacing/shape, using saved placement rather than type singleton Actors.
		const FVector Location(Building.CellOrigin.X * 900., Building.CellOrigin.Y * 900., 0.);
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
		if (auto* HUD = CreateWidget<UDYKCityResourceWidget>(PC, UDYKCityResourceWidget::StaticClass()))
		{
			HUD->AddToViewport();
		}
	}
}
