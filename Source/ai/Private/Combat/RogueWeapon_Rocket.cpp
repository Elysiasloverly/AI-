// 火箭武器 —— 齐射逻辑
#include "Combat/RogueWeapon_Rocket.h"

#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Combat/RogueRocketProjectile.h"

void ARogueWeapon_Rocket::WeaponTick(float DeltaSeconds)
{
	if (GetEffectiveCount() <= 0)
	{
		return;
	}

	RocketTimer -= DeltaSeconds;
	if (RocketTimer > 0.0f)
	{
		return;
	}

	TArray<ARogueEnemy*> Enemies;
	CollectEnemiesInRange(Config.Range + 700.0f, Enemies, FMath::Max(12, GetEffectiveCount() * 3));
	if (Enemies.Num() == 0)
	{
		return;
	}

	FireRocketVolley(Enemies);
	RocketTimer = Config.Cooldown;
}

bool ARogueWeapon_Rocket::OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude)
{
	// 火箭数量升级
	if (UpgradeType == ERogueUpgradeType::RocketCount)
	{
		Config.Count = FMath::Max(1, Config.Count + FMath::RoundToInt(Magnitude));
		return true;
	}

	// 弹速升级（火箭获得 75% 加成）
	if (UpgradeType == ERogueUpgradeType::ProjectileSpeed)
	{
		Config.RocketSpeed += Magnitude * 0.75f;
		return true;
	}

	return false;
}

void ARogueWeapon_Rocket::ApplySharedDamageBonus(float Magnitude)
{
	Config.Damage += Magnitude;
}

void ARogueWeapon_Rocket::ApplySharedSpeedBonus(float Magnitude)
{
	Config.Cooldown = FMath::Max(0.55f, Config.Cooldown - Magnitude * 2.5f);
}

void ARogueWeapon_Rocket::ApplySharedRangeBonus(float Magnitude)
{
	Config.ExplosionRadius += Magnitude * 0.25f;
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
		float FinalRocketSpeed = Config.RocketSpeed;
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
			Rocket->ActivatePooledRocket(OwnerChar, OwnerChar, SpawnLocation, Direction.Rotation(), Direction, FinalRocketSpeed, Config.Damage, Config.ExplosionRadius);
		}
	}
}
