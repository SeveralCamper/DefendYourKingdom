#include "DYKEnemyActor.h"
#include "DYKEnemyRoute.h"
#include "DYKPolygonGameMode.h"
#include "DYKCombatRules.h"
#include "../Core/Content/DYKUnitCatalog.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FDYKUnitBaseStats& GoblinStats() { return DYKUnitCatalog::Find(DYKUnitIds::Goblin())->Base; }
}

ADYKEnemyActor::ADYKEnemyActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TemporaryGoblinVisual"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cone.Cone"));
	Mesh->SetStaticMesh(Shape.Object);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.7, 0.7, 1.2));
	SetActorEnableCollision(false);
}

bool ADYKEnemyActor::Initialize(ADYKPolygonGameMode* OwnerMode, ADYKEnemyRoute* AssignedRoute, const FGuid& Token, FName Group)
{
	if (EnemyId.IsValid() || IsActorBeingDestroyed() || !IsValid(OwnerMode) || !IsValid(AssignedRoute) || OwnerMode->GetWorld() != GetWorld() ||
		AssignedRoute->GetWorld() != GetWorld() || !AssignedRoute->IsUsable() ||
		OwnerMode->GetBattleSession().GetProtectedObjectHP(AssignedRoute->ProtectedObjectId) <= 0) return false;
	const FGuid NewId = FGuid::NewGuid();
	if (!OwnerMode->GetBattleSession().RegisterEnemy(Token, Group, NewId)) return false;
	Route = AssignedRoute; WaveToken = Token; EnemyId = NewId;
	ObjectiveId = AssignedRoute->ProtectedObjectId;
	bRegistered = true;
	InitializeCombat(OwnerMode, DYKUnitCatalog::Find(DYKUnitIds::Goblin()));
	SetActorLocation(AssignedRoute->Path->GetLocationAtDistanceAlongSpline(0, ESplineCoordinateSpace::World));
	SetActorRotation(AssignedRoute->Path->GetRotationAtDistanceAlongSpline(0, ESplineCoordinateSpace::World));
	SetActorTickEnabled(true);
	return true;
}

bool ADYKEnemyActor::CanAct() const
{
	return bRegistered && Super::CanAct() && Mode->GetBattleSession().IsEnemyActive(WaveToken, EnemyId);
}

void ADYKEnemyActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!CanAct()) { StopCombat(); return; }
	if (GetCurrentTarget()) return;
	if (!Route.IsValid() || !Route->IsUsable())
	{
		Mode->GetBattleSession().Abort(GetWorld()->GetTimeSeconds());
		StopCombat(); return;
	}
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	const auto* Path = Route->Path.Get();
	const float Length = Path->GetSplineLength();
	Distance = FMath::Min(Length, Distance + GoblinStats().MovementSpeed.Value * DYKCombatRules::CentimetersPerUnitDEV_PLACEHOLDER * DeltaSeconds);
	SetActorLocation(Path->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
	SetActorRotation(Path->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
	Mode->MarkCombatTargetsDirty();
	if (Distance >= Length)
	{
		bAtObjective = true;
		SetActorTickEnabled(false); // Only moving actors tick; stationary attacks use game-time timers.
	}
}

bool ADYKEnemyActor::HasFallbackTarget() const
{
	return bAtObjective && Mode.IsValid() && Mode->GetBattleSession().GetProtectedObjectHP(ObjectiveId) > 0;
}

void ADYKEnemyActor::HitFallbackTarget()
{
	if (!HasFallbackTarget()) return;
	Mode->GetBattleSession().DamageProtectedObject(WaveToken, ObjectiveId, GoblinStats().Attack.DamagePerHit.Value, GetWorld()->GetTimeSeconds());
	if (!CanAct()) StopCombat();
}

void ADYKEnemyActor::OnDeath()
{
	bRegistered = false; // Commit death before notifications or Actor teardown.
	Mode->GetBattleSession().RemoveEnemy(WaveToken, EnemyId, true);
	Destroy();
}

void ADYKEnemyActor::OnTargetsUpdated()
{
	SetActorTickEnabled(CanAct() && !bAtObjective && !GetCurrentTarget());
}

void ADYKEnemyActor::StopCombat()
{
	SetActorTickEnabled(false);
	Super::StopCombat();
}

void ADYKEnemyActor::EndPlay(const EEndPlayReason::Type Reason)
{
	StopCombat();
	if (bRegistered && Mode.IsValid())
	{
		bRegistered = false;
		Mode->GetBattleSession().RemoveEnemy(WaveToken, EnemyId, false);
	}
	Super::EndPlay(Reason);
}
