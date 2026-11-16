#include "DYKDefenderActor.h"
#include "DYKPolygonGameMode.h"
#include "../DefendYourKingdom.h"
#include "../Core/Content/DYKUnitCatalog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "../UI/DYKVisuals.h"

ADYKDefenderActor::ADYKDefenderActor()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("LogicalPosition")));
	auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PeasantVisual"));
	Mesh->SetupAttachment(GetRootComponent());
	DYKVisuals::Unit(Mesh,TEXT("character_human"),1.8f);
	auto* Tool = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TemporaryPoleTool"));
	Tool->SetupAttachment(GetRootComponent());
	Tool->SetStaticMesh(DYKVisuals::Mesh(TEXT("Dungeon"),TEXT("weapon_spear")));
	Tool->SetRelativeLocation(FVector(10,-35,-60)); Tool->SetRelativeScale3D(FVector(1,1,2.2));
	Tool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADYKDefenderActor::BeginPlay()
{
	Super::BeginPlay();
	if (!bMissionPlaced) return;
	auto* OwnerMode = GetWorld()->GetAuthGameMode<ADYKPolygonGameMode>();
	const FGuid Id = FGuid::NewGuid();
	if (!OwnerMode || !OwnerMode->GetBattleSession().AddDefense(Id) || !Initialize(OwnerMode, Id))
	{
		UE_LOG(LogDYK, Error, TEXT("Mission-placed defender requires a valid polygon preparation."));
		if (OwnerMode) OwnerMode->GetBattleSession().Abort(GetWorld()->GetTimeSeconds());
	}
}

bool ADYKDefenderActor::Initialize(ADYKPolygonGameMode* OwnerMode, const FGuid& Id)
{
	if (DefenseId.IsValid() || !Id.IsValid() || !IsValid(OwnerMode) || OwnerMode->GetWorld() != GetWorld()) return false;
	DefenseId = Id;
	InitializeCombat(OwnerMode, DYKUnitCatalog::Find(DYKUnitIds::Peasant()));
	return true;
}

void ADYKDefenderActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bMissionPlaced && Mode.IsValid()) Mode->GetBattleSession().RemoveDefense(DefenseId);
	Super::EndPlay(Reason);
}

void ADYKDefenderActor::OnDeath()
{
	ResurrectAt = GetWorld()->GetTimeSeconds() + GetUnitDefinition()->Base.DeathCooldownSeconds.Value;
	SetActorHiddenInGame(true);
}

bool ADYKDefenderActor::Resurrect()
{
	if (!CanResurrect()) return false;
	RestoreHealth();
	SetActorHiddenInGame(false);
	return true;
}

bool ADYKDefenderActor::CanResurrect() const
{
	return Mode.IsValid() && !IsAlive() && !IsActorBeingDestroyed() && !UGameplayStatics::IsGamePaused(this) &&
		Mode->GetBattleSession().GetPhase() == EDYKBattlePhase::ActiveWave && GetWorld()->GetTimeSeconds() >= ResurrectAt;
}

void ADYKDefenderActor::RestoreAfterWave()
{
	RestoreHealth();
	SetActorHiddenInGame(false);
	ResurrectAt = 0;
}
