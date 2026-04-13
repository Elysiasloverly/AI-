#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"

enum class ERogueEnemyMovementModel : uint8
{
	Direct,
	BurstCharge,
	Orbit,
	RangedKite,
	ZigZagChase
};

enum class ERogueEnemyShotPattern : uint8
{
	None,
	Single,
	Spread3
};

enum class ERogueEnemyVisualKey : uint8
{
	Hunter,
	Charger,
	Tank,
	Orbiter,
	Shooter,
	Skirmisher,
	Artillery,
	Spitter
};

struct AI_API FRogueEnemyMovementArchetype
{
	ERogueEnemyMovementModel Model = ERogueEnemyMovementModel::Direct;
	float BaseMoveMultiplier = 1.0f;
	float BurstCycleTime = 2.8f;
	float BurstDuration = 0.9f;
	float BurstActiveMultiplier = 2.4f;
	float BurstIdleMultiplier = 1.0f;
	float PreferredRange = 0.0f;
	float FarDistance = 0.0f;
	float CloseDistance = 0.0f;
	float FarTowardWeight = 1.0f;
	float FarTangentWeight = 0.0f;
	float CloseTowardWeight = 0.0f;
	float CloseTangentWeight = 0.0f;
	float MidTowardWeight = 1.0f;
	float MidTangentWeight = 0.0f;
	bool bUseRangeBiasForMidToward = false;
	float ZigZagFrequency = 0.0f;
	float ZigZagTowardWeight = 1.0f;
	float ZigZagTangentWeight = 0.0f;
};

struct AI_API FRogueEnemyRangedArchetype
{
	bool bUsesRangedAttack = false;
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::None;
	float InitialCooldownMin = 0.0f;
	float InitialCooldownMax = 0.0f;
	float PreferredRange = 0.0f;
	float MinimumRange = 0.0f;
	float MaximumRange = 0.0f;
	float AttackRangeMinFactor = 0.0f;
	float NextShotCooldown = 0.0f;
	float ProjectileSpeed = 0.0f;
	float ProjectileDamageMultiplier = 1.0f;
	bool bLargeVisual = false;
	bool bSpawnMuzzleEffect = false;
	FVector MuzzleEffectScale = FVector(0.2f, 0.2f, 0.2f);
	float MuzzleEffectLifetime = 0.12f;
	int32 SpreadShotCount = 3;
	float SpreadAngleStep = 10.0f;
	float SpreadSpawnOffset = 12.0f;
};

struct AI_API FRogueEnemyVisualArchetype
{
	ERogueEnemyVisualKey VisualKey = ERogueEnemyVisualKey::Hunter;
	FVector MeshScale = FVector(0.52f, 0.52f, 0.96f);
	FVector MeshLocation = FVector(0.0f, 0.0f, -40.0f);
	FRotator MeshRotation = FRotator::ZeroRotator;
	float CapsuleRadius = 34.0f;
	float CapsuleHalfHeight = 72.0f;
	bool bUseBossMaterialOverride = false;
};

struct AI_API FRogueEnemyArchetype
{
	FRogueEnemyMovementArchetype Movement;
	FRogueEnemyRangedArchetype Ranged;
	FRogueEnemyVisualArchetype Visual;
};

namespace RogueEnemyArchetypes
{
	AI_API FRogueEnemyArchetype BuildEnemyArchetype(ERogueEnemyType Type, bool bIsBoss);
}
