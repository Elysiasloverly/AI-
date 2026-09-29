#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "RogueStateEffects.generated.h"

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
