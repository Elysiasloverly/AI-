#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FGameplayEffectSpecHandle;

/** 服务端 GAS 伤害入口：统一创建伤害 Spec，并携带来源标签。
 * ApplyDamageToTarget 不调用 AActor::TakeDamage，也不自动桥接 Actor 的伤害事件。 */
namespace RogueAbilitySystem
{
	/** 对具备 ASC 的有效目标施加正数伤害；仅在目标 ASC 权威端执行。
	 * @param SourceActor 写入 SourceObject；为 APawn 时额外覆盖 Instigator/EffectCauser。
	 *                    当前非 Pawn 来源不会覆盖默认 EffectCauser，不能依此保证击杀归属。
	 * @param TargetActor 目标需存活，且无死亡、濒死、无敌标签。
	 * @param DamageAmount 写入 SetByCaller(Data.Damage)，由属性集执行减伤/护甲/扣血。
	 * @param SourceTag 可选 Damage.Source.*，写入动态资产标签；标签本身不实现免疫或减伤。
	 * @return GE 应用是否成功；不表示一定扣除了生命（可能全部由护甲/减伤吸收）。
	 * 此路径未检查 CanBeDamaged，也不广播 OnTakeAnyDamage。 */
	AI_API bool ApplyDamageToTarget(AActor* SourceActor, AActor* TargetActor, float DamageAmount, FGameplayTag SourceTag);
}
