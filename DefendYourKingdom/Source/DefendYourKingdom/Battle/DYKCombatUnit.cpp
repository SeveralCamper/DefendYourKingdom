#include "DYKCombatUnit.h"
#include "DYKCombatRules.h"
#include "DYKPolygonGameMode.h"
#include "Engine/World.h"
#include "TimerManager.h"

ADYKCombatUnit::ADYKCombatUnit()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
}

void ADYKCombatUnit::InitializeCombat(ADYKPolygonGameMode* OwnerMode, const FDYKUnitDefinition* UnitDefinition)
{
	Mode = OwnerMode;
	Definition = UnitDefinition;
	HP = Definition->Base.MaxHP.Value;
	IdleFacing = GetActorRotation();
	OwnerMode->RegisterCombatUnit(this);
}

bool ADYKCombatUnit::CanAct() const
{
	return IsAlive() && Mode.IsValid() && Mode->GetBattleSession().GetPhase() == EDYKBattlePhase::ActiveWave;
}

bool ADYKCombatUnit::IsInRange(const ADYKCombatUnit* Candidate) const
{
	return IsValid(Candidate) && Candidate != this && Candidate->CanAct() && Candidate->Definition &&
		Candidate->Mode == Mode && Definition && Candidate->Definition->Side != Definition->Side &&
		FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation()) <= FMath::Square(
			Definition->Base.Attack.Range.Value * DYKCombatRules::CentimetersPerUnitDEV_PLACEHOLDER);
}

ADYKCombatUnit* ADYKCombatUnit::GetCurrentTarget() const
{
	for (const auto& Target : Targets) if (IsInRange(Target.Get())) return Target.Get();
	return nullptr;
}

void ADYKCombatUnit::RefreshTargets(const TArray<ADYKCombatUnit*>& Candidates)
{
	if (!CanAct()) { StopCombat(); return; }
	Targets.RemoveAll([this, &Candidates](const auto& Target)
	{
		return !IsInRange(Target.Get()) || !Candidates.Contains(Target.Get());
	});
	// Candidates arrive in the world's stable registration order for this game step.
	for (auto* Candidate : Candidates)
		if (IsInRange(Candidate)) Targets.AddUnique(Candidate);
	OnTargetsUpdated();
	RestoreIdleFacing();
	if ((GetCurrentTarget() || HasFallbackTarget()) && !GetWorldTimerManager().IsTimerActive(SwingTimer) &&
		!GetWorldTimerManager().IsTimerActive(HitTimer))
	{
		const double Remaining = NextAttackAt - GetWorld()->GetTimeSeconds();
		if (Remaining > 0)
			GetWorldTimerManager().SetTimer(SwingTimer, this, &ThisClass::RequestAttack, float(Remaining), false);
		else BeginAttack();
	}
}

void ADYKCombatUnit::RequestAttack()
{
	// Choose the next swing only after this step's movement/spawn/revival events
	// have refreshed the pool. In particular, a new ally takes priority at a goal.
	if (Mode.IsValid()) Mode->MarkCombatTargetsDirty();
}

void ADYKCombatUnit::BeginAttack()
{
	if (!CanAct()) { StopCombat(); return; }
	auto* Target = GetCurrentTarget();
	if (!Target && !HasFallbackTarget()) { RestoreIdleFacing(); return; }
	PendingTarget = Target;
	bPendingFallback = !Target;
	if (Target)
	{
		const FVector Direction = Target->GetActorLocation() - GetActorLocation();
		if (!Direction.IsNearlyZero()) SetActorRotation(Direction.Rotation());
	}
	const auto& Attack = Definition->Base.Attack;
	NextAttackAt = GetWorld()->GetTimeSeconds() + Attack.IntervalSeconds.Value;
	// The logical impact drives presentation later (M1.10); there is no projectile actor.
	GetWorldTimerManager().SetTimer(HitTimer, this, &ThisClass::ResolveHit, Attack.FirstHitDelaySeconds.Value, false);
	GetWorldTimerManager().SetTimer(SwingTimer, this, &ThisClass::RequestAttack, Attack.IntervalSeconds.Value, false);
}

void ADYKCombatUnit::ResolveHit()
{
	if (!CanAct()) { StopCombat(); return; }
	auto* Target = PendingTarget.Get();
	const bool bFallback = bPendingFallback;
	PendingTarget.Reset();
	bPendingFallback = false;
	// A committed swing survives range exit, but never redirects after its target dies.
	if (Target && Target->IsAlive()) Target->ApplyDamage(Definition->Base.Attack.DamagePerHit.Value, Definition->Base.Attack.DamageType);
	else if (bFallback) HitFallbackTarget();
	RestoreIdleFacing();
}

void ADYKCombatUnit::RestoreIdleFacing()
{
	if (Definition && Definition->Movement == EDYKUnitMovement::Stationary && !GetCurrentTarget() && !PendingTarget.IsValid())
		SetActorRotation(IdleFacing);
}

bool ADYKCombatUnit::ApplyDamage(int32 Damage, EDYKDamageType Type)
{
	if (!CanAct() || Damage <= 0 || !Definition) return false;
	const int32 Effective = DYKCombatRules::DamageAfterArmor(Damage, Definition->Base.PhysicalArmor.Value, Definition->Base.MagicalArmor.Value, Type);
	HP = FMath::Max(0, HP - Effective);
	if (HP == 0)
	{
		StopCombat();
		Mode->MarkCombatTargetsDirty();
		OnDeath();
	}
	return true;
}

void ADYKCombatUnit::StopCombat()
{
	GetWorldTimerManager().ClearTimer(SwingTimer);
	GetWorldTimerManager().ClearTimer(HitTimer);
	PendingTarget.Reset();
	bPendingFallback = false;
	Targets.Reset();
}

void ADYKCombatUnit::RestoreHealth()
{
	StopCombat();
	HP = Definition->Base.MaxHP.Value;
	NextAttackAt = 0;
	SetActorRotation(IdleFacing);
	if (Mode.IsValid()) Mode->MarkCombatTargetsDirty();
}

void ADYKCombatUnit::EndPlay(const EEndPlayReason::Type Reason)
{
	StopCombat();
	if (Mode.IsValid()) Mode->MarkCombatTargetsDirty();
	Super::EndPlay(Reason);
}
