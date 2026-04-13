#pragma once

#include "CoreMinimal.h"

class AActor;

namespace RogueCombatFeedback
{
	AI_API void AddDamageNumber(AActor* WorldContextActor, const FVector& WorldLocation, float Damage, bool bIsBossDamage);
}
