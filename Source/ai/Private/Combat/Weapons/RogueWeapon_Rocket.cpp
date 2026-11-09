// 冰焰·陨星：目标上空召唤，斜向坠落，命中后范围爆炸。
#include "Combat/Weapons/RogueWeapon_Rocket.h"
#include "AbilitySystem/Abilities/RogueWeaponAbility.h"
#include "AbilitySystem/RogueAttributeSet.h"

#include "Engine/World.h"

#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"

ARogueWeapon_Rocket::ARogueWeapon_Rocket()
{
	FireAbilityClass = URogueWeaponAbility_Rocket::StaticClass();
}

void ARogueWeapon_Rocket::CollectFireTargets(TArray<ARogueEnemy*>& OutTargets) const
{
	// 索敌额外范围由武器表决定，先取 max(12, 数量×3) 个距离候选。
	// 齐射同时考虑本轮与在途弹体的目标分配，优先选择空闲敌人。
	CollectEnemiesInRange(GetEffectiveRange() + GetConfig().RocketSeekRangeBonus, OutTargets, FMath::Max(12, GetEffectiveCount() * 3));
}

void ARogueWeapon_Rocket::FireAtTargets(const TArray<ARogueEnemy*>& Targets)
{
	if (Targets.Num() == 0)
	{
		return;
	}

	FireRocketVolley(Targets);
}

// 生效数值：火箭的换算系数与主武器不同，公式由本武器定义
//（升级只改玩家 ASC 上的加成属性，这里负责"基础值 + 加成×本武器系数"）。

float ARogueWeapon_Rocket::GetEffectiveCooldown() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.AttackSpeed;
	const FRogueWeaponTableRow& WeaponConfig = GetConfig();
	const float MinCooldown = WeaponConfig.MinCooldown >= 0.0f ? WeaponConfig.MinCooldown : 0.0f;
	return FMath::Max(MinCooldown, WeaponConfig.Cooldown - Bonus * WeaponConfig.CooldownReductionScale);
}

float ARogueWeapon_Rocket::GetExplosionRadius() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float RangeBonus = Bonuses.AttackRange;
	const FRogueWeaponTableRow& WeaponConfig = GetConfig();
	return FMath::Max(0.0f, WeaponConfig.ExplosionRadius + RangeBonus * WeaponConfig.RocketExplosionRadiusRangeBonusScale + Bonuses.RocketBlastRadius);
}

void ARogueWeapon_Rocket::FireRocketVolley(const TArray<ARogueEnemy*>& Enemies)
{
	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr)
	{
		return;
	}

    const float ExplosionRadius = GetExplosionRadius();
    const float VisualScale = ExplosionRadius / FMath::Max(1.f, Config.ExplosionRadius);
	const int32 EffectiveCount = GetEffectiveCount();
	URogueCombatPoolSubsystem* Pools = GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;

    TArray<ARogueEnemy*> VolleyTargets;
	bool bAudioTagged = false;
	for (int32 RocketIndex = 0; RocketIndex < EffectiveCount; ++RocketIndex)
	{
        ARogueEnemy* TargetEnemy=SelectDistributedTarget(Enemies,VolleyTargets);
        if (!TargetEnemy) { break; }
        const FVector TargetLocation = TargetEnemy->GetActorLocation();
        FVector Approach = (TargetLocation - OwnerChar->GetActorLocation()).GetSafeNormal2D();
        if (Approach.IsNearlyZero()) { Approach = OwnerChar->GetActorForwardVector().GetSafeNormal2D(); }
        if (Approach.IsNearlyZero()) { Approach = FVector::ForwardVector; }

		// 随机的是召唤方位，随后重新指向目标；不会把下坠方向偏离落点。
		if (Config.bEnableTrajectoryDeviation && Config.TrajectoryDeviationMaxAngle > 0.0f)
		{
			const float RandomDeviation = FMath::FRandRange(-Config.TrajectoryDeviationMaxAngle, Config.TrajectoryDeviationMaxAngle);
			Approach = Approach.RotateAngleAxis(RandomDeviation, FVector::UpVector);
		}

		// 弹速随机增幅
		float FinalRocketSpeed = GetEffectiveProjectileSpeed(Config.RocketSpeed);
		if (Config.bEnableSpeedRandomization && Config.SpeedRandomizationRatio > 0.0f)
		{
			FinalRocketSpeed *= (1.0f + FMath::FRandRange(-Config.SpeedRandomizationRatio, Config.SpeedRandomizationRatio));
			FinalRocketSpeed = FMath::Max(FinalRocketSpeed, 0.0f);
		}

        const FVector SpawnLocation = TargetLocation
            + FVector::UpVector * FMath::Max(1.f, Config.RocketFall.SummonHeight)
            - Approach * FMath::Max(1.f, Config.RocketFall.HorizontalOffset);
        const FVector Direction = (TargetLocation - SpawnLocation).GetSafeNormal();

		UClass* RocketClassToSpawn = Config.RocketClass ? Config.RocketClass.Get() : ARogueRocketProjectile::StaticClass();
		ARogueRocketProjectile* Rocket = Pools != nullptr
			? Pools->AcquireRocketProjectile(RocketClassToSpawn, OwnerChar, OwnerChar, SpawnLocation, Direction.Rotation())
			: GetWorld()->SpawnActor<ARogueRocketProjectile>(RocketClassToSpawn, SpawnLocation, Direction.Rotation());
		if (Rocket != nullptr)
		{
            ReserveProjectileTarget(Rocket,TargetEnemy);
            VolleyTargets.Add(TargetEnemy);
			Rocket->ActivatePooledRocket(OwnerChar, OwnerChar, SpawnLocation, Direction.Rotation(), Direction, FinalRocketSpeed, GetEffectiveDamage(), ExplosionRadius, Config.ProjectileCombat);
            Rocket->SetExplosionVisualScale(VisualScale);
            Rocket->ConfigureGroundFlame(Config.IceFlame);

            // 下坠路径、爆炸半径和模型倍率均使用服务端这一发的结果，客户端不重复随机。
			FRogueWeaponCosmeticFire Fire;
			Fire.bPlayAttackSound = !bAudioTagged;
            Fire.bPlayFireAccent = !bAudioTagged;
			bAudioTagged = true;
			Fire.Start = SpawnLocation;
			Fire.Direction = Direction;
			Fire.Speed = FinalRocketSpeed;
			Fire.Param = ExplosionRadius;
            Fire.ProjectileVisualScale = VisualScale;
			BroadcastCosmeticFire(Fire);
		}
	}
}

void ARogueWeapon_Rocket::PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire)
{
    // 客户端补画服务端确认的下坠弹体；伤害为 0，权威伤害只由服务端结算。
	if (GetWorld() == nullptr)
	{
		return;
	}

	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr)
	{
		return;
	}

	URogueCombatPoolSubsystem* Pools = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>();
	if (Pools == nullptr)
	{
		return;
	}

	UClass* RocketClassToSpawn = Config.RocketClass ? Config.RocketClass.Get() : ARogueRocketProjectile::StaticClass();
	ARogueRocketProjectile* Rocket = Pools->AcquireRocketProjectile(
		RocketClassToSpawn, OwnerChar, OwnerChar, Fire.Start, Fire.Direction.Rotation());
	if (Rocket == nullptr)
	{
		return;
	}

	Rocket->ActivatePooledRocket(
		OwnerChar, OwnerChar, Fire.Start, Fire.Direction.Rotation(), Fire.Direction, Fire.Speed, /*InDamage=*/0.0f, Fire.Param, Config.ProjectileCombat);
    Rocket->SetExplosionVisualScale(Fire.ProjectileVisualScale);
}
