#pragma once

#include "CoreMinimal.h"

class AActor;

namespace RogueCombatFeedback
{
	AI_API void AddDamageNumber(AActor* WorldContextActor, const FVector& WorldLocation, float Damage, bool bIsBossDamage);

	/** 玩家回血飘字：路由到所有本地 HUD，与伤害飘字同一条绘制管线（绿色 + 号）。 */
	AI_API void AddHealingNumber(AActor* WorldContextActor, const FVector& WorldLocation, float Amount);
}
