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

	// ---- 攻速**无设计上限**（原本这里夹 0.15 秒，已按要求去掉）----
	//
	// 历史：0.15 秒下限沿用自研版本（旧的 ApplySharedSpeedBonus 也这么夹），
	// 它的作用是"设计上限"—— 攻速加成堆再多，每秒最多也就 6.7 发。
	// 现在去掉，攻速加成可以一直堆下去。
	//
	// ⚠️ 但**必须保留一个技术下限（> 0）**，否则会退化成一个危险的边界：
	//     冷却 = 0 或负数时，武器 Tick 的判据 `TimeSinceLastShot >= Cooldown` **恒为真**
	//     → 每帧开火。按项目默认 100Hz 算就是**每把武器每秒 100 发**，
	//     六把武器一起就是 600 发/秒，每发还要生成弹体 —— 帧率会直接崩掉。
	//
	// 所以下限设在 0.01 秒 = **每秒最多 100 发**：相对原来的 0.15 秒（6.7 发/秒）
	// 已经放宽了 15 倍，实战中等同"无上限"（没有任何升级能堆到这个密度），
	// 但它挡住了"冷却归零 → 逐帧开火"这个会把游戏跑崩的退化情况。
	//
	// 这个下限现在是**数据表里可配的**：`FRogueWeaponConfig::MinCooldown`（"上限"分组）。
	// 想给某把武器封攻速就在它在 DT_WeaponConfig 里的那一行填值。
	//
	// 语义与其它上限统一：**-1 = 不设下限**。此时仍会把结果夹在 >= 0
	//（负冷却与 0 冷却等价 —— 判据 `TimeSinceLastShot >= Cooldown` 都恒为真，
	//  所以"不设下限"的实际含义就是"可以逐帧开火"）。
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

	// 折射上限：**数据表配置，-1 = 不限制**（FRogueWeaponConfig::MaxRefractionCount）。
	// 原来这里硬编码夹死 0..8（旧实现的遗留），上界现在交回数据表；
	// 下界 0 保留 —— 负数折射没有意义。
	return Config.MaxRefractionCount >= 0 ? FMath::Min(Combined, Config.MaxRefractionCount) : Combined;
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
	// 判据只有一份（Tracker 上的 ShouldCullCombatEffects）。
	// 这里原本还有一个 `GetActiveEnemyCount() >= 40` 的硬编码兜底，它抢先命中，
	// 使配置项 CombatEffectsCullEnemyCount（默认 52）对武器路径完全失效，
	// 并让武器（40）与弹体/命中特效（52）在同一帧里用了两个不同的阈值。已删除。
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
		// 客户端不广播：它本来就是这条广播的接收方，
		// 而且客户端这一发是本地预测，没有权威性。
		// ⚠️ 判据必须是 IsAuthoritySide()（GetNetMode）而不是 HasAuthority() ——
		// 客户端上本机拥有的角色 HasAuthority() 也是 true，闸门会失效，
		// 客户端会一路走到 Multicast RPC。见 ARogueCharacter::BroadcastWeaponCosmetic。
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
