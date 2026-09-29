#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RogueAttributeUpgradeEffect.generated.h"

/**
 * 通用的"属性类升级"效果（Instant）。
 *
 * 一份 GE 承载全部 8 种数值升级：
 *   · 改哪个属性 → Spec 上的动态资源 Tag `Upgrade.Attribute.*`
 *   · 改多少     → SetByCaller(Tag = Data.Magnitude)
 *   · 具体施加逻辑 → URogueUpgradeExecution
 *
 * 为什么是 **Instant** 而不是 Infinite：
 *   升级的效果是"永久变强"，Instant GE 会把结果写进属性的 BaseValue 后消失，
 *   既不需要持续占用一个激活中的 GE 槽位，也不必为每个升级维护一个持续效果。
 *   属性本身是复制的，所以客户端能通过属性同步看到升级结果。
 */
UCLASS()
class AI_API URogueAttributeUpgradeEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URogueAttributeUpgradeEffect();
};
