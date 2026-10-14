#pragma once

#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"
#include "GameplayEffect.h"
#include "RogueAttributeUpgradeEffect.generated.h"

/**
 * "属性类升级"效果的公共基类（**Infinite + Stacking**）。
 *
 * ## 为什么必须"一种升级一个子类"
 *
 * 一份 GE 只能改一个属性（修饰器的 `Attribute` 是烤在 CDO 上的），而项目有 20 种升级。
 * 改造前的做法是**一个类 + 动态载荷 Tag**，由 `URogueUpgradeExecution` 在运行时查表决定改哪个属性。
 *
 * 那个做法与 Stacking **根本冲突**：引擎的 `FindStackableActiveGameplayEffect`
 * **只按 `Spec.Def` 匹配**可叠加实例（`ActiveEffect.Spec.Def == Spec.Def`）。
 * 20 种升级共用一个类 → 开启 Stacking 会让它们**共用一个栈**：
 * 买"伤害"再买"生命"只会叠成 1 个栈，而执行计算只认其中一个载荷 Tag → **数值错乱**。
 *
 * 所以拆成 20 个子类，每个在构造里把目标属性烤死。
 * 顺带的好处：`URogueUpgradeExecution` 不再需要（属性在编译期就确定了）。
 *
 * ## 数值语义（与改造前 Instant 连加等价）
 *
 * | | 值 |
 * |---|---|
 * | `DurationPolicy` | `Infinite` —— Stacking 必须驻留（Instant 的 GE 不会被保留） |
 * | `StackingType` | `AggregateByTarget` —— 每个目标一份栈 |
 * | `StackLimitCount` | **0 = 不限制**（引擎只在 `> 0` 时钳制）。想给某张卡设"最多买 N 次"就填正整数 |
 * | 量级 | `SetByCaller(Data.Magnitude)`，引擎会**乘以层数** |
 *
 * 引擎在 `GameplayEffectUtilities::ComputeStackedModifierMagnitude` 里做 `量级 × 层数`
 * （Additive 的 OperationBias 为 0），且对**执行输出**同样生效。
 * 所以"买 N 次 = N × 单次数值"，与改造前 Instant 连加 N 次**逐项相等**。
 *
 * ⚠️ **代价**：Infinite GE 只能改 **CurrentValue**（走属性聚合器），不像 Instant 那样改 BaseValue。
 *    因此"升级最大生命 / 护甲上限时等量回复"那条联动**不能**再依赖
 *    `URogueAttributeSet::PostGameplayEffectExecute`（那个钩子只对 Instant 触发），
 *    已改由 `ARogueCharacter` 监听属性变化委托实现 —— 见 `HandleMaxHealthChanged` 的说明。
 */
UCLASS(Abstract)
class AI_API URogueAttributeUpgradeEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URogueAttributeUpgradeEffect();

protected:
	/** 派生类构造里调用：把"这次升级改哪个属性"烤进修饰器。
	 *  改多少走 `SetByCaller(Data.Magnitude)`（冲刺冷却是负向升级，调用方传负数）。 */
	void ConfigureUpgrade(FGameplayAttribute TargetAttribute);
};

// ===================================================================
//  20 个子类 —— 一种升级一个。
//  命名与 ERogueUpgradeType 一一对应（映射见 RogueUpgradeEffectMap）。
//  每个子类的 .cpp 里只有一行：ConfigureUpgrade(对应的属性访问器)。
// ===================================================================

UCLASS()
class AI_API URogueUpgradeEffect_MaxHealth : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_MaxHealth();
};

UCLASS()
class AI_API URogueUpgradeEffect_MoveSpeed : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_MoveSpeed();
};

UCLASS()
class AI_API URogueUpgradeEffect_DashCooldown : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_DashCooldown();
};

UCLASS()
class AI_API URogueUpgradeEffect_AttackPower : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_AttackPower();
};

UCLASS()
class AI_API URogueUpgradeEffect_AttackSpeed : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_AttackSpeed();
};

UCLASS()
class AI_API URogueUpgradeEffect_PickupRadius : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_PickupRadius();
};

UCLASS()
class AI_API URogueUpgradeEffect_Recovery : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_Recovery();
};

UCLASS()
class AI_API URogueUpgradeEffect_AttackRange : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_AttackRange();
};

UCLASS()
class AI_API URogueUpgradeEffect_ProjectileSpeed : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_ProjectileSpeed();
};

UCLASS()
class AI_API URogueUpgradeEffect_ProjectileCount : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_ProjectileCount();
};

UCLASS()
class AI_API URogueUpgradeEffect_Armor : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_Armor();
};

UCLASS()
class AI_API URogueUpgradeEffect_ArmorCapacity : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_ArmorCapacity();
};

UCLASS()
class AI_API URogueUpgradeEffect_ExperienceGain : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_ExperienceGain();
};

UCLASS()
class AI_API URogueUpgradeEffect_ScytheCount : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_ScytheCount();
};

UCLASS()
class AI_API URogueUpgradeEffect_RocketCount : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_RocketCount();
};

UCLASS()
class AI_API URogueUpgradeEffect_LaserCount : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_LaserCount();
};

UCLASS()
class AI_API URogueUpgradeEffect_HellTowerCount : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_HellTowerCount();
};

UCLASS()
class AI_API URogueUpgradeEffect_LaserRefraction : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_LaserRefraction();
};

UCLASS()
class AI_API URogueUpgradeEffect_MortarCount : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_MortarCount();
};

UCLASS()
class AI_API URogueUpgradeEffect_MortarBlastRadius : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_MortarBlastRadius();
};

/**
 * 升级类型 → 该用哪个 GE 子类。
 *
 * 这个映射**取代了改造前 `URogueUpgradeExecution` 里那张"载荷 Tag → 属性"的运行时表**：
 * 现在属性烤在类上，所以这里只需要把枚举映射到类。
 *
 * 返回 nullptr 表示该枚举没有对应实现（枚举加了但映射忘了加）——
 * 调用方必须处理，否则会表现成"卡牌能抽到但没效果"。
 */
namespace RogueUpgradeEffectMap
{
	AI_API TSubclassOf<UGameplayEffect> GetEffectClass(ERogueUpgradeType UpgradeType);
}
