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

	ARogueEnemy* Target = FindNearestEnemy(Config.Range);
	if (Target != nullptr)
	{
		FireAtTarget(Target);
		AttackTimer = Config.Cooldown;
	}
}

bool ARogueWeapon_Projectile::OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude)
{
	// 弹体数量升级
	if (UpgradeType == ERogueUpgradeType::ProjectileCount)
	{
		Config.Count = FMath::Max(1, Config.Count + FMath::RoundToInt(Magnitude));
		return true;
	}

	// 弹速升级
	if (UpgradeType == ERogueUpgradeType::ProjectileSpeed)
	{
		Config.ProjectileSpeed += Magnitude;
		return true;
	}

	return false;
}

void ARogueWeapon_Projectile::ApplySharedDamageBonus(float Magnitude)
{
	Config.Damage += Magnitude;
}

void ARogueWeapon_Projectile::ApplySharedSpeedBonus(float Magnitude)
{
	Config.Cooldown = FMath::Max(0.15f, Config.Cooldown - Magnitude);
}

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

		// 弹速随机增幅
		float FinalSpeed = Config.ProjectileSpeed;
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
			Projectile->ActivatePooledProjectile(OwnerChar, OwnerChar, SpawnLocation, ShotDirection.Rotation(), ShotDirection, FinalSpeed, Config.Damage);
		}
	}
}
