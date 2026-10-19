#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RogueDamageEffect.generated.h"

/** 瞬时伤害效果：以 SetByCaller(Data.Damage) 加到 Damage 元属性。
 * 属性集执行减伤、护甲、生命及状态联动。效果应用成功不等于一定扣血。
 * 共用 GE 类；每次攻击通过独立 Spec 传入本次数值。 */
UCLASS()
class AI_API URogueDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URogueDamageEffect();
};
