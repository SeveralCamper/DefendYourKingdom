#pragma once

#include "DYKTypedContentId.h"

enum class EDYKDefinitionSource : uint8
{
	Author,
	DEV_PLACEHOLDER,
	// Structural zero for an unused field, not an authored balance number.
	NotApplicable
};

// Provenance follows the individual number, including when a record is copied.
template <typename T>
struct TDYKDefinitionValue
{
	T Value;
	EDYKDefinitionSource Source = EDYKDefinitionSource::Author;
};

enum class EDYKUnitSide : uint8 { Defender, Enemy };
enum class EDYKUnitLine : uint8 { None, Infantry, Ranged };
enum class EDYKDamageType : uint8 { Physical, Magical };
enum class EDYKUnitMovement : uint8 { Stationary, GroundRoute };
enum class EDYKHitTiming : uint8 { FixedDelay, DistanceScaled };

struct FDYKBasicAttackDefinition
{
	TDYKDefinitionValue<int32> DamagePerHit;
	TDYKDefinitionValue<int32> HitCount;
	EDYKDamageType DamageType;
	TDYKDefinitionValue<float> IntervalSeconds;
	// Ranges/speeds retain passport gameplay units, not UE centimetres.
	TDYKDefinitionValue<float> Range;
	EDYKHitTiming HitTiming;
	TDYKDefinitionValue<float> FirstHitDelaySeconds;
	TDYKDefinitionValue<float> FollowUpHitDelaySeconds;
	// For DistanceScaled: delay = distance * SecondsPerRangeUnit.
	TDYKDefinitionValue<float> SecondsPerRangeUnit;
};

// Base values only. Current HP, effects, cooldown progress and research modifiers
// belong to each runtime instance, never to this process-lifetime catalog.
struct FDYKUnitBaseStats
{
	TDYKDefinitionValue<int32> MaxHP;
	TDYKDefinitionValue<int32> PhysicalArmor;
	TDYKDefinitionValue<int32> MagicalArmor;
	FDYKBasicAttackDefinition Attack;
	TDYKDefinitionValue<float> MovementSpeed;
	TDYKDefinitionValue<int32> PlacementSupplies;
	TDYKDefinitionValue<float> DeathCooldownSeconds;
	TDYKDefinitionValue<int32> KillReloadResource;
};

struct FDYKUnitDefinition
{
	FDYKUnitTypeId Id;
	EDYKUnitSide Side;
	EDYKUnitLine Line;
	int32 Tier;
	EDYKUnitMovement Movement;
	FDYKUnitBaseStats Base;
};
