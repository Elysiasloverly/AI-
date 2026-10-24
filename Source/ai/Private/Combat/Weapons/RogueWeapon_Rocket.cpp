// 火箭武器 —— 齐射逻辑
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
	// 当前策略：射程额外放宽 700，取 `max(12, 数量×3)` 个候选，
	// 然后按顺序分给每一发火箭（见 FireRocketVolley 的 `RocketIndex % Enemies.Num()`）。
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

	const FVector LaunchOrigin = OwnerChar->GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
	const int32 EffectiveCount = GetEffectiveCount();
	URogueCombatPoolSubsystem* Pools = GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;

	bool bAudioTagged = false;
	for (int32 RocketIndex = 0; RocketIndex < EffectiveCount; ++RocketIndex)
	{
		if (!Enemies.IsValidIndex(RocketIndex % Enemies.Num()) || !IsValid(Enemies[RocketIndex % Enemies.Num()]))
		{
			continue;
		}

		ARogueEnemy* TargetEnemy = Enemies[RocketIndex % Enemies.Num()];
		FVector Direction = (TargetEnemy->GetActorLocation() - LaunchOrigin).GetSafeNormal();

		// 弹道随机平面偏转
		if (Config.bEnableTrajectoryDeviation && Config.TrajectoryDeviationMaxAngle > 0.0f)
		{
			const float RandomDeviation = FMath::FRandRange(-Config.TrajectoryDeviationMaxAngle, Config.TrajectoryDeviationMaxAngle);
			Direction = Direction.RotateAngleAxis(RandomDeviation, FVector::UpVector).GetSafeNormal();
		}

		// 弹速随机增幅
		float FinalRocketSpeed = GetEffectiveProjectileSpeed(Config.RocketSpeed);
		if (Config.bEnableSpeedRandomization && Config.SpeedRandomizationRatio > 0.0f)
		{
			FinalRocketSpeed *= (1.0f + FMath::FRandRange(-Config.SpeedRandomizationRatio, Config.SpeedRandomizationRatio));
			FinalRocketSpeed = FMath::Max(FinalRocketSpeed, 0.0f);
		}

		const FVector SpawnLocation = LaunchOrigin + Direction * 90.0f + FVector(0.0f, 0.0f, RocketIndex * 8.0f);

		UClass* RocketClassToSpawn = Config.RocketClass ? Config.RocketClass.Get() : ARogueRocketProjectile::StaticClass();
		ARogueRocketProjectile* Rocket = Pools != nullptr
			? Pools->AcquireRocketProjectile(RocketClassToSpawn, OwnerChar, OwnerChar, SpawnLocation, Direction.Rotation())
			: GetWorld()->SpawnActor<ARogueRocketProjectile>(RocketClassToSpawn, SpawnLocation, Direction.Rotation());
		if (Rocket != nullptr)
		{
			Rocket->ActivatePooledRocket(OwnerChar, OwnerChar, SpawnLocation, Direction.Rotation(), Direction, FinalRocketSpeed, GetEffectiveDamage(), GetExplosionRadius());

			// 权威那一发已经生成，再把"画出来需要什么"广播给队友。
			// 逐发广播：偏转角与弹速随机是每发单独掷的；客户端调用它是空操作。
			// Param 传爆炸半径 —— 队友那台机器上爆炸特效的大小是按半径算的
			// （见 ARogueRocketProjectile::Explode），不传的话队友看到的爆炸会偏小。
			FRogueWeaponCosmeticFire Fire;
			Fire.bPlayAttackSound = !bAudioTagged;
			bAudioTagged = true;
			Fire.Start = SpawnLocation;
			Fire.Direction = Direction;
			Fire.Speed = FinalRocketSpeed;
			Fire.Param = GetExplosionRadius();
			BroadcastCosmeticFire(Fire);
		}
	}
}

void ARogueWeapon_Rocket::PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire)
{
	// 只在"既非服务端、也非本人"的机器上被调用（见 Multicast_WeaponCosmetic）。
	// 与 ARogueEnemy::Multicast_SpawnCosmeticProjectile 同形：
	// 同一个池函数、同一套参数，只把伤害换成 0；权威伤害在服务端爆炸时结算。
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
		OwnerChar, OwnerChar, Fire.Start, Fire.Direction.Rotation(), Fire.Direction, Fire.Speed, /*InDamage=*/0.0f, Fire.Param);
}
