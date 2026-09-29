// 武器系统基类 —— 通用辅助函数实现
#include "Combat/RogueWeaponBase.h"

#include "Engine/World.h"

#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "AbilitySystem/RogueAttributeSet.h"
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

void ARogueWeaponBase::CollectFireTargets(TArray<ARogueEnemy*>& OutTargets) const
{
	// 基类默认：范围内最近的**一个**敌人（子弹武器改造前就是 FindNearestEnemy）。
	// 打多个目标的武器（火箭/迫击炮/激光）重写这里。
	if (ARogueEnemy* Target = FindNearestEnemy(GetEffectiveRange()))
	{
		OutTargets.Add(Target);
	}
}

void ARogueWeaponBase::FireAtTargets(const TArray<ARogueEnemy*>& Targets)
{
	// 基类没有"开火"这一说（每把武器的这一发都不一样），子类实现。
	// 留着空实现是为了让"忘了实现"表现为"这把武器不开火"而不是编译不过 ——
	// 但那样会静默失效，所以这里给一次明确的报错。
	UE_LOG(LogTemp, Error, TEXT("[武器] %s 没有实现 FireAtTargets —— 它被能力驱动了但永远打不出伤害"),
		*GetClass()->GetName());
}

const FRogueWeaponBonusValues& ARogueWeaponBase::GetWeaponBonusValues() const
{
	// 加成来自拥有者角色身上的**复制快照**（服务端每帧从属性集同步，变化才推）。
	//
	// 为什么要这一层而不是直接读 ASC 上的属性集：武器生效值是"数据表基础值 + 加成×本武器系数"，
	// 各端要各算一遍，所以需要一份跨端一致的"加成输入"。
	// 快照挂在 ARogueCharacter 上（与 bDead 同一条复制通道）；双进程自检已验证
	// 客户端读到的加成与服务端一致（见 Docs/GAS-Main-Architecture-Plan.md 的 Phase 1b 记录）。
	static const FRogueWeaponBonusValues EmptyValues;

	if (OwnerCharacter == nullptr)
	{
		return EmptyValues;
	}

	return OwnerCharacter->GetWeaponBonusSnapshot();
}

int32 ARogueWeaponBase::GetEffectiveCount() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();

	// 只读**本武器自己那一类**数量加成（见 GetCountUpgradeType 的说明）
	int32 WeaponSpecificBonus = 0;
	{
		switch (GetCountUpgradeType())
		{
		case ERogueUpgradeType::ScytheCount:    WeaponSpecificBonus = FMath::RoundToInt(Bonuses.ScytheCount); break;
		case ERogueUpgradeType::RocketCount:    WeaponSpecificBonus = FMath::RoundToInt(Bonuses.RocketCount); break;
		case ERogueUpgradeType::LaserCount:     WeaponSpecificBonus = FMath::RoundToInt(Bonuses.LaserCount); break;
		case ERogueUpgradeType::HellTowerCount: WeaponSpecificBonus = FMath::RoundToInt(Bonuses.HellTowerCount); break;
		case ERogueUpgradeType::MortarCount:    WeaponSpecificBonus = FMath::RoundToInt(Bonuses.MortarCount); break;

		// 主武器以外的武器如果既没重写 GetCountUpgradeType、数据列也没填，
		// 会落到这里 —— 那时**不算任何加成**，比"读错别人的加成"安全得多。
		case ERogueUpgradeType::ProjectileCount: WeaponSpecificBonus = FMath::RoundToInt(Bonuses.ProjectileCount); break;
		default: WeaponSpecificBonus = 0; break;
		}
	}

	// 基础数量为 0 = 该武器未解锁；此时只有它自己的数量升级能把它拉起来（沿用自研版本的语义：
	// 那条升级会把 0 变成 1）。
	if (Config.Count <= 0 && WeaponSpecificBonus <= 0)
	{
		return 0;
	}

	return FMath::Max(1, Config.Count + WeaponSpecificBonus);
}

float ARogueWeaponBase::GetEffectiveDamage() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	return Config.Damage + Bonuses.AttackPower;
}

float ARogueWeaponBase::GetEffectiveCooldown() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Reduced = Config.Cooldown - Bonuses.AttackSpeed;
	// 0.15 秒下限与自研版本一致（旧的 ApplySharedSpeedBonus 也是这么夹的）
	return FMath::Max(0.15f, Reduced);
}

float ARogueWeaponBase::GetEffectiveRange() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	return Config.Range + Bonuses.AttackRange;
}

float ARogueWeaponBase::GetEffectiveProjectileSpeed(float BaseSpeed) const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.ProjectileSpeed;
	return BaseSpeed + Bonus * GetProjectileSpeedBonusScale();
}

int32 ARogueWeaponBase::GetEffectiveRefractionCount() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const int32 Combined = Config.RefractionCount
		+ FMath::RoundToInt(Bonuses.LaserRefraction);
	// 旧实现的夹取范围（0..8）原样保留
	return FMath::Clamp(Combined, 0, 8);
}

// 注：OnUpgradeApplied / ApplyShared*Bonus 已删除 —— 武器升级现在落成玩家 ASC 上的
// URogueAttributeSet 加成属性，由上面的 GetEffective* 读取（见头文件说明）。

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
