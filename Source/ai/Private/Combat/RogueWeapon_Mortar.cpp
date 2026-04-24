#include "Combat/RogueWeapon_Mortar.h"

#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Combat/RogueMortarProjectile.h"
#include "Combat/RogueRocketProjectile.h"

void ARogueWeapon_Mortar::WeaponTick(float DeltaSeconds)
{
	if (GetEffectiveCount() <= 0)
	{
		return;
	}

	FireTimer -= DeltaSeconds;
	if (FireTimer > 0.0f)
	{
		return;
	}

	TArray<ARogueEnemy*> Enemies;
	CollectEnemiesInRange(GetRange(), Enemies, FMath::Max(8, GetEffectiveCount() * 4));
	if (Enemies.Num() == 0)
	{
		return;
	}

	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr)
	{
		return;
	}

	const FVector OwnerLocation = OwnerChar->GetActorLocation();
	Enemies.Sort([&OwnerLocation](const ARogueEnemy& Left, const ARogueEnemy& Right)
	{
		return FVector::DistSquared(Left.GetActorLocation(), OwnerLocation) > FVector::DistSquared(Right.GetActorLocation(), OwnerLocation);
	});

	FireMortarVolley(Enemies);
	FireTimer = Config.Cooldown;
}

bool ARogueWeapon_Mortar::OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude)
{
	if (UpgradeType == ERogueUpgradeType::MortarCount)
	{
		Config.Count = FMath::Max(1, Config.Count + FMath::RoundToInt(Magnitude));
		return true;
	}

	if (UpgradeType == ERogueUpgradeType::MortarBlastRadius)
	{
		ExplosionRadiusBonus += Magnitude;
		return true;
	}

	if (UpgradeType == ERogueUpgradeType::ProjectileSpeed)
	{
		Config.MortarLaunchSpeed += Magnitude * 0.5f;
		return true;
	}

	return false;
}

void ARogueWeapon_Mortar::ApplySharedDamageBonus(float Magnitude)
{
	Config.Damage += Magnitude;
}

void ARogueWeapon_Mortar::ApplySharedSpeedBonus(float Magnitude)
{
	Config.Cooldown = FMath::Max(0.7f, Config.Cooldown - Magnitude * 2.0f);
}

void ARogueWeapon_Mortar::ApplySharedRangeBonus(float Magnitude)
{
	Config.Range += Magnitude;
	Config.MortarExplosionRadius += Magnitude * 0.35f;
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
				Config.Damage,
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

			Projectile->ActivatePooledRocket(
				OwnerChar,
				OwnerChar,
				SpawnLocation,
				Direction.Rotation(),
				Direction,
				FinalProjectileSpeed,
				Config.Damage,
				GetExplosionRadius());
		}
	}
}
