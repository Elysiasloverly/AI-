#include "Combat/RogueWeapon_Mortar.h"
#include "AbilitySystem/Abilities/RogueWeaponAbility.h"
#include "AbilitySystem/RogueAttributeSet.h"

#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Combat/RogueMortarProjectile.h"
#include "Combat/RogueRocketProjectile.h"

ARogueWeapon_Mortar::ARogueWeapon_Mortar()
{
	FireAbilityClass = URogueWeaponAbility_Mortar::StaticClass();
}

void ARogueWeapon_Mortar::CollectFireTargets(TArray<ARogueEnemy*>& OutTargets) const
{
	// 与改造前 WeaponTick 完全一致：射程内取 `max(8, 数量×4)` 个候选，
	// 再按"离玩家从远到近"排序（迫击炮优先打最远的那批）。
	CollectEnemiesInRange(GetEffectiveRange(), OutTargets, FMath::Max(8, GetEffectiveCount() * 4));
	if (OutTargets.Num() == 0)
	{
		return;
	}

	const ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr)
	{
		return;
	}

	const FVector OwnerLocation = OwnerChar->GetActorLocation();
	OutTargets.Sort([&OwnerLocation](const ARogueEnemy& Left, const ARogueEnemy& Right)
	{
		return FVector::DistSquared(Left.GetActorLocation(), OwnerLocation) > FVector::DistSquared(Right.GetActorLocation(), OwnerLocation);
	});
}

void ARogueWeapon_Mortar::FireAtTargets(const TArray<ARogueEnemy*>& Targets)
{
	if (Targets.Num() == 0)
	{
		return;
	}

	FireMortarVolley(Targets);
}

// 生效数值：迫击炮的换算系数（攻速 ×2.0/下限 0.7、弹速 ×0.5、爆炸半径 ×0.35）
// 全部沿用自研版本；升级只改玩家 ASC 上的加成属性。

float ARogueWeapon_Mortar::GetEffectiveCooldown() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.AttackSpeed;
	return FMath::Max(0.7f, GetConfig().Cooldown - Bonus * 2.0f);
}

float ARogueWeapon_Mortar::GetExplosionRadius() const
{
	const FRogueWeaponTableRow& WeaponConfig = GetConfig();
	const float BaseRadius = WeaponConfig.MortarExplosionRadius > 0.0f ? WeaponConfig.MortarExplosionRadius : WeaponConfig.ExplosionRadius;

	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float RadiusBonus = Bonuses.MortarBlastRadius;
	const float RangeBonus = Bonuses.AttackRange;
	return BaseRadius + RadiusBonus + RangeBonus * 0.35f;
}

float ARogueWeapon_Mortar::GetProjectileSpeed() const
{
	const FRogueWeaponTableRow& WeaponConfig = GetConfig();
	const float BaseSpeed = WeaponConfig.MortarLaunchSpeed > 0.0f ? WeaponConfig.MortarLaunchSpeed : WeaponConfig.RocketSpeed;
	return GetEffectiveProjectileSpeed(BaseSpeed);
}

void ARogueWeapon_Mortar::FireMortarVolley(const TArray<ARogueEnemy*>& Enemies)
{
	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr || Enemies.Num() == 0)
	{
		return;
	}

	const FVector LaunchOrigin = OwnerChar->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f);
	URogueCombatPoolSubsystem* Pools = GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;
	UClass* RocketClassToSpawn = Config.MortarProjectileClass
		? Config.MortarProjectileClass.Get()
		: (Config.RocketClass ? Config.RocketClass.Get() : ARogueMortarProjectile::StaticClass());
	const int32 EffectiveCount = GetEffectiveCount();
	const float LateralSpacing = 18.0f;
	const int32 HalfCount = EffectiveCount / 2;
	const FVector LateralDirection = GetOwnerCameraRightVector();

	for (int32 MortarIndex = 0; MortarIndex < EffectiveCount; ++MortarIndex)
	{
		ARogueEnemy* TargetEnemy = Enemies[MortarIndex % Enemies.Num()];
		if (!IsValid(TargetEnemy))
		{
			continue;
		}

		const FVector TargetLocation = TargetEnemy->GetActorLocation() + FVector(0.0f, 0.0f, 6.0f);
		FVector Direction = (TargetLocation - LaunchOrigin).GetSafeNormal();
		if (!Direction.Normalize())
		{
			Direction = OwnerChar->GetActorForwardVector().GetSafeNormal();
		}

		if (Config.bEnableTrajectoryDeviation && Config.TrajectoryDeviationMaxAngle > 0.0f)
		{
			const float RandomDeviation = FMath::FRandRange(-Config.TrajectoryDeviationMaxAngle, Config.TrajectoryDeviationMaxAngle);
			Direction = Direction.RotateAngleAxis(RandomDeviation, FVector::UpVector).GetSafeNormal();
		}

		const float OffsetIndex = static_cast<float>(MortarIndex - HalfCount);
		const float LateralOffset = EffectiveCount % 2 == 0 ? (OffsetIndex + 0.5f) * LateralSpacing : OffsetIndex * LateralSpacing;
		const FVector SpawnLocation = LaunchOrigin + Direction * 90.0f + LateralDirection * LateralOffset;

		// 广播出去让队友照着画的那一发用的弹速。
		// 迫击炮分支用配置速度（抛物线由 ActivatePooledMortar 按起终点自己解算），
		// 退化成火箭的分支才额外掷随机弹速，所以那里会把它覆盖成随机后的值。
		float CosmeticSpeed = GetProjectileSpeed();

		ARogueRocketProjectile* Projectile = Pools != nullptr
			? Pools->AcquireRocketProjectile(RocketClassToSpawn, OwnerChar, OwnerChar, SpawnLocation, Direction.Rotation())
			: GetWorld()->SpawnActor<ARogueRocketProjectile>(RocketClassToSpawn, SpawnLocation, Direction.Rotation());

		if (ARogueMortarProjectile* MortarProjectile = Cast<ARogueMortarProjectile>(Projectile))
		{
			MortarProjectile->ActivatePooledMortar(
				OwnerChar,
				OwnerChar,
				SpawnLocation,
				TargetLocation,
				GetEffectiveDamage(),
				GetExplosionRadius(),
				GetProjectileSpeed());
		}
		else if (Projectile != nullptr)
		{
			float FinalProjectileSpeed = GetProjectileSpeed();
			if (Config.bEnableSpeedRandomization && Config.SpeedRandomizationRatio > 0.0f)
			{
				FinalProjectileSpeed *= (1.0f + FMath::FRandRange(-Config.SpeedRandomizationRatio, Config.SpeedRandomizationRatio));
				FinalProjectileSpeed = FMath::Max(FinalProjectileSpeed, 0.0f);
			}
			CosmeticSpeed = FinalProjectileSpeed;

			Projectile->ActivatePooledRocket(
				OwnerChar,
				OwnerChar,
				SpawnLocation,
				Direction.Rotation(),
				Direction,
				FinalProjectileSpeed,
				GetEffectiveDamage(),
				GetExplosionRadius());
		}

		if (Projectile != nullptr)
		{
			// 权威那一发已经生成，再把"画出来需要什么"广播给队友。
			// 迫击炮比别的弹体多两个必需量：
			//   · End —— 落点。抛物线是"从起点打到落点"解算出来的，只给方向复现不了；
			//     客户端也靠它把地面落点指示圈画在同一个位置。
			//   · Param —— 爆炸半径，队友那边落点指示圈大小与爆炸特效大小都按它算。
			FRogueWeaponCosmeticFire Fire;
			Fire.Start = SpawnLocation;
			Fire.End = TargetLocation;
			Fire.Direction = Direction;
			Fire.Speed = CosmeticSpeed;
			Fire.Param = GetExplosionRadius();
			BroadcastCosmeticFire(Fire);
		}
	}
}

void ARogueWeapon_Mortar::PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire)
{
	// 只在"既非服务端、也非本人"的机器上被调用（见 Multicast_WeaponCosmetic）。
	// 分支形状必须与 FireMortarVolley 保持一致：本机配置里 MortarProjectileClass
	// 是迫击炮类就走迫击炮抛物线，否则退化成火箭 —— 否则队友看到的弹道会对不上。
	// 两边的伤害都传 0：伤害只在服务端那一发里结算。
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

	UClass* RocketClassToSpawn = Config.MortarProjectileClass
		? Config.MortarProjectileClass.Get()
		: (Config.RocketClass ? Config.RocketClass.Get() : ARogueMortarProjectile::StaticClass());

	ARogueRocketProjectile* Projectile = Pools->AcquireRocketProjectile(
		RocketClassToSpawn, OwnerChar, OwnerChar, Fire.Start, Fire.Direction.Rotation());
	if (Projectile == nullptr)
	{
		return;
	}

	if (ARogueMortarProjectile* MortarProjectile = Cast<ARogueMortarProjectile>(Projectile))
	{
		MortarProjectile->ActivatePooledMortar(
			OwnerChar,
			OwnerChar,
			Fire.Start,
			Fire.End,
			/*InDamage=*/0.0f,
			Fire.Param,
			Fire.Speed);
	}
	else
	{
		Projectile->ActivatePooledRocket(
			OwnerChar,
			OwnerChar,
			Fire.Start,
			Fire.Direction.Rotation(),
			Fire.Direction,
			Fire.Speed,
			/*InDamage=*/0.0f,
			Fire.Param);
	}
}
