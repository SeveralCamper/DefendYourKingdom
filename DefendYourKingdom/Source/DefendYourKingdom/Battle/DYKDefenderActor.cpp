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
#include "../UI/DYKHeroAbilityWidget.h"
#include "../Saving/DYKSaveSubsystem.h"
#include "DYKCombatRules.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

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
	if(IsHero() && (HeroType!=DYKUnitIds::Paladin().GetName() || !OwnerMode->CanDeployHero(HeroType,bMissionPlaced))) return false;
	DefenseId = Id;
	InitializeCombat(OwnerMode, DYKUnitCatalog::Find(IsHero() ? DYKUnitIds::Paladin() : DYKUnitIds::Peasant()));
	if(IsHero())
	{
		const auto* Save=GetGameInstance() ? GetGameInstance()->GetSubsystem<UDYKSaveSubsystem>() : nullptr;
		AbilityDefinition=DYKHeroes::Ability(Save ? Save->GetProfileSnapshot() : nullptr);
		// Reuse licensed CC0 geometry with a distinct silhouette and hammer.
		TInlineComponentArray<UStaticMeshComponent*> Meshes(this);
		for(auto* Visual:Meshes)
		{
			if(Visual->GetFName()==TEXT("PeasantVisual")) DYKVisuals::Unit(Visual,TEXT("character_human"),2.2f);
			else { Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); Visual->SetRelativeScale3D(FVector(.08f,.08f,.7f)); }
		}
		auto* Head=NewObject<UStaticMeshComponent>(this,TEXT("HammerHeadDEV_PLACEHOLDER")); AddInstanceComponent(Head);
		Head->SetupAttachment(GetRootComponent()); Head->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Head->SetRelativeLocation({10,-35,-22}); Head->SetRelativeScale3D({.4f,.18f,.18f});
		Head->SetCollisionEnabled(ECollisionEnabled::NoCollision); Head->RegisterComponent();
		auto* Button=NewObject<UWidgetComponent>(this,TEXT("HeroAbility")); AddInstanceComponent(Button);
		Button->SetupAttachment(GetRootComponent()); Button->SetWidgetSpace(EWidgetSpace::Screen);
		Button->SetWidgetClass(UDYKHeroAbilityWidget::StaticClass()); Button->SetDrawSize({170,42});
		Button->SetRelativeLocation({0,0,155}); Button->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Button->RegisterComponent(); Button->InitWidget();
		if(auto* UI=Cast<UDYKHeroAbilityWidget>(Button->GetUserWidgetObject())) UI->Configure(this);
	}
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
	AbilityReadyAt=0;
}

bool ADYKDefenderActor::CanActivateAbility() const
{
	return IsHero() && CanAct() && !UGameplayStatics::IsGamePaused(this) && AbilityCooldownRemaining()<=0;
}

double ADYKDefenderActor::AbilityCooldownRemaining() const
{ return GetWorld() ? FMath::Max(0.,AbilityReadyAt-GetWorld()->GetTimeSeconds()) : 0.; }

bool ADYKDefenderActor::ActivateAbility()
{
	if(!CanActivateAbility()) return false;
	AbilityReadyAt=GetWorld()->GetTimeSeconds()+AbilityDefinition.Cooldown.Value;
	AbilityWave=Mode->GetBattleSession().GetWaveToken();
	AbilityPoint=GetActorLocation()+GetActorForwardVector()*AbilityDefinition.ForwardDistance.Value*DYKCombatRules::CentimetersPerUnitDEV_PLACEHOLDER;
	if(AbilityDefinition.ImpactDelay.Value<=0) ResolveAbility();
	else GetWorldTimerManager().SetTimer(AbilityTimer,this,&ThisClass::ResolveAbility,AbilityDefinition.ImpactDelay.Value,false);
	return true;
}

void ADYKDefenderActor::ResolveAbility()
{
	if(!CanAct() || UGameplayStatics::IsGamePaused(this) || Mode->GetBattleSession().GetWaveToken()!=AbilityWave) return;
	Mode->ApplyHeroAreaDamage(AbilityPoint,AbilityDefinition.Radius.Value,AbilityDefinition.Damage.Value,AbilityDefinition.DamageType.Value);
}

void ADYKDefenderActor::StopCombat()
{
	Super::StopCombat();
	GetWorldTimerManager().ClearTimer(AbilityTimer);
	AbilityWave.Invalidate();
}
