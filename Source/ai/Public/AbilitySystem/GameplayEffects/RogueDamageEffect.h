#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RogueDamageEffect.generated.h"

/**
 * 通用伤害效果。
 *
 * 设计要点：
 *   1) **Instant**：伤害是一次性结算，不该像 Buff 那样留在身上。
 *   2) 改的是**元属性 `Damage`**，不是直接改 `Health`。
 *      因为项目有一条真实的减伤链（减伤比例 → 护甲吸收 → 剩余扣血），
 *      这条链写在 `URogueAttributeSet::PostGameplayEffectExecute` 里。
 *      如果 GE 直接改 Health，那条链就没有落脚点，而且"扣护甲"这种副作用
 *      会让 GE 的语义变得含糊。
 *   3) 伤害量走 **SetByCaller（Tag = Data.Damage）**：
 *      一份 GE 资产被全部 9 个伤害调用点复用（玩家侧：普通子弹 / 火箭爆炸 /
 *      激光主目标 / 激光折射 / 地狱塔 / 环绕刀片；敌人侧：接触伤害 /
 *      震荡柱地面波 / 敌人弹体），调用方只需在 Spec 上写一个数字。
 *      这就是"数据驱动"和"每个武器一个 GE 资产"的区别。
 */
UCLASS()
class AI_API URogueDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URogueDamageEffect();
};
