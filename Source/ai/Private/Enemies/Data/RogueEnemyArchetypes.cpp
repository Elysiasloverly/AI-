#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Engine/DataTable.h"

namespace
{
	FRogueEnemyArchetype MakeRiflemanArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::RangedKite;
		Archetype.Movement.PreferredRange = bIsBoss ? 760.0f : 620.0f;
		Archetype.Movement.FarDistance = Archetype.Movement.PreferredRange + 160.0f;
		Archetype.Movement.CloseDistance = bIsBoss ? 460.0f : 360.0f;
		Archetype.Movement.FarTowardWeight = 0.95f;
		Archetype.Movement.FarTangentWeight = 0.20f;
		Archetype.Movement.CloseTowardWeight = -0.88f;
		Archetype.Movement.CloseTangentWeight = 0.50f;
		Archetype.Movement.MidTowardWeight = 0.28f;
		Archetype.Movement.MidTangentWeight = 0.88f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Single;
		Archetype.Ranged.InitialCooldownMin = bIsBoss ? 0.25f : 0.4f;
		Archetype.Ranged.InitialCooldownMax = bIsBoss ? 0.6f : 0.9f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 0.85f : 1.25f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1100.0f : 950.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 0.75f : 0.55f;
		Archetype.Ranged.bSpawnMuzzleEffect = true;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.18f, 0.18f, 0.18f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.10f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Rifleman;
		Archetype.Visual.MeshScale = FVector(0.52f, 0.52f, 0.96f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -40.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeAssaultArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::BurstCharge;
		Archetype.Movement.BurstCycleTime = 2.8f;
		Archetype.Movement.BurstDuration = 0.9f;
		Archetype.Movement.BurstActiveMultiplier = 2.4f;
		Archetype.Movement.BurstIdleMultiplier = 1.0f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Spread3;
		Archetype.Ranged.InitialCooldownMin = bIsBoss ? 0.4f : 0.65f;
		Archetype.Ranged.InitialCooldownMax = bIsBoss ? 0.8f : 1.1f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 1.2f : 1.7f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 980.0f : 850.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 0.48f : 0.35f;
		Archetype.Ranged.SpreadShotCount = 3;
		Archetype.Ranged.SpreadAngleStep = bIsBoss ? 14.0f : 12.0f;
		Archetype.Ranged.SpreadSpawnOffset = 10.0f;
		Archetype.Ranged.bSpawnMuzzleEffect = true;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.20f, 0.20f, 0.20f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.10f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Assault;
		Archetype.Visual.MeshScale = FVector(0.72f, 0.72f, 1.08f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -44.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeJuggernautArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::Direct;
		Archetype.Movement.BaseMoveMultiplier = 0.72f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Single;
		Archetype.Ranged.InitialCooldownMin = bIsBoss ? 0.9f : 1.4f;
		Archetype.Ranged.InitialCooldownMax = bIsBoss ? 1.5f : 2.2f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 1.8f : 2.6f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 820.0f : 680.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 1.8f : 1.4f;
		Archetype.Ranged.bLargeVisual = true;
		Archetype.Ranged.bSpawnMuzzleEffect = true;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.28f, 0.28f, 0.28f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.16f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Juggernaut;
		Archetype.Visual.MeshScale = FVector(0.96f, 0.96f, 0.96f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -34.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeOrbiterArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::Orbit;
		Archetype.Movement.FarDistance = 700.0f;
		Archetype.Movement.CloseDistance = 420.0f;
		Archetype.Movement.FarTowardWeight = 0.75f;
		Archetype.Movement.FarTangentWeight = 0.35f;
		Archetype.Movement.CloseTowardWeight = -0.55f;
		Archetype.Movement.CloseTangentWeight = 0.80f;
		Archetype.Movement.MidTowardWeight = 0.25f;
		Archetype.Movement.MidTangentWeight = 0.85f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Single;
		Archetype.Ranged.InitialCooldownMin = bIsBoss ? 0.4f : 0.65f;
		Archetype.Ranged.InitialCooldownMax = bIsBoss ? 0.8f : 1.2f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 0.95f : 1.4f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1150.0f : 1000.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 0.8f : 0.6f;
		Archetype.Ranged.bSpawnMuzzleEffect = true;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.18f, 0.18f, 0.18f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.10f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Orbiter;
		Archetype.Visual.MeshScale = FVector(0.68f, 0.68f, 0.68f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -22.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeMarksmanArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::RangedKite;
		Archetype.Movement.PreferredRange = bIsBoss ? 860.0f : 760.0f;
		Archetype.Movement.FarDistance = Archetype.Movement.PreferredRange + 180.0f;
		Archetype.Movement.CloseDistance = bIsBoss ? 500.0f : 420.0f;
		Archetype.Movement.FarTowardWeight = 0.92f;
		Archetype.Movement.FarTangentWeight = 0.18f;
		Archetype.Movement.CloseTowardWeight = -0.90f;
		Archetype.Movement.CloseTangentWeight = 0.45f;
		Archetype.Movement.MidTowardWeight = 0.30f;
		Archetype.Movement.MidTangentWeight = 0.90f;
		Archetype.Movement.bUseRangeBiasForMidToward = true;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Single;
		Archetype.Ranged.InitialCooldownMin = 0.35f;
		Archetype.Ranged.InitialCooldownMax = 0.85f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 0.72f : 1.18f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1220.0f : 980.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 1.0f : 0.85f;
		Archetype.Ranged.bLargeVisual = bIsBoss;
		Archetype.Ranged.bSpawnMuzzleEffect = bIsBoss;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.24f, 0.24f, 0.24f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.12f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Marksman;
		Archetype.Visual.MeshScale = FVector(0.76f, 0.76f, 0.48f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -22.0f);
		Archetype.Visual.MeshRotation = FRotator(0.0f, 90.0f, 0.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeSkirmisherArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::ZigZagChase;
		Archetype.Movement.CloseDistance = 220.0f;
		Archetype.Movement.CloseTowardWeight = -0.30f;
		Archetype.Movement.CloseTangentWeight = 0.95f;
		Archetype.Movement.ZigZagFrequency = 7.5f;
		Archetype.Movement.ZigZagTowardWeight = 0.96f;
		Archetype.Movement.ZigZagTangentWeight = 0.82f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Spread3;
		Archetype.Ranged.InitialCooldownMin = bIsBoss ? 0.35f : 0.55f;
		Archetype.Ranged.InitialCooldownMax = bIsBoss ? 0.7f : 1.0f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 0.95f : 1.3f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1000.0f : 900.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 0.42f : 0.35f;
		Archetype.Ranged.SpreadShotCount = 3;
		Archetype.Ranged.SpreadAngleStep = bIsBoss ? 12.0f : 10.0f;
		Archetype.Ranged.SpreadSpawnOffset = 10.0f;
		Archetype.Ranged.bSpawnMuzzleEffect = true;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.18f, 0.18f, 0.18f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.10f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Skirmisher;
		Archetype.Visual.MeshScale = FVector(0.60f, 0.60f, 0.18f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -10.0f);
		Archetype.Visual.MeshRotation = FRotator(90.0f, 0.0f, 0.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeCannoneerArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::RangedKite;
		Archetype.Movement.PreferredRange = bIsBoss ? 1260.0f : 1120.0f;
		Archetype.Movement.FarDistance = Archetype.Movement.PreferredRange + 240.0f;
		Archetype.Movement.CloseDistance = bIsBoss ? 720.0f : 620.0f;
		Archetype.Movement.FarTowardWeight = 0.90f;
		Archetype.Movement.FarTangentWeight = 0.16f;
		Archetype.Movement.CloseTowardWeight = -0.96f;
		Archetype.Movement.CloseTangentWeight = 0.28f;
		Archetype.Movement.MidTowardWeight = 0.08f;
		Archetype.Movement.MidTangentWeight = 0.52f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Single;
		Archetype.Ranged.InitialCooldownMin = 0.75f;
		Archetype.Ranged.InitialCooldownMax = 1.30f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 1.08f : 1.85f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1120.0f : 860.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 1.25f : 1.10f;
		Archetype.Ranged.bLargeVisual = true;
		Archetype.Ranged.bSpawnMuzzleEffect = true;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.28f, 0.28f, 0.28f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.18f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Cannoneer;
		Archetype.Visual.MeshScale = FVector(0.94f, 0.94f, 0.68f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -26.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeScattererArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::RangedKite;
		Archetype.Movement.PreferredRange = bIsBoss ? 640.0f : 540.0f;
		Archetype.Movement.FarDistance = Archetype.Movement.PreferredRange + 120.0f;
		Archetype.Movement.CloseDistance = bIsBoss ? 320.0f : 260.0f;
		Archetype.Movement.FarTowardWeight = 0.98f;
		Archetype.Movement.FarTangentWeight = 0.26f;
		Archetype.Movement.CloseTowardWeight = -0.72f;
		Archetype.Movement.CloseTangentWeight = 0.78f;
		Archetype.Movement.MidTowardWeight = 0.20f;
		Archetype.Movement.MidTangentWeight = 0.96f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Spread3;
		Archetype.Ranged.InitialCooldownMin = 0.18f;
		Archetype.Ranged.InitialCooldownMax = 0.48f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 0.42f : 0.70f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1180.0f : 980.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 0.48f : 0.42f;
		Archetype.Ranged.bLargeVisual = false;
		Archetype.Ranged.bSpawnMuzzleEffect = bIsBoss;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.18f, 0.18f, 0.18f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.10f;
		Archetype.Ranged.SpreadShotCount = 3;
		Archetype.Ranged.SpreadAngleStep = bIsBoss ? 12.0f : 10.0f;
		Archetype.Ranged.SpreadSpawnOffset = 12.0f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Scatterer;
		Archetype.Visual.MeshScale = FVector(0.70f, 0.70f, 0.86f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -18.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeShockPillarArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::Stationary;
		Archetype.Movement.BaseMoveMultiplier = 0.0f;

		// 使用远程攻击槽发射弹幕，不再有旧地面波能力。
		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::RadialRing;
		Archetype.Ranged.InitialCooldownMin = bIsBoss ? 1.1f : 1.6f;
		Archetype.Ranged.InitialCooldownMax = bIsBoss ? 2.2f : 3.2f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 3.8f : 4.8f;
		Archetype.Ranged.BulletRing = FRogueEnemyBulletRingSettings();
		Archetype.Ranged.ProjectileSpeed = Archetype.Ranged.BulletRing.ProjectileSpeed;
		Archetype.Ranged.ProjectileDamageMultiplier = Archetype.Ranged.BulletRing.DamageMultiplier;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::ShockPillar;
		Archetype.Visual.MeshScale = FVector(0.74f, 0.74f, 1.72f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -10.0f);
		Archetype.Visual.CapsuleRadius = bIsBoss ? 72.0f : 48.0f;
		Archetype.Visual.CapsuleHalfHeight = bIsBoss ? 156.0f : 112.0f;
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeSniperArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::RangedKite;
		Archetype.Movement.PreferredRange = bIsBoss ? 1350.0f : 1150.0f;
		Archetype.Movement.FarDistance = Archetype.Movement.PreferredRange + 180.0f;
		Archetype.Movement.CloseDistance = bIsBoss ? 760.0f : 680.0f;
		Archetype.Movement.FarTowardWeight = 0.98f;
		Archetype.Movement.FarTangentWeight = 0.12f;
		Archetype.Movement.CloseTowardWeight = -0.95f;
		Archetype.Movement.CloseTangentWeight = 0.35f;
		Archetype.Movement.MidTowardWeight = 0.55f;
		Archetype.Movement.MidTangentWeight = 0.65f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Single;
		Archetype.Ranged.InitialCooldownMin = bIsBoss ? 0.8f : 1.2f;
		Archetype.Ranged.InitialCooldownMax = bIsBoss ? 1.4f : 2.0f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 1.5f : 2.1f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1950.0f : 1700.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 1.65f : 1.35f;
		Archetype.Ranged.bLargeVisual = true;
		Archetype.Ranged.bSpawnMuzzleEffect = true;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.30f, 0.30f, 0.30f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.16f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Sniper;
		Archetype.Visual.MeshScale = FVector(0.54f, 0.54f, 1.0f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -42.0f);
		Archetype.Visual.CapsuleRadius = 34.0f;
		Archetype.Visual.CapsuleHalfHeight = 74.0f;
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeRepeaterArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::RangedKite;
		Archetype.Movement.PreferredRange = bIsBoss ? 820.0f : 700.0f;
		Archetype.Movement.FarDistance = Archetype.Movement.PreferredRange + 160.0f;
		Archetype.Movement.CloseDistance = bIsBoss ? 440.0f : 380.0f;
		Archetype.Movement.FarTowardWeight = 0.90f;
		Archetype.Movement.FarTangentWeight = 0.22f;
		Archetype.Movement.CloseTowardWeight = -0.82f;
		Archetype.Movement.CloseTangentWeight = 0.55f;
		Archetype.Movement.MidTowardWeight = 0.26f;
		Archetype.Movement.MidTangentWeight = 0.88f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Single;
		Archetype.Ranged.InitialCooldownMin = bIsBoss ? 0.15f : 0.25f;
		Archetype.Ranged.InitialCooldownMax = bIsBoss ? 0.35f : 0.55f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 0.30f : 0.48f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1380.0f : 1180.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 0.38f : 0.28f;
		Archetype.Ranged.bSpawnMuzzleEffect = true;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.16f, 0.16f, 0.16f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.08f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Repeater;
		Archetype.Visual.MeshScale = FVector(0.46f, 0.46f, 0.84f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -20.0f);
		Archetype.Visual.CapsuleRadius = 30.0f;
		Archetype.Visual.CapsuleHalfHeight = 68.0f;
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeShotgunnerArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::RangedKite;
		Archetype.Movement.PreferredRange = bIsBoss ? 600.0f : 480.0f;
		Archetype.Movement.FarDistance = Archetype.Movement.PreferredRange + 100.0f;
		Archetype.Movement.CloseDistance = bIsBoss ? 320.0f : 250.0f;
		Archetype.Movement.FarTowardWeight = 0.98f;
		Archetype.Movement.FarTangentWeight = 0.20f;
		Archetype.Movement.CloseTowardWeight = -0.78f;
		Archetype.Movement.CloseTangentWeight = 0.72f;
		Archetype.Movement.MidTowardWeight = 0.18f;
		Archetype.Movement.MidTangentWeight = 0.94f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::Spread3;
		Archetype.Ranged.InitialCooldownMin = bIsBoss ? 0.25f : 0.4f;
		Archetype.Ranged.InitialCooldownMax = bIsBoss ? 0.55f : 0.85f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 1.1f : 1.65f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1000.0f : 860.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 0.38f : 0.30f;
		Archetype.Ranged.SpreadShotCount = 5;
		Archetype.Ranged.SpreadAngleStep = bIsBoss ? 13.0f : 11.0f;
		Archetype.Ranged.SpreadSpawnOffset = 10.0f;
		Archetype.Ranged.bSpawnMuzzleEffect = true;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.20f, 0.20f, 0.20f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.10f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Shotgunner;
		Archetype.Visual.MeshScale = FVector(0.88f, 0.88f, 0.62f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -24.0f);
		Archetype.Visual.CapsuleRadius = 38.0f;
		Archetype.Visual.CapsuleHalfHeight = 76.0f;
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeRingcasterArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::RangedKite;
		Archetype.Movement.PreferredRange = bIsBoss ? 760.0f : 650.0f;
		Archetype.Movement.FarDistance = Archetype.Movement.PreferredRange + 140.0f;
		Archetype.Movement.CloseDistance = bIsBoss ? 440.0f : 380.0f;
		Archetype.Movement.FarTowardWeight = 0.86f;
		Archetype.Movement.FarTangentWeight = 0.28f;
		Archetype.Movement.CloseTowardWeight = -0.84f;
		Archetype.Movement.CloseTangentWeight = 0.50f;
		Archetype.Movement.MidTowardWeight = 0.22f;
		Archetype.Movement.MidTangentWeight = 0.80f;

		Archetype.Ranged.bUsesRangedAttack = true;
		Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::RadialRing;
		Archetype.Ranged.InitialCooldownMin = bIsBoss ? 1.0f : 1.5f;
		Archetype.Ranged.InitialCooldownMax = bIsBoss ? 1.8f : 2.4f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 2.7f : 3.4f;
		FRogueEnemyBulletRingSettings Ring;
		Ring.ProjectileCount = bIsBoss ? 24 : 16;
		Ring.ProjectileSpeed = bIsBoss ? 880.0f : 760.0f;
		Ring.ProjectileLifetime = 3.2f;
		Ring.ProjectileScale = 0.32f;
		Ring.DamageMultiplier = bIsBoss ? 0.34f : 0.28f;
		Ring.HeightAboveGround = 50.0f;
		Archetype.Ranged.BulletRing = Ring;
		Archetype.Ranged.ProjectileSpeed = Ring.ProjectileSpeed;
		Archetype.Ranged.ProjectileDamageMultiplier = Ring.DamageMultiplier;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Ringcaster;
		Archetype.Visual.MeshScale = FVector(0.58f, 0.58f, 1.34f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -12.0f);
		Archetype.Visual.CapsuleRadius = 40.0f;
		Archetype.Visual.CapsuleHalfHeight = 84.0f;
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}
}

FRogueEnemyArchetype RogueEnemyArchetypes::BuildEnemyArchetype(ERogueEnemyType Type, bool bIsBoss)
{
	FRogueEnemyArchetype Archetype;

	switch (Type)
	{
	case ERogueEnemyType::Assault:
		Archetype = MakeAssaultArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Juggernaut:
		Archetype = MakeJuggernautArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Orbiter:
		Archetype = MakeOrbiterArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Marksman:
		Archetype = MakeMarksmanArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Sniper:
		Archetype = MakeSniperArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Repeater:
		Archetype = MakeRepeaterArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Shotgunner:
		Archetype = MakeShotgunnerArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Ringcaster:
		Archetype = MakeRingcasterArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Skirmisher:
		Archetype = MakeSkirmisherArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Cannoneer:
		Archetype = MakeCannoneerArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Scatterer:
		Archetype = MakeScattererArchetype(bIsBoss);
		break;
	case ERogueEnemyType::ShockPillar:
		Archetype = MakeShockPillarArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Rifleman:
	default:
		Archetype = MakeRiflemanArchetype(bIsBoss);
		break;
	}

	if (bIsBoss && Type != ERogueEnemyType::ShockPillar)
	{
		Archetype.Visual.MeshScale *= 1.75f;
		Archetype.Visual.CapsuleRadius = 62.0f;
		Archetype.Visual.CapsuleHalfHeight = 116.0f;
		Archetype.Visual.bUseBossMaterialOverride = true;
	}

	return Archetype;
}

FName RogueEnemyArchetypes::FindRowNameForEnemyType(const UDataTable* DataTable, ERogueEnemyType Type)
{
	if (DataTable == nullptr)
	{
		return NAME_None;
	}

	// 只缓存最近一次表/类型对应的行名，弱引用不延长表的生命周期。
	// 命中率取决于查询顺序；表内容若原地修改，同一键不会自动重新解析行名。
	static TWeakObjectPtr<const UDataTable> CachedTable;
	static ERogueEnemyType CachedType = ERogueEnemyType::Rifleman;
	static FName CachedRowName = NAME_None;
	static bool bCacheValid = false;

	if (bCacheValid && CachedTable.Get() == DataTable && CachedType == Type)
	{
		return CachedRowName;
	}

	FName Result = NAME_None;
	const TArray<FName> RowNames = DataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FRogueEnemyArchetypeRow* Row = DataTable->FindRow<FRogueEnemyArchetypeRow>(RowName, TEXT("FindRowNameForEnemyType"));
		if (Row != nullptr && Row->EnemyType == Type)
		{
			Result = RowName;
			break;
		}
	}

	CachedTable = DataTable;
	CachedType = Type;
	CachedRowName = Result;
	bCacheValid = true;

	return Result;
}

FRogueEnemyArchetype RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(const UDataTable* DataTable, ERogueEnemyType Type, bool bIsBoss)
{
	if (DataTable != nullptr)
	{
		const FName RowName = FindRowNameForEnemyType(DataTable, Type);
		if (!RowName.IsNone())
		{
			const FRogueEnemyArchetypeRow* Row = DataTable->FindRow<FRogueEnemyArchetypeRow>(RowName, TEXT("BuildEnemyArchetypeFromDataTable"));
			if (Row != nullptr)
			{
				return Row->ToArchetype(bIsBoss);
			}
		}
	}

	// DataTable 中未找到对应行，回退到硬编码
	UE_LOG(LogTemp, Verbose, TEXT("[RogueEnemyArchetypes] DataTable 中未找到敌人类型 %d 的配置行，回退到硬编码"), static_cast<int32>(Type));
	return BuildEnemyArchetype(Type, bIsBoss);
}

namespace
{
	/** 按敌人类型取原型表里的那一行；表为空或没有对应行时返回 nullptr */
	const FRogueEnemyArchetypeRow* FindArchetypeRow(const UDataTable* DataTable, ERogueEnemyType Type, const TCHAR* Context)
	{
		if (DataTable == nullptr)
		{
			return nullptr;
		}

		const FName RowName = RogueEnemyArchetypes::FindRowNameForEnemyType(DataTable, Type);
		return RowName.IsNone() ? nullptr : DataTable->FindRow<FRogueEnemyArchetypeRow>(RowName, Context);
	}
}

int32 RogueEnemyArchetypes::GetMaxSpawnPerWave(const UDataTable* DataTable, ERogueEnemyType Type)
{
	// 表没配 / 表里没这一行 / 行里没填 → 一律返回 0（不限）。
	// 这里刻意不做"表里查不到就封杀"的处理：漏配一行不该把一种怪直接禁掉。
	const FRogueEnemyArchetypeRow* Row = FindArchetypeRow(DataTable, Type, TEXT("GetMaxSpawnPerWave"));
	return Row != nullptr ? FMath::Max(0, Row->MaxSpawnPerWave) : 0;
}

int32 RogueEnemyArchetypes::GetMaxAlive(const UDataTable* DataTable, ERogueEnemyType Type)
{
	const FRogueEnemyArchetypeRow* Row = FindArchetypeRow(DataTable, Type, TEXT("GetMaxAlive"));
	return Row != nullptr ? FMath::Max(0, Row->MaxAlive) : 0;
}
