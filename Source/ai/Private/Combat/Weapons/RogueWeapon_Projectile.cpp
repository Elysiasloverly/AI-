// 弹体武器 —— 自动射击逻辑
#include "Combat/Weapons/RogueWeapon_Projectile.h"

#include "AbilitySystem/Abilities/RogueWeaponAbility.h"
#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueProjectile.h"

ARogueWeapon_Projectile::ARogueWeapon_Projectile()
{
	// 开火全部由这个技能驱动（见 ARogueCharacter::EnsureWeaponAbilities）。
	FireAbilityClass = URogueWeaponAbility_Projectile::StaticClass();
}

void ARogueWeapon_Projectile::CollectFireTargets(TArray<ARogueEnemy*>& OutTargets) const
{
	// 使用基类的单个最近目标策略。
	Super::CollectFireTargets(OutTargets);
}

void ARogueWeapon_Projectile::FireAtTargets(const TArray<ARogueEnemy*>& Targets)
{
	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (!CanFireNow() || !IsValid(OwnerChar) || !OwnerChar->IsAlive() || Targets.IsEmpty() || !IsValid(Targets[0])) { return; }
	BurstTarget = Targets[0];
	BurstTargetSpawnId = Targets[0]->GetSpawnCycleId();
	RemainingBurstShots = GetEffectiveBurstCount();
	FireNextBurstShot();
}

// 开火节奏由能力冷却 GE 控制；每次开火从配置与加成快照计算数值。

void ARogueWeapon_Projectile::FireAtTarget(AActor* TargetActor)
{
	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (!IsValid(TargetActor) || OwnerChar == nullptr)
	{
		return;
	}

	const FVector AimOrigin = OwnerChar->GetActorLocation() + FVector(0.0f, 0.0f, 25.0f);
    TArray<ARogueEnemy*> Candidates;
    CollectEnemiesInRange(GetEffectiveRange(),Candidates);
    if (auto* Preferred=Cast<ARogueEnemy>(TargetActor))
    { Candidates.Remove(Preferred); Candidates.Insert(Preferred,0); }
    TArray<ARogueEnemy*> VolleyTargets;
    TMap<ARogueEnemy*,int32> TargetShotCounts;

	const float SpreadStepDegrees = Config.SpreadStepDegrees;
	const int32 EffectiveCount = GetEffectiveProjectileCount();

	URogueCombatPoolSubsystem* Pools = GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;

	bool bAudioTagged = false;
	for (int32 Index = 0; Index < EffectiveCount; ++Index)
	{
        ARogueEnemy* ShotTarget=SelectDistributedTarget(Candidates,VolleyTargets);
        if (!ShotTarget) { break; }
        const FVector ToTarget = ShotTarget->GetActorLocation() - AimOrigin;
        const bool bFlyingTarget = RogueEnemyArchetypes::IsFlyingType(ShotTarget->GetEnemyType());
        // 飞行怪的碰撞胶囊在空中，按实际高度瞄准；地面寒弹仍平射。
        FVector Direction = bFlyingTarget ? ToTarget.GetSafeNormal() : ToTarget.GetSafeNormal2D();
        if (Direction.IsNearlyZero()) { Direction=OwnerChar->GetActorForwardVector().GetSafeNormal2D(); }
        if (Direction.IsNearlyZero()) { Direction=FVector::ForwardVector; }
        const float SpreadOffset=GetSpreadOffsetDegrees(TargetShotCounts.FindOrAdd(ShotTarget)++,SpreadStepDegrees);
        FVector ShotDirection=Direction.RotateAngleAxis(SpreadOffset,FVector::UpVector).GetSafeNormal();

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

		// 飞行怪贴近玩家或就在头顶时，出弹点不能越过目标。
		constexpr double MuzzleOffset = 100.0;
		const double LaunchOffset = bFlyingTarget ? FMath::Min(MuzzleOffset, ToTarget.Size() * 0.5) : MuzzleOffset;
		const FVector SpawnLocation = AimOrigin + ShotDirection * LaunchOffset;

		UClass* ProjectileToSpawn = Config.ProjectileClass ? Config.ProjectileClass.Get() : ARogueProjectile::StaticClass();
		ARogueProjectile* Projectile = Pools != nullptr
			? Pools->AcquirePlayerProjectile(ProjectileToSpawn, OwnerChar, OwnerChar, SpawnLocation, ShotDirection.Rotation())
			: GetWorld()->SpawnActor<ARogueProjectile>(ProjectileToSpawn, SpawnLocation, ShotDirection.Rotation());
		if (Projectile != nullptr)
		{
            ReserveProjectileTarget(Projectile,ShotTarget);
            VolleyTargets.Add(ShotTarget);
			Projectile->ActivatePooledProjectile(OwnerChar, OwnerChar, SpawnLocation, ShotDirection.Rotation(), ShotDirection, FinalSpeed, GetEffectiveDamage(), Config.ProjectileCombat);

			// 权威那一发已经生成，再把"画出来需要什么"广播给队友。
			// 必须**逐发**广播：偏转角度和弹速随机都是每一发单独掷的，
			// 一次广播整轮齐射的话，队友看到的弹道会和这里对不上。
			// 客户端调用它是空操作（见 BroadcastCosmeticFire），所以这里可以无条件调。
			FRogueWeaponCosmeticFire Fire;
			Fire.bPlayAttackSound = !bAudioTagged;
            Fire.bPlayFireAccent = !bAudioTagged;
			bAudioTagged = true;
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
	// 这里伤害传 0；权威弹体在服务端后续命中时才结算，
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
		OwnerChar, OwnerChar, Fire.Start, Fire.Direction.Rotation(), Fire.Direction, Fire.Speed, /*InDamage=*/0.0f, Config.ProjectileCombat);
}

float ARogueWeapon_Projectile::GetSpreadOffsetDegrees(int32 ShotIndex, float StepDegrees)
{
    if (ShotIndex <= 0) { return 0.0f; }
    const int32 Ring = ShotIndex / 2 + ShotIndex % 2;
    return static_cast<float>(Ring) * StepDegrees * (ShotIndex % 2 ? -1.0f : 1.0f);
}
