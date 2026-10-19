// 武器系统基类 —— 通用辅助函数实现
#include "Combat/Weapons/RogueWeaponBase.h"

#include "Engine/World.h"

#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Pooling/RogueCombatPools.h"
#include "Enemies/RogueEnemy.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Visuals/RogueLaserBeam.h"
#include "Combat/Projectiles/RogueMortarProjectile.h"
#include "Combat/Projectiles/RogueOrbitingBlade.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
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
	// 默认选择范围内最近的一个敌人；多目标武器重写此函数。
	if (ARogueEnemy* Target = FindNearestEnemy(GetEffectiveRange()))
	{
		OutTargets.Add(Target);
	}
}

void ARogueWeaponBase::FireAtTargets(const TArray<ARogueEnemy*>& Targets)
{
	// 能力驱动的武器应重写此函数；误用基类实现时输出错误日志。
	UE_LOG(LogTemp, Error, TEXT("[武器] %s 没有实现 FireAtTargets —— 它被能力驱动了但永远打不出伤害"),
		*GetClass()->GetName());
}

const FRogueWeaponBonusValues& ARogueWeaponBase::GetWeaponBonusValues() const
{
	// 从 Character 的复制快照读取统一加成输入，具体换算由各武器完成。
	// 服务器在刷新快照时读取属性集；客户端等待复制，数值可能短暂滞后。
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

		// ProjectileCount 是主武器加成；未覆盖类型的子类会使用配置列（默认亦为此项）。
		case ERogueUpgradeType::ProjectileCount: WeaponSpecificBonus = FMath::RoundToInt(Bonuses.ProjectileCount); break;
		default: WeaponSpecificBonus = 0; break;
		}
	}

	// 基础数量不为正且自身数量加成不为正时保持未解锁。
	if (Config.Count <= 0 && WeaponSpecificBonus <= 0)
	{
		return 0;
	}

	// 数量上限：**数据表配置，-1 = 不限制**
	const int32 Combined = FMath::Max(1, Config.Count + WeaponSpecificBonus);
	return Config.MaxCount >= 0 ? FMath::Min(Combined, Config.MaxCount) : Combined;
}

float ARogueWeaponBase::GetEffectiveDamage() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Combined = Config.Damage + Bonuses.AttackPower;
	// 伤害上限：-1 = 不限制
	return Config.MaxDamage >= 0.0f ? FMath::Min(Combined, Config.MaxDamage) : Combined;
}

float ARogueWeaponBase::GetEffectiveCooldown() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Reduced = Config.Cooldown - Bonuses.AttackSpeed;

	// 配置下限 >= 0 时使用该值；负值取消配置限制，但仍将冷却钳到 0。
	// 是否逐帧开火还取决于能力循环、目标与激活条件，不承诺固定射速或性能。
	return Config.MinCooldown >= 0.0f
		? FMath::Max(Config.MinCooldown, Reduced)
		: FMath::Max(0.0f, Reduced);
}

float ARogueWeaponBase::GetEffectiveRange() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Combined = Config.Range + Bonuses.AttackRange;
	// 范围上限：-1 = 不限制
	return Config.MaxRange >= 0.0f ? FMath::Min(Combined, Config.MaxRange) : Combined;
}

float ARogueWeaponBase::GetEffectiveProjectileSpeed(float BaseSpeed) const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Combined = BaseSpeed + Bonuses.ProjectileSpeed * GetProjectileSpeedBonusScale();
	// 弹速上限：-1 = 不限制
	return Config.MaxProjectileSpeed >= 0.0f ? FMath::Min(Combined, Config.MaxProjectileSpeed) : Combined;
}

int32 ARogueWeaponBase::GetEffectiveRefractionCount() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const int32 Combined = FMath::Max(0, Config.RefractionCount
		+ FMath::RoundToInt(Bonuses.LaserRefraction));

	// 非负配置值限制最终折射次数；负值不设上限，下限 0 已在合并时处理。
	return Config.MaxRefractionCount >= 0 ? FMath::Min(Combined, Config.MaxRefractionCount) : Combined;
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
	// 复用当前 World 的表现裁剪阈值；不是帧耗时测量。
	return Tracker != nullptr && Tracker->ShouldCullCombatEffects();
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
	if (OwnerCharacter == nullptr || !OwnerCharacter->IsAuthoritySide())
	{
		// 这里按进程 NetMode 禁止客户端广播。HasAuthority 判断的是 Actor 的本地角色：
		// 正常服务器复制的自控 Pawn 在客户端是 AutonomousProxy；客户端本地生成的
		// 非复制 Actor 则可能是 Authority，不能把所有权或本地控制等同于 Authority。
		return;
	}

	OwnerCharacter->BroadcastWeaponCosmetic(this, Fire);
}

void ARogueWeaponBase::PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire)
{
	// 默认什么都不做：事件型武器各自重写（子弹/火箭/迫击炮/光束）。
	// 光束类武器的重写就是转调 PlayCosmeticBeam。
}

void ARogueWeaponBase::PlayCosmeticBeam(const FRogueWeaponCosmeticFire& Fire)
{
	// 用广播过来的起点/终点把这条光束画出来。
	// 这里生成的 Beam 只负责好看：它不索敌、不结算伤害
	// （伤害在服务端那一发里早就算完了）。
	SpawnLaserBeam(Fire.Start, Fire.End, Fire.bAlternateStyle, /*bImpactEffect=*/true, Fire.Param);
}
