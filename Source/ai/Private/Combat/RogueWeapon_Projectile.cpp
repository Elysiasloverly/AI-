// 弹体武器 —— 自动射击逻辑
#include "Combat/RogueWeapon_Projectile.h"

#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Combat/RogueProjectile.h"

void ARogueWeapon_Projectile::WeaponTick(float DeltaSeconds)
{
	if (GetEffectiveProjectileCount() <= 0)
	{
		return;
	}

	AttackTimer -= DeltaSeconds;
	if (AttackTimer > 0.0f)
	{
		return;
	}

	ARogueEnemy* Target = FindNearestEnemy(GetEffectiveRange());
	if (Target != nullptr)
	{
		FireAtTarget(Target);
		AttackTimer = GetEffectiveCooldown();
	}
}

// 注：本类**不再有** OnUpgradeApplied / ApplyShared*Bonus。
// 升级统一落成玩家 ASC 上 URogueAttributeSet 的加成属性，
// 由上面的 GetEffective*() 在开火时合并（"数据表基础值 + 属性加成"）。

void ARogueWeapon_Projectile::FireAtTarget(AActor* TargetActor)
{
	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (!IsValid(TargetActor) || OwnerChar == nullptr)
	{
		return;
	}

	const FVector AimOrigin = OwnerChar->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	FVector Direction = TargetActor->GetActorLocation() - AimOrigin;
	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		Direction = OwnerChar->GetActorForwardVector().GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = FVector::ForwardVector;
		}
	}

	const float SpreadStepDegrees = 9.0f;
	const int32 EffectiveCount = GetEffectiveProjectileCount();
	const int32 HalfCount = EffectiveCount / 2;

	URogueCombatPoolSubsystem* Pools = GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;

	for (int32 Index = 0; Index < EffectiveCount; ++Index)
	{
		const float OffsetIndex = static_cast<float>(Index - HalfCount);
		const float SpreadOffset = EffectiveCount % 2 == 0 ? (OffsetIndex + 0.5f) * SpreadStepDegrees : OffsetIndex * SpreadStepDegrees;
		FVector ShotDirection = Direction.RotateAngleAxis(SpreadOffset, FVector::UpVector).GetSafeNormal();

		// 弹道随机平面偏转
		if (Config.bEnableTrajectoryDeviation && Config.TrajectoryDeviationMaxAngle > 0.0f)
		{
			const float RandomDeviation = FMath::FRandRange(-Config.TrajectoryDeviationMaxAngle, Config.TrajectoryDeviationMaxAngle);
			ShotDirection = ShotDirection.RotateAngleAxis(RandomDeviation, FVector::UpVector).GetSafeNormal();
		}

		// 弹速随机增幅（基础弹速已含 GAS 加成）
		const float BaseSpeed = GetEffectiveProjectileSpeed(Config.ProjectileSpeed);
		float FinalSpeed = BaseSpeed;
		if (Config.bEnableSpeedRandomization && Config.SpeedRandomizationRatio > 0.0f)
		{
			FinalSpeed *= (1.0f + FMath::FRandRange(-Config.SpeedRandomizationRatio, Config.SpeedRandomizationRatio));
			FinalSpeed = FMath::Max(FinalSpeed, 0.0f);
		}

		const FVector SpawnLocation = AimOrigin + ShotDirection * 100.0f;

		UClass* ProjectileToSpawn = Config.ProjectileClass ? Config.ProjectileClass.Get() : ARogueProjectile::StaticClass();
		ARogueProjectile* Projectile = Pools != nullptr
			? Pools->AcquirePlayerProjectile(ProjectileToSpawn, OwnerChar, OwnerChar, SpawnLocation, ShotDirection.Rotation())
			: GetWorld()->SpawnActor<ARogueProjectile>(ProjectileToSpawn, SpawnLocation, ShotDirection.Rotation());
		if (Projectile != nullptr)
		{
			Projectile->ActivatePooledProjectile(OwnerChar, OwnerChar, SpawnLocation, ShotDirection.Rotation(), ShotDirection, FinalSpeed, GetEffectiveDamage());

			// 权威那一发已经生成，再把"画出来需要什么"广播给队友。
			// 必须**逐发**广播：偏转角度和弹速随机都是每一发单独掷的，
			// 一次广播整轮齐射的话，队友看到的弹道会和这里对不上。
			// 客户端调用它是空操作（见 BroadcastCosmeticFire），所以这里可以无条件调。
			FRogueWeaponCosmeticFire Fire;
			Fire.Start = SpawnLocation;
			Fire.Direction = ShotDirection;
			Fire.Speed = FinalSpeed;
			BroadcastCosmeticFire(Fire);
		}
	}
}

void ARogueWeapon_Projectile::PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire)
{
	// 只会在这里是"既非服务端、也非本人"的机器上被调用（见 Multicast_WeaponCosmetic）。
	// 形状与 ARogueEnemy::Multicast_SpawnCosmeticProjectile 一致：
	// 用同一个池函数、同一套参数生成一个纯表现弹体。
	// 唯一的区别是伤害传 0 —— 伤害早已在服务端那一发里结算完，
	// 这里再算一次就是双份扣血。
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

	UClass* ProjectileToSpawn = Config.ProjectileClass ? Config.ProjectileClass.Get() : ARogueProjectile::StaticClass();
	ARogueProjectile* Projectile = Pools->AcquirePlayerProjectile(
		ProjectileToSpawn, OwnerChar, OwnerChar, Fire.Start, Fire.Direction.Rotation());
	if (Projectile == nullptr)
	{
		return;
	}

	Projectile->ActivatePooledProjectile(
		OwnerChar, OwnerChar, Fire.Start, Fire.Direction.Rotation(), Fire.Direction, Fire.Speed, /*InDamage=*/0.0f);
}
