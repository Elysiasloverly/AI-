// 武器系统基类 —— 通用辅助函数实现
#include "Combat/RogueWeaponBase.h"

#include "Engine/World.h"

#include "Player/RogueCharacter.h"
#include "Core/RogueCombatPools.h"
#include "Enemies/RogueEnemy.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "Combat/RogueLaserBeam.h"
#include "Combat/RogueMortarProjectile.h"
#include "Combat/RogueOrbitingBlade.h"
#include "Combat/RogueProjectile.h"
#include "Combat/RogueRocketProjectile.h"
#include "Camera/CameraComponent.h"

ARogueWeaponBase::ARogueWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false; // 由 Character 手动调用 WeaponTick
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void FRogueWeaponTableRow::CollectPoolPrewarmClasses(FRogueCombatPoolPrewarmClasses& OutClasses) const
{
	OutClasses.PlayerProjectileClasses.Add(ProjectileClass);
	OutClasses.RocketProjectileClasses.Add(RocketClass);
	OutClasses.RocketProjectileClasses.Add(MortarProjectileClass.Get());
	OutClasses.LaserBeamClasses.Add(BeamClass);
	OutClasses.OrbitingBladeClasses.Add(BladeClass);
}

void ARogueWeaponBase::InitializeWeapon(ARogueCharacter* InOwnerCharacter, const FRogueWeaponTableRow& InConfig)
{
	OwnerCharacter = InOwnerCharacter;
	Config = InConfig;
}

void ARogueWeaponBase::OnOwnerDied()
{
	// 基类默认不做任何事，子类可重写
}

bool ARogueWeaponBase::OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude)
{
	// 默认处理：如果升级类型匹配本武器的数量升级类型，则 Count +1
	if (UpgradeType == Config.CountUpgradeType)
	{
		Config.Count = FMath::Max(1, Config.Count + FMath::RoundToInt(Magnitude));
		return true;
	}
	return false;
}

void ARogueWeaponBase::ApplySharedDamageBonus(float Magnitude)
{
	Config.Damage += Magnitude;
}

void ARogueWeaponBase::ApplySharedSpeedBonus(float Magnitude)
{
	Config.Cooldown = FMath::Max(0.15f, Config.Cooldown - Magnitude);
}

void ARogueWeaponBase::ApplySharedRangeBonus(float Magnitude)
{
	Config.Range += Magnitude;
}

ARogueEnemy* ARogueWeaponBase::FindNearestEnemy(float MaxRange) const
{
	URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	if (Tracker == nullptr || OwnerCharacter == nullptr)
	{
		return nullptr;
	}

	TArray<TObjectPtr<ARogueEnemy>> Empty;
	return Tracker->FindNearestEnemyInRange(OwnerCharacter->GetActorLocation(), MaxRange, Empty);
}

ARogueEnemy* ARogueWeaponBase::FindNearestEnemyExcluding(const FVector& Origin, const TArray<TObjectPtr<ARogueEnemy>>& Excluded, float MaxRange) const
{
	URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	if (Tracker == nullptr)
	{
		return nullptr;
	}

	return Tracker->FindNearestEnemyInRange(Origin, MaxRange, Excluded);
}

void ARogueWeaponBase::CollectEnemiesInRange(float InRange, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults) const
{
	URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	if (Tracker == nullptr || OwnerCharacter == nullptr)
	{
		OutEnemies.Reset();
		return;
	}

	Tracker->CollectEnemiesInRange(OwnerCharacter->GetActorLocation(), InRange, OutEnemies, MaxResults, true);
}

FVector ARogueWeaponBase::GetOwnerLocation() const
{
	return OwnerCharacter != nullptr ? OwnerCharacter->GetActorLocation() : FVector::ZeroVector;
}

FVector ARogueWeaponBase::GetOwnerCameraRightVector() const
{
	if (OwnerCharacter == nullptr)
	{
		return FVector::RightVector;
	}

	// 通过 Character 的 Camera 获取右方向
	UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
	if (Camera != nullptr)
	{
		FVector Right = Camera->GetRightVector();
		Right.Z = 0.0f;
		if (Right.Normalize())
		{
			return Right;
		}
	}

	FVector Right = OwnerCharacter->GetActorRightVector();
	Right.Z = 0.0f;
	if (!Right.Normalize())
	{
		Right = FVector::RightVector;
	}
	return Right;
}

bool ARogueWeaponBase::IsHeavyCombat() const
{
	URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	return Tracker != nullptr && (Tracker->ShouldCullCombatEffects() || Tracker->GetActiveEnemyCount() >= 40);
}

void ARogueWeaponBase::SpawnLaserBeam(const FVector& Start, const FVector& End, bool bInfernoStyle, bool bImpactEffect, float Lifetime)
{
	URogueCombatPoolSubsystem* Pools = GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;
	if (Pools == nullptr || OwnerCharacter == nullptr)
	{
		return;
	}

	UClass* BeamClassToSpawn = Config.BeamClass ? Config.BeamClass.Get() : ARogueLaserBeam::StaticClass();
	ARogueLaserBeam* Beam = Pools->AcquireLaserBeam(BeamClassToSpawn, OwnerCharacter, Start, FRotator::ZeroRotator);
	if (Beam != nullptr)
	{
		Beam->ActivatePooledBeam(OwnerCharacter, Start, End, bInfernoStyle, bImpactEffect, Lifetime);
	}

	// 光束是"事件型"表现：服务端这一发必须广播出去。
	// 否则队友看不到激光/地狱塔的光束 —— 客户端已经不再自己跑别人的武器了。
	// （在客户端调用 SpawnLaserBeam 时这里会直接返回：BroadcastCosmeticFire 只在服务端有效。）
	FRogueWeaponCosmeticFire Fire;
	Fire.Start = Start;
	Fire.End = End;
	Fire.Param = Lifetime;
	Fire.bAlternateStyle = bInfernoStyle;
	BroadcastCosmeticFire(Fire);
}

void ARogueWeaponBase::BroadcastCosmeticFire(const FRogueWeaponCosmeticFire& Fire) const
{
	if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority())
	{
		// 客户端不广播：它本来就是这条广播的接收方，
		// 而且客户端这一发是本地预测，没有权威性。
		return;
	}

	OwnerCharacter->BroadcastWeaponCosmetic(this, Fire);
}

void ARogueWeaponBase::PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire)
{
	// 默认什么都不做：事件型武器各自重写（子弹/火箭/迫击炮/刀片）。
	// 光束类武器的重写就是转调 PlayCosmeticBeam。
}

void ARogueWeaponBase::PlayCosmeticBeam(const FRogueWeaponCosmeticFire& Fire)
{
	// 用广播过来的起点/终点把这条光束画出来。
	// 这里生成的 Beam 只负责好看：它不索敌、不结算伤害
	// （伤害在服务端那一发里早就算完了）。
	SpawnLaserBeam(Fire.Start, Fire.End, Fire.bAlternateStyle, /*bImpactEffect=*/true, Fire.Param);
}
