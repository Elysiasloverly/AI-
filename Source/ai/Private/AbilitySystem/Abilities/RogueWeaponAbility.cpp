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

	// LocalPredicted：本人在本机预测开火（零延迟手感，改造前就是这样），
	// 服务端由这条激活请求在自己的 ASC 上跑一份权威的。别人的客户端不参与。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 持续能力不允许"再来一次"：重复激活没有意义，还会把冷却句柄弄乱。
	bRetriggerInstancedAbility = false;

	// 用它做激活入口（ARogueCharacter::EnsureWeaponAbilities 按 Tag 批量激活）。
	// Tag 是**资产标签**，随能力类一起在两端可知，不需要额外复制字段。
	// 注：成员 `AbilityTags` 在 5.5 已标记废弃，要用 SetAssetTags（只能在构造期设默认值）。
	FGameplayTagContainer AutoAttackTags;
	AutoAttackTags.AddTag(RogueGameplayTags::Ability_WeaponAutoAttack);
	SetAssetTags(AutoAttackTags);

	// 刻意**不**加 ActivationBlockedTags = State.Dead：
	// 死亡时能力的循环自己会停（见 RunFireLoopStep 第 0 步），而"倒地时连激活都被挡"
	// 会让"加载时就已经倒地"的客户端永远激活不了这份能力（本项目踩过一次同类坑：
	// 诊断代码被 bDead 早退挡掉，看起来像功能失效）。等复活后循环自然恢复。
}

void URogueWeaponAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 武器解析不出来（例如本机武器还没生成、或技能先于武器到达）就**不激活**：
	// 调用方每帧会重试，等条件具备再激活。这样比"先激活再每帧判空"更干净 ——
	// 后者会在日志里留下一堆"激活了但什么也没做"的实例。
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

	// 激活只发生一次（持续能力），所以这条日志不会刷屏；
	// 但它能一眼看出"哪台机器上、哪把武器进入了能力驱动"。
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

	// 0) 拥有者状态。改造前 ARogueCharacter::Tick 在 `if (bDead) return;` 之后就不到
	//    TickWeapons 了，也就是"倒地期间所有武器停火、复活后自动恢复"。
	//    这里用同一条判据，行为不变。
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

	// 2) 冷却：**唯一真相源是 ASC 上的冷却 GE**，不是武器里的 float 计时器。
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

	// 先清掉上一发的冷却再施加新的：ApplyGameplayEffectSpecToSelf 默认是"叠加"语义，
	// 不清的话同一把武器身上会挂着好几个冷却 GE，而句柄只记住最后一个 ——
	// 表现就是"冷却偶尔变得很长"（前一个更长的还在跑）。改造前是 `Timer = Cooldown` 直接覆盖，
	// 语义就是"覆盖"，所以这里显式对齐。
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

	// ⚠️⚠️ 施加冷却 GE 必须**显式带上这次激活的预测键**。踩过的坑（完整链路）：
	//
	//   1) 一开始用 `ASC->ApplyGameplayEffectSpecToSelf(Spec)`（不带预测键）。
	//      引擎在 `HasNetworkAuthorityToApplyGameplayEffect` 里有硬规则：
	//      `IsOwnerActorAuthoritative() || PredictionKey.IsValidForMorePrediction()`
	//      （AbilitySystemComponent.cpp:383）。客户端两者都不满足 → 返回**空句柄**。
	//   2) 于是改成引擎自己的写法 `ApplyGameplayEffectSpecToOwner(...)`（ApplyCooldown 就这么写）。
	//      结果客户端**依然是空句柄** —— 因为它内部用的是
	//      `GetPredictionKeyForNewAction()`（GameplayAbility.cpp 的 ApplyGameplayEffectSpecToOwner），
	//      那个键只有在**预测窗口内**（即能力激活的调用栈里）才有效；
	//      而我们的调用发生在逐帧心跳任务 `TickTask` 里，窗口早就关了。
	//   3) 正确做法：用 `CurrentActivationInfo.GetActivationPredictionKey()` ——
	//      它是这次激活的预测键，实测在客户端上"有效/本地客户端键/可继续预测"三项都为真。
	//
	// 失效时的症状非常隐蔽：冷却 GE 挂不上 → IsWeaponCoolingDown() 恒为假 →
	// **这把武器每帧开一枪**（实测客户端火箭 1531 发 / 迫击炮 9486 发，服务端只有 11/71）。
	// 所以下面保留了显式报错。
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

	// 按**类**找武器：每类武器在角色身上只有一把（ARogueCharacter::InitializeWeapons
	// 按 DT_WeaponConfig 每行生成一把，缺迫击炮时再补一把）。
	// 之所以能这么找：武器是本机各自生成的本地 Actor（不复制），
	// 而"类"两端都知道，所以这是唯一不需要额外复制字段的身份。
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

// ===========================================================
//  六把武器的能力子类：一行"我驱动哪把武器"
// ===========================================================
//  刻意不把这些映射写进数据表：DT_WeaponConfig 的架构（列与行）是"数据资产唯一入口"
//  这条约束的一部分，不该为了 C++ 类型映射去加列。这也和武器侧的
//  GetCountUpgradeType()（每把武器硬编码自己的数量升级类型）是同一种做法。

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
