// 激光武器 —— 激光爆发 + 折射链逻辑
#include "Combat/RogueWeapon_Laser.h"

#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Kismet/GameplayStatics.h"

void ARogueWeapon_Laser::WeaponTick(float DeltaSeconds)
{
	if (GetEffectiveCount() <= 0)
	{
		return;
	}

	LaserTimer -= DeltaSeconds;
	if (LaserTimer > 0.0f)
	{
		return;
	}

	TArray<ARogueEnemy*> Enemies;
	CollectEnemiesInRange(Config.Range, Enemies, FMath::Max(8, GetEffectiveCount() * 3));
	if (Enemies.Num() == 0)
	{
		return;
	}

	FireLaserBurst(Enemies);
	LaserTimer = Config.Cooldown;
}

bool ARogueWeapon_Laser::OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude)
{
	// 激光数量升级
	if (UpgradeType == ERogueUpgradeType::LaserCount)
	{
		Config.Count = FMath::Max(1, Config.Count + FMath::RoundToInt(Magnitude));
		return true;
	}

	// 激光折射升级
	if (UpgradeType == ERogueUpgradeType::LaserRefraction)
	{
		Config.RefractionCount = FMath::Clamp(Config.RefractionCount + FMath::RoundToInt(Magnitude), 0, 8);
		return true;
	}

	return false;
}

void ARogueWeapon_Laser::ApplySharedDamageBonus(float Magnitude)
{
	Config.Damage += Magnitude;
}

void ARogueWeapon_Laser::ApplySharedSpeedBonus(float Magnitude)
{
	Config.Cooldown = FMath::Max(0.35f, Config.Cooldown - Magnitude * 2.0f);
}

void ARogueWeapon_Laser::ApplySharedRangeBonus(float Magnitude)
{
	Config.Range += Magnitude;
	Config.RefractionRange += Magnitude * 0.45f;
}

void ARogueWeapon_Laser::FireLaserBurst(const TArray<ARogueEnemy*>& Enemies)
{
	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr)
	{
		return;
	}

	const FVector BeamOriginBase = OwnerChar->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
	const int32 EffectiveCount = GetEffectiveCount();
	const FVector LateralDirection = GetOwnerCameraRightVector();
	const int32 HalfCount = EffectiveCount / 2;
	const float BeamSpacing = 34.0f;

	for (int32 LaserIndex = 0; LaserIndex < EffectiveCount; ++LaserIndex)
	{
		if (!Enemies.IsValidIndex(LaserIndex % Enemies.Num()) || !IsValid(Enemies[LaserIndex % Enemies.Num()]))
		{
			continue;
		}

		ARogueEnemy* TargetEnemy = Enemies[LaserIndex % Enemies.Num()];
		const float OffsetIndex = static_cast<float>(LaserIndex - HalfCount);
		const float LateralOffset = EffectiveCount % 2 == 0 ? (OffsetIndex + 0.5f) * BeamSpacing : OffsetIndex * BeamSpacing;
		const FVector BeamOrigin = BeamOriginBase + LateralDirection * LateralOffset;
		const FVector TargetLocation = TargetEnemy->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		UGameplayStatics::ApplyDamage(TargetEnemy, Config.Damage, nullptr, OwnerChar, UDamageType::StaticClass());
		SpawnLaserBeam(BeamOrigin, TargetLocation);

		if (Config.RefractionCount > 0)
		{
			FireLaserRefractionChain(TargetEnemy, TargetLocation, Config.Damage * 0.5f);
		}
	}
}

void ARogueWeapon_Laser::FireLaserRefractionChain(ARogueEnemy* InitialTarget, const FVector& InitialImpactLocation, float InitialDamage)
{
	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (!IsValid(InitialTarget) || Config.RefractionCount <= 0 || InitialDamage <= 0.0f || OwnerChar == nullptr)
	{
		return;
	}

	TArray<TObjectPtr<ARogueEnemy>> HitEnemies;
	HitEnemies.Add(InitialTarget);

	ARogueEnemy* CurrentTarget = InitialTarget;
	FVector CurrentStartLocation = InitialImpactLocation;
	float CurrentDamage = InitialDamage;

	for (int32 RefractionIndex = 0; RefractionIndex < Config.RefractionCount; ++RefractionIndex)
	{
		if (!IsValid(CurrentTarget) || CurrentDamage <= 1.0f)
		{
			break;
		}

		ARogueEnemy* NextTarget = FindNearestEnemyExcluding(CurrentTarget->GetActorLocation(), HitEnemies, Config.RefractionRange);
		if (!IsValid(NextTarget))
		{
			break;
		}

		const FVector NextImpactLocation = NextTarget->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		UGameplayStatics::ApplyDamage(NextTarget, CurrentDamage, nullptr, OwnerChar, UDamageType::StaticClass());
		SpawnLaserBeam(CurrentStartLocation, NextImpactLocation);

		HitEnemies.Add(NextTarget);
		CurrentTarget = NextTarget;
		CurrentStartLocation = NextImpactLocation;
	}
}
