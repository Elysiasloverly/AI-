#pragma once

#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"
#include "GameplayEffect.h"
#include "RogueAttributeUpgradeEffect.generated.h"

/** 属性升级公共基类：无周期 Infinite + AggregateByTarget。
 * GE 可以包含多个属性修饰器；本项目为每种升级提供独立子类，固定目标属性，
 * 避免共用 Spec.Def 的不同升级被合并进同一栈。AggregateBySource 还会比较来源 ASC。
 * StackLimitCount=0 不限制层数；本类 Additive 量级按层数相乘。
 * 当同类每次传入相同 SetByCaller 数值时，N 层贡献为 N 倍该量级；
 * 合栈不会逐层保存不同购买数值，后续若引入可变幅度需重新设计。
 * 无周期 Infinite 修饰 CurrentValue 聚合器，移除后撤销贡献；Instant/周期执行通常修改基础值。
 * 最大生命/护甲提升时的等量回复由角色属性变化委托处理，不依赖执行回调。 */
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
//  22 个子类 —— 一种升级一个。
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

UCLASS()
class AI_API URogueUpgradeEffect_ScytheSize : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_ScytheSize();
};

UCLASS()
class AI_API URogueUpgradeEffect_ScytheLifesteal : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_ScytheLifesteal();
};

UCLASS()
class AI_API URogueUpgradeEffect_ProjectileBurst : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_ProjectileBurst();
};

UCLASS()
class AI_API URogueUpgradeEffect_RocketBlastRadius : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_RocketBlastRadius();
};

UCLASS()
class AI_API URogueUpgradeEffect_HellTowerDamageRamp : public URogueAttributeUpgradeEffect
{
	GENERATED_BODY()
public:
	URogueUpgradeEffect_HellTowerDamageRamp();
};

/** 升级枚举到效果类的映射；目标属性由对应子类配置。
 * 未映射的枚举返回 nullptr，调用方须报告并处理配置缺失。 */
namespace RogueUpgradeEffectMap
{
	AI_API TSubclassOf<UGameplayEffect> GetEffectClass(ERogueUpgradeType UpgradeType);
}
