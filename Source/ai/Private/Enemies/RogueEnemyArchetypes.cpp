#include "Enemies/RogueEnemyArchetypes.h"
#include "Enemies/RogueEnemyDataTable.h"
#include "Engine/DataTable.h"

namespace
{
	FRogueEnemyArchetype MakeHunterArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Hunter;
		Archetype.Visual.MeshScale = FVector(0.52f, 0.52f, 0.96f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -40.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeChargerArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::BurstCharge;
		Archetype.Movement.BurstCycleTime = 2.8f;
		Archetype.Movement.BurstDuration = 0.9f;
		Archetype.Movement.BurstActiveMultiplier = 2.4f;
		Archetype.Movement.BurstIdleMultiplier = 1.0f;
		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Charger;
		Archetype.Visual.MeshScale = FVector(0.72f, 0.72f, 1.08f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -44.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeTankArchetype(bool bIsBoss)
	{
		FRogueEnemyArchetype Archetype;
		Archetype.Movement.Model = ERogueEnemyMovementModel::Direct;
		Archetype.Movement.BaseMoveMultiplier = 0.72f;
		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Tank;
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
		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Orbiter;
		Archetype.Visual.MeshScale = FVector(0.68f, 0.68f, 0.68f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -22.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeShooterArchetype(bool bIsBoss)
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
		Archetype.Ranged.PreferredRange = Archetype.Movement.PreferredRange;
		Archetype.Ranged.MinimumRange = Archetype.Movement.CloseDistance;
		Archetype.Ranged.MaximumRange = bIsBoss ? 1550.0f : 1300.0f;
		Archetype.Ranged.AttackRangeMinFactor = 0.55f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 0.72f : 1.18f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1220.0f : 980.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 1.0f : 0.85f;
		Archetype.Ranged.bLargeVisual = bIsBoss;
		Archetype.Ranged.bSpawnMuzzleEffect = bIsBoss;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.24f, 0.24f, 0.24f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.12f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Shooter;
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
		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Skirmisher;
		Archetype.Visual.MeshScale = FVector(0.60f, 0.60f, 0.18f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -10.0f);
		Archetype.Visual.MeshRotation = FRotator(90.0f, 0.0f, 0.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeArtilleryArchetype(bool bIsBoss)
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
		Archetype.Ranged.PreferredRange = Archetype.Movement.PreferredRange;
		Archetype.Ranged.MinimumRange = Archetype.Movement.CloseDistance;
		Archetype.Ranged.MaximumRange = bIsBoss ? 1950.0f : 1720.0f;
		Archetype.Ranged.AttackRangeMinFactor = 0.82f;
		Archetype.Ranged.NextShotCooldown = bIsBoss ? 1.08f : 1.85f;
		Archetype.Ranged.ProjectileSpeed = bIsBoss ? 1120.0f : 860.0f;
		Archetype.Ranged.ProjectileDamageMultiplier = bIsBoss ? 1.25f : 1.10f;
		Archetype.Ranged.bLargeVisual = true;
		Archetype.Ranged.bSpawnMuzzleEffect = true;
		Archetype.Ranged.MuzzleEffectScale = FVector(0.28f, 0.28f, 0.28f);
		Archetype.Ranged.MuzzleEffectLifetime = 0.18f;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Artillery;
		Archetype.Visual.MeshScale = FVector(0.94f, 0.94f, 0.68f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -26.0f);
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}

	FRogueEnemyArchetype MakeSpitterArchetype(bool bIsBoss)
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
		Archetype.Ranged.PreferredRange = Archetype.Movement.PreferredRange;
		Archetype.Ranged.MinimumRange = Archetype.Movement.CloseDistance;
		Archetype.Ranged.MaximumRange = bIsBoss ? 1120.0f : 940.0f;
		Archetype.Ranged.AttackRangeMinFactor = 0.40f;
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

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::Spitter;
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

		Archetype.GroundWave.bUsesGroundWave = true;
		Archetype.GroundWave.InitialCooldownMin = bIsBoss ? 1.1f : 1.6f;
		Archetype.GroundWave.InitialCooldownMax = bIsBoss ? 2.2f : 3.2f;
		Archetype.GroundWave.AttackCooldown = bIsBoss ? 3.8f : 4.8f;
		Archetype.GroundWave.MaxRadius = bIsBoss ? 1280.0f : 980.0f;
		Archetype.GroundWave.ExpansionDuration = bIsBoss ? 0.95f : 1.05f;
		Archetype.GroundWave.ExpansionSpeed = bIsBoss ? 3200.0f : 2600.0f;
		Archetype.GroundWave.HitThickness = bIsBoss ? 116.0f : 92.0f;
		Archetype.GroundWave.DamageMultiplier = bIsBoss ? 1.45f : 1.2f;
		Archetype.GroundWave.bJumpCanDodge = true;

		Archetype.Visual.VisualKey = ERogueEnemyVisualKey::ShockPillar;
		Archetype.Visual.MeshScale = FVector(0.74f, 0.74f, 1.72f);
		Archetype.Visual.MeshLocation = FVector(0.0f, 0.0f, -10.0f);
		Archetype.Visual.CapsuleRadius = bIsBoss ? 72.0f : 48.0f;
		Archetype.Visual.CapsuleHalfHeight = bIsBoss ? 156.0f : 112.0f;
		Archetype.Visual.bUseBossMaterialOverride = bIsBoss;
		return Archetype;
	}
}

FRogueEnemyArchetype RogueEnemyArchetypes::BuildEnemyArchetype(ERogueEnemyType Type, bool bIsBoss)
{
	FRogueEnemyArchetype Archetype;

	switch (Type)
	{
	case ERogueEnemyType::Charger:
		Archetype = MakeChargerArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Tank:
		Archetype = MakeTankArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Orbiter:
		Archetype = MakeOrbiterArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Shooter:
		Archetype = MakeShooterArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Skirmisher:
		Archetype = MakeSkirmisherArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Artillery:
		Archetype = MakeArtilleryArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Spitter:
		Archetype = MakeSpitterArchetype(bIsBoss);
		break;
	case ERogueEnemyType::ShockPillar:
		Archetype = MakeShockPillarArchetype(bIsBoss);
		break;
	case ERogueEnemyType::Hunter:
	default:
		Archetype = MakeHunterArchetype(bIsBoss);
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

	const TArray<FName> RowNames = DataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FRogueEnemyArchetypeRow* Row = DataTable->FindRow<FRogueEnemyArchetypeRow>(RowName, TEXT("FindRowNameForEnemyType"));
		if (Row != nullptr && Row->EnemyType == Type)
		{
			return RowName;
		}
	}

	return NAME_None;
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
