#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "RogueStateEffects.generated.h"

class UAbilitySystemComponent;

/**
 * 三个"给持有者授予一个状态标签"的 GameplayEffect 的公共基类。
 *
 * 为什么要有这个基类（而不是三个各写一遍）：
 *   在 UE 5.5 里，"授予一个标签"这件事是**四行代码 + 一个组件对象**，而且有两个不写就静默失效的坑：
 *     1) `InheritableOwnedTagsContainer` 自 5.3 起废弃，往它里面塞标签**一个都不会授予**
 *        （`GetGrantedTags()` 读的是由 GEComponents 重建的 `CachedGrantedTags`）；
 *     2) 构造期只能 `CreateDefaultSubobject`，因为 `UGameplayEffect::AddComponent<>()` 内部是
 *        `NewObject(this, NAME_None, ...)`，在构造函数里是非法用法，会直接 Fatal。
 *   这两条以前在三个文件里各解释了一遍（注释比代码长三倍）。现在只留一份。
 *
 * ⚠️ 迁移/维护提示：新增"授予状态标签"的效果时，**派生自本类**并调用 GrantStateTag()，
 *    不要再去碰 InheritableOwnedTagsContainer，也不要重复踩 CreateDefaultSubobject 那个坑。
 */
UCLASS(Abstract)
class AI_API URogueStateTagEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URogueStateTagEffect();

protected:
	/** 在派生类构造函数里调用：让本效果在持有者身上授予这个状态标签。 */
	void GrantStateTag(FGameplayTag StateTag);
};

/**
 * 死亡状态效果：授予 State.Dead，持续到被显式移除（复活时会移除）。
 *
 * 为什么死亡要表示成一个 GE，而不是直接调 Die()：
 *   · 死亡变成 GAS 的**原生状态**：State.Dead 会随效果复制，客户端不需要额外同步一个 bool；
 *   · 技能可以用 `ActivationBlockedTags = State.Dead` 直接挡住，不用每个技能里写 if；
 *   · `URogueAttributeSet` 不需要认识 ARogueCharacter / ARogueEnemy —— 它只负责
 *     "血量归零 → 施加死亡效果"，谁死谁自己监听标签做善后。数值与玩法就此解耦。
 *
 * 用 Infinite 而不是 HasDuration：死亡是持续状态，不会自己过期。
 */
UCLASS()
class AI_API URogueDeathEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueDeathEffect();
};

/**
 * "刚受伤"状态效果：授予 State.RecentlyDamaged，**时长取自持有者的 ArmorRechargeDelay 属性**。
 *
 * 用途：受伤后延迟一段时间护甲才开始回复。做成状态标签而不是一个私有 float 计时器，
 * 好处是"是否处于延迟期"可以查询、可以复制（HUD/调试都能看到），
 * 护甲回复逻辑也只需要问一句"身上有没有这个标签"。
 *
 * 时长用属性而不是常数：它是升级可以改的数值（迁移前是 ArmorRechargeDelayRemaining 这个 float）。
 * 必须 bSnapshot：持续时间在施加瞬间定死，否则中途属性变化会让剩余时长漂移。
 */
UCLASS()
class AI_API URogueRecentlyDamagedEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueRecentlyDamagedEffect();
};

/**
 * 冲刺冷却效果：授予 Cooldown.Dash，**时长取自持有者的 DashCooldownDuration 属性**。
 *
 * 它是 `URogueGameplayAbility_Dash::CooldownGameplayEffectClass`：
 * GAS 内置的冷却判定读的就是这个 GE 的 `GetGrantedTags()`（`UGameplayAbility::GetCooldownTags()`
 * 的实现是 `return CDGE ? &CDGE->GetGrantedTags() : nullptr;`），
 * 所以"授予 Cooldown.Dash"这一条是冲刺冷却能否生效的关键 —— 曾经因为标签没真的授予，
 * 冲刺**完全没有冷却**，而且没有任何报错。
 */
UCLASS()
class AI_API URogueDashCooldownEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueDashCooldownEffect();
};

/**
 * 武器开火冷却效果：授予 Cooldown.Weapon.AutoAttack，**时长由 SetByCaller(Data.Cooldown) 给出**。
 *
 * 为什么时长走 SetByCaller 而不是像冲刺那样绑属性：
 *   每把武器的冷却算法都不同（换算系数、下限都不一样，见各武器的 GetEffectiveCooldown），
 *   而"这个加成本武器怎么用"是**武器自己的知识**。所以流程是：
 *     武器算出 `GetAbilityCooldown()` → 能力把它写成一次 SetByCaller → 冷却 GE 按它计时。
 *   同一个 GE 类因此能被六把武器共用，而每把武器仍然各算各的。
 *
 * ⚠️ 与冲刺冷却的区别：这个 GE **不是** `CooldownGameplayEffectClass`。
 *    武器能力是"持续激活"的（激活一次就一直跑），GAS 内置的"冷却期间不能重新激活"
 *    对持续能力没有意义。它的作用是**节拍**：开火循环每帧问一次"这把武器的冷却 GE
 *    还在不在"，在就跳过这一帧 —— 节奏的唯一真相源是这个 GE，而不是武器里的 float 计时器。
 */
UCLASS()
class AI_API URogueWeaponCooldownEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueWeaponCooldownEffect();
};

/**
 * 敌人攻击冷却效果：授予 Cooldown.Enemy.Attack，时长同样由 SetByCaller(Data.Cooldown) 给出。
 *
 * 三种攻击（接触 / 远程 / 地面波）共用这一个 GE 类：
 *   · 它们各自的冷却数值来自 DT_FRogueEnemyArchetypeRow 的不同字段
 *     （ContactInterval / Ranged.NextShotCooldown / GroundWave.AttackCooldown），
 *     由能力在激活时按秒数写进来；
 *   · 各自的能力实例有自己的冷却 GE 实例，所以"共用类"不会让三种攻击互相卡冷却。
 *
 * ⚠️ 分工要说清楚：**AI 的计时器仍然是"要不要打"的决策节流**（它同时还要判断距离、
 *    目标是否有效，这些不属于能力层）。这个 GE 是能力层的权威冷却：
 *    GAS 用它挡住同一次攻击的重复激活，也让"这只怪的远程攻击在冷却中"变成可查询的 GAS 状态。
 */
UCLASS()
class AI_API URogueEnemyAttackCooldownEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueEnemyAttackCooldownEffect();
};

/**
 * 倒地等待：授予 State.Downed，时长 = SetByCaller(Data.Duration) = 复活等待秒数。
 *
 * 它是"还剩多久复活"的**唯一真相源**（原先是一个复制到 PlayerState 上的截止时刻 + 三处各自比对）。
 * 换成 GE 之后：
 *   · 时长本身随 GE 复制（玩家 ASC 是 Mixed 模式，带时长的 GE 会同步给本人），
 *     所以客户端 HUD 直接读"这个 GE 还剩多少秒"，不需要再复制一个时间戳；
 *   · 到期自动消失，"是否还在倒地等待"变成一次标签/GE 查询，而不是"比两个时钟"；
 *   · 团灭取消等待 = 移除这个 GE。
 *
 * ⚠️ 注意它**不**负责"人是不是倒了"：那是 `State.Dead`（URogueDeathEffect）的事。
 *    两个状态刻意分开：Dead = 已经倒下（伤害闸门、技能阻塞都看它），
 *    Downed = 正在等待复活（只用于倒计时与复活触发）。
 */
UCLASS()
class AI_API URogueDownedEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueDownedEffect();
};

/**
 * 复活无敌：授予 State.Invulnerable，时长 = SetByCaller(Data.Duration)。
 *
 * 原先它是"loose tag + 角色上的一个 float 截止时刻"，靠 Tick 比对服务器时间解除。
 * 换成带时长的 GE 之后：到期由 GAS 自己移除（不再依赖 Tick 是否在跑 ——
 * 这个项目已经踩过两次"倒下的角色不再 Tick，于是清理逻辑永远不执行"的坑）。
 */
UCLASS()
class AI_API URogueInvulnerableEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueInvulnerableEffect();
};

/** 取"倒地等待"还剩多少秒：读 ASC 上那个带时长的 GE；没有生效中的就返回 0。
 *
 *  放在这里的理由：服务端（复活轮询）与客户端（HUD 倒计时）都要问同一个问题，
 *  而两边用的是同一个 ASC 类型。实现只有一份，就不会出现"两边算法不一致"这种问题。 */
AI_API float GetDownedSecondsRemaining(const UAbilitySystemComponent* ASC);

// ===========================================================
//  自然回复：生命 / 护甲（Periodic GameplayEffect）
// ===========================================================

/**
 * 自然回复的公共基类（**Periodic GE**）。
 *
 * 迁移前，生命回复与护甲回复是 `ARogueCharacter` 上的两个**逐帧函数**
 * （`HandleRecovery` / `HandleArmorRecharge`）：每帧自己读属性、自己乘 DeltaSeconds、
 * 自己判标签、自己钳上限。合并到这里之后由 GAS 接管：
 *
 * | 迁移前（逐帧 C++） | 迁移后（GE 机制） |
 * |---|---|
 * | 每帧调用一次 | `Period` —— 每 0.25 秒结算一次 |
 * | `Rate * DeltaSeconds` | `AttributeBased` 修饰器量级（`Rate × Period`） |
 * | `if (HasMatchingGameplayTag(...)) return;` | `OngoingTagRequirements`（声明式标签条件） |
 * | 自己 `FMath::Min(Max, ...)` 钳上限 | 交给 `URogueAttributeSet::PreAttributeChange` |
 *
 * **最关键的一条**：`AttributeBased` 的 `bSnapshot` 必须是 **false**。
 * 为 true 会在施加瞬间把速率快照下来，之后升级改 `HealthRegenPerSecond` /
 * `ArmorRechargeRate` 就**再也不生效**，而且不会有任何报错。
 *
 * ⚠️ 客户端不会执行周期结算：引擎在 `GameplayEffect.cpp:4325` 要求
 *    `Owner->IsOwnerActorAuthoritative()` 才创建周期定时器，客户端拿到的副本只负责显示
 *    复制过来的属性值。所以这个 GE 不会重现迁移前"客户端自己也回甲、和服务端复制打架"
 *    导致护甲条来回跳的问题。
 */
UCLASS(Abstract)
class AI_API URoguePeriodicRegenEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URoguePeriodicRegenEffect();

protected:
	/** 派生类构造里调用：每个结算周期给 `TargetAttribute` 加 `RateAttribute × Period`。 */
	void ConfigureRegen(FGameplayAttribute TargetAttribute, FGameplayAttribute RateAttribute);

	/** 身上带着 `BlockingTag` 时暂停回复（写进 OngoingTagRequirements，可多次调用叠加）。
	 *
	 *  条件不满足时 GE 进入 Inhibited —— 效果保留、周期暂停，条件消失后自动继续，
	 *  不需要任何人手动移除/重挂。 */
	void BlockRegenWhileTagPresent(FGameplayTag BlockingTag);
};

/** 生命自然回复：每周期 `Health += HealthRegenPerSecond × Period`，`State.Dead` 期间暂停。
 *  （迁移前 `HandleRecovery` 靠 `Tick` 里那句 `if (bDead) return;` 挡住死亡期间回复。） */
UCLASS()
class AI_API URogueHealthRegenEffect : public URoguePeriodicRegenEffect
{
	GENERATED_BODY()

public:
	URogueHealthRegenEffect();
};

/** 护甲自然回复：每周期 `Armor += ArmorRechargeRate × Period`。
 *
 *  除了 `State.Dead`，**`State.RecentlyDamaged` 期间也暂停** ——
 *  这两条标签条件合起来就是"受伤后等 `ArmorRechargeDelay` 秒才开始回甲"这条完整规则，
 *  而它现在是**声明式**的（读 GE 就能看懂），不需要再去 C++ 里找 if。 */
UCLASS()
class AI_API URogueArmorRechargeEffect : public URoguePeriodicRegenEffect
{
	GENERATED_BODY()

public:
	URogueArmorRechargeEffect();
};
