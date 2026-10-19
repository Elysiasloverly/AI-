#include "AbilitySystem/Abilities/RogueWeaponAbility.h"

#include "AbilitySystem/Tasks/RogueAbilityTask_WeaponFireLoop.h"
#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"
#include "AbilitySystemComponent.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Combat/Weapons/RogueWeapon_Laser.h"
#include "Combat/Weapons/RogueWeapon_Mortar.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Combat/Weapons/RogueWeapon_Rocket.h"
#include "Combat/Weapons/RogueWeapon_Scythe.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"

// ===========================================================
//  基类
// ===========================================================

URogueWeaponAbility::URogueWeaponAbility()
{
	// 一个武器一份实例：每把武器都有自己的冷却句柄与缓存，必须是独立实例。
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 拥有者与服务端分别执行能力；其他客户端不执行这份拥有者能力。
	// 自定义开火、冷却和表现并不会仅因 LocalPredicted 就获得完整预测校正。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 持续能力不允许"再来一次"：重复激活没有意义，还会把冷却句柄弄乱。
	bRetriggerInstancedAbility = false;

	// 用它做激活入口（ARogueCharacter::EnsureWeaponAbilities 按 Tag 批量激活）。
	// Tag 是**资产标签**，随能力类一起在两端可知，不需要额外复制字段。
	// 注：成员 `AbilityTags` 在 5.5 已标记废弃，要用 SetAssetTags（只能在构造期设默认值）。
	FGameplayTagContainer AutoAttackTags;
	AutoAttackTags.AddTag(RogueGameplayTags::Ability_WeaponAutoAttack);
	SetAssetTags(AutoAttackTags);

	// 不用死亡标签阻止本持续能力激活；循环按角色状态暂停，存活后可继续。
	// 本策略依赖角色生命周期保持能力/武器有效，不能概括为所有死亡重生方式均自动恢复。
}

void URogueWeaponAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 武器尚未生成/解析失败时立即结束；角色的 EnsureWeaponAbilities 后续会重试。
	if (GetDrivenWeapon() == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/false, /*bWasCancelled=*/false);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 逐帧心跳：GameplayAbility 没有 Tick，开火节拍靠这个任务（见任务头文件的说明）。
	FireLoopTask = URogueAbilityTask_WeaponFireLoop::RunWeaponFireLoop(this);
	if (FireLoopTask != nullptr)
	{
		FireLoopTask->ReadyForActivation();
	}

	// 记录每次成功进入循环的实例；取消后重新激活或角色重建时还会再次记录。
	UE_LOG(LogTemp, Log, TEXT("[武器技能] %s 已激活：驱动武器=%s（本机=%s 玩家=%s）"),
		*GetClass()->GetName(),
		*GetNameSafe(GetDrivenWeapon()),
		GetOwnerCharacter() != nullptr && GetOwnerCharacter()->IsLocallyControlled() ? TEXT("本人") : TEXT("服务端"),
		*GetNameSafe(GetOwnerCharacter()));
}

bool URogueWeaponAbility::RunFireLoopStep(float DeltaSeconds)
{
	ARogueWeaponBase* Weapon = GetDrivenWeapon();
	if (!IsValid(Weapon))
	{
		return false;
	}

	// 0) 非存活期间暂停本次循环，保留持续能力供复活后继续使用。
	const ARogueCharacter* Character = GetOwnerCharacter();
	if (Character == nullptr || Character->IsDead())
	{
		return false;
	}

	// 1) 持续型武器（地狱塔）：节拍在武器内部（每个锁定目标各有自己的伤害跳与光束刷新间隔），
	//    能力只负责给它逐帧心跳。事件型武器才有"目标选择 → 开火 → 冷却"这三步。
	//    注意：这类武器的 FireCount 恒为 0（它没有"某一发"），看报告时要看 ContinuousTickCount。
	if (!Weapon->UsesTargetedFireLoop())
	{
		++ContinuousTickCount;
		Weapon->WeaponTick(DeltaSeconds);
		return false;
	}

	// 目标型武器检查本地句柄对应的冷却 GE；服务端与拥有者各自维护实例。
	if (IsWeaponCoolingDown())
	{
		return false;
	}

	// 3) 未解锁（生效数量为 0）就不参与索敌 —— 与改造前各武器开头的 `if (Count <= 0) return;` 等价。
	if (!Weapon->CanFireNow())
	{
		return false;
	}

	// 4) 目标选择。无目标时的行为与改造前一致：**不消耗冷却、下一帧继续找**
	//    （改造前 AttackTimer 会一直减到负数，敌人一进射程当帧就开火）。
	//    所以这里不能用"没目标就等 0.1 秒再试"那种写法 —— 那会引入可见的开火延迟。
	TArray<ARogueEnemy*> Targets;
	Weapon->CollectFireTargets(Targets);
	if (Targets.Num() == 0)
	{
		return false;
	}

	// 5) 开火。池化弹体、散射、广播都在武器里（能力不知道这些细节）。
	Weapon->FireAtTargets(Targets);
	++FireCount;

	// 6) 把这一发写成一次冷却：时长是武器算出来的**生效冷却**（数据表基础值 + GAS 加成）。
	StampWeaponCooldown(Weapon->GetAbilityCooldown());
	return true;
}

TSubclassOf<UGameplayEffect> URogueWeaponAbility::GetWeaponCooldownEffectClass() const
{
	return URogueWeaponCooldownEffect::StaticClass();
}

bool URogueWeaponAbility::IsWeaponCoolingDown() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC == nullptr || !CooldownHandle.IsValid())
	{
		return false;
	}

	const FActiveGameplayEffect* Active = ASC->GetActiveGameplayEffect(CooldownHandle);
	if (Active == nullptr)
	{
		// 已经到期并被移除 —— 可以开火了。
		return false;
	}

	const UWorld* World = GetWorld();
	const float WorldTime = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	return Active->GetTimeRemaining(WorldTime) > 0.0f;
}

float URogueWeaponAbility::GetCooldownRemainingForSelfTest() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC == nullptr || !CooldownHandle.IsValid())
	{
		return -1.0f;
	}

	const FActiveGameplayEffect* Active = ASC->GetActiveGameplayEffect(CooldownHandle);
	if (Active == nullptr)
	{
		return -1.0f;
	}

	const UWorld* World = GetWorld();
	return Active->GetTimeRemaining(World != nullptr ? World->GetTimeSeconds() : 0.0f);
}

void URogueWeaponAbility::StampWeaponCooldown(float Seconds)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC == nullptr || Seconds <= 0.0f)
	{
		return;
	}

	// 显式移除本句柄记录的上一个效果，再应用本次冷却。
	// 该 GE 未配置 Stacking，多次应用可产生独立实例；本循环只检查所保存的句柄。
	// 因此旧实例未清理会残留状态/开销，但并不会自动延长本句柄查询到的冷却。
	if (CooldownHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(CooldownHandle);
	}

	const TSubclassOf<UGameplayEffect> CooldownClass = GetWeaponCooldownEffectClass();
	if (CooldownClass == nullptr)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(CooldownClass, GetAbilityLevel(), Context);
	if (!SpecHandle.IsValid() || SpecHandle.Data == nullptr)
	{
		return;
	}

	// 时长走 SetByCaller（见 URogueWeaponCooldownEffect 的说明）：
	// "加成怎么换算成冷却"是武器的知识，GE 只负责按这个秒数计时。
	SpecHandle.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Cooldown, Seconds);

	// 当前每次心跳复用激活时的 PredictionKey。权威端可直接应用；客户端需有效预测键。
	// UE 5.5 的 IsValidForMorePrediction 仅检查本地客户端键形态，不证明该动作
	// 仍处于有效预测窗口，或服务器会为本次开火生成一一对应的确认。
	// 预测 GE 会在键拒绝/追平时移除；当前实现没有为后续每次开火创建并同步新预测窗口，
	// 也没有把 CooldownHandle 重新映射为确认后的实例，不能视为完整的持续开火预测方案。
	// 下方日志仅能检测立即应用失败，不能验证长期同步正确性。
	const FPredictionKey ActivationPredictionKey = CurrentActivationInfo.GetActivationPredictionKey();
	const FActiveGameplayEffectHandle NewHandle =
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get(), ActivationPredictionKey);
	if (!NewHandle.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[武器技能] %s 冷却 GE 施加失败：本机=%s 武器=%s 时长=%.2f | 权威=%d 预测键有效=%d 本地客户端键=%d 可继续预测=%d"),
			*GetClass()->GetName(),
			GetOwnerCharacter() != nullptr && GetOwnerCharacter()->IsLocallyControlled() ? TEXT("本人") : TEXT("服务端"),
			*GetNameSafe(GetDrivenWeapon()),
			Seconds,
			ASC->IsOwnerActorAuthoritative() ? 1 : 0,
			ActivationPredictionKey.IsValidKey() ? 1 : 0,
			ActivationPredictionKey.IsLocalClientKey() ? 1 : 0,
			ActivationPredictionKey.IsValidForMorePrediction() ? 1 : 0);
	}
	CooldownHandle = NewHandle;
}

ARogueWeaponBase* URogueWeaponAbility::GetDrivenWeapon() const
{
	if (IsValid(CachedWeapon))
	{
		return CachedWeapon;
	}

	const ARogueCharacter* Character = GetOwnerCharacter();
	if (Character == nullptr)
	{
		return nullptr;
	}

	const TSubclassOf<ARogueWeaponBase> WeaponClass = GetDrivenWeaponClass();
	if (WeaponClass == nullptr)
	{
		return nullptr;
	}

	// 本地武器由各端生成，不复制；按类型匹配对应实例。
	// 当前约定每个类型仅一把，若允许同类多把需增加可跨端对应的稳定身份。
	for (ARogueWeaponBase* Weapon : Character->GetWeapons())
	{
		if (IsValid(Weapon) && Weapon->GetClass() == WeaponClass.Get())
		{
			CachedWeapon = Weapon;
			return Weapon;
		}
	}

	return nullptr;
}

ARogueCharacter* URogueWeaponAbility::GetOwnerCharacter() const
{
	return Cast<ARogueCharacter>(GetAvatarActorFromActorInfo());
}

// 每个能力子类返回对应武器类型，与武器 GetFireAbilityClass 构成配对映射。
// 新增武器类型时需同时维护两个方向。

TSubclassOf<ARogueWeaponBase> URogueWeaponAbility_Projectile::GetDrivenWeaponClass() const
{
	return ARogueWeapon_Projectile::StaticClass();
}

TSubclassOf<ARogueWeaponBase> URogueWeaponAbility_Rocket::GetDrivenWeaponClass() const
{
	return ARogueWeapon_Rocket::StaticClass();
}

TSubclassOf<ARogueWeaponBase> URogueWeaponAbility_Mortar::GetDrivenWeaponClass() const
{
	return ARogueWeapon_Mortar::StaticClass();
}

TSubclassOf<ARogueWeaponBase> URogueWeaponAbility_Laser::GetDrivenWeaponClass() const
{
	return ARogueWeapon_Laser::StaticClass();
}

TSubclassOf<ARogueWeaponBase> URogueWeaponAbility_HellTower::GetDrivenWeaponClass() const
{
	return ARogueWeapon_HellTower::StaticClass();
}

TSubclassOf<ARogueWeaponBase> URogueWeaponAbility_Scythe::GetDrivenWeaponClass() const
{
	return ARogueWeapon_Scythe::StaticClass();
}
