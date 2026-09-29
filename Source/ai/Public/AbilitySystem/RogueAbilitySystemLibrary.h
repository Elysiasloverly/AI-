#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FGameplayEffectSpecHandle;

/**
 * GAS 相关的通用工具。
 *
 * 存在的意义：把"怎么施加伤害"这件事收敛成**一个入口**。
 * 迁移前项目里有 **9 处** `UGameplayStatics::ApplyDamage(...)`（玩家侧 6 处 + 敌人侧 3 处）：
 *   玩家侧：普通子弹 / 火箭爆炸 / 激光主目标 / 激光折射 / 地狱塔 / 环绕刀片
 *   敌人侧：接触伤害 / 震荡柱地面波 / 敌人弹体
 * 每处都自己拼参数，于是"伤害来源"这个信息根本传不下去。
 * 现在统一成一次函数调用，伤害来源用 Tag 表达（Damage.Source.*），
 * 后续做伤害飘字样式、命中特效、免疫与减伤分流时都有据可依。
 */
namespace RogueAbilitySystem
{
	/** 对目标施加一次伤害。
	 *
	 *  @param SourceActor   伤害来源（武器 Actor / 敌人 / 弹体）。会写进 GE 上下文，
	 *                       用于击杀归属与伤害溯源。
	 *  @param TargetActor   受击者。**必须带 ASC**，否则本次伤害不会生效（返回 false）。
	 *  @param DamageAmount  伤害数值，写入 Spec 的 SetByCaller(Data.Damage)。
	 *  @param SourceTag     伤害来源 Tag（Damage.Source.*），可为空 Tag。
	 *  @return 是否成功施加。
	 *
	 *  服务端权威：非权威端调用会直接返回 false —— 伤害只在服务器结算，
	 *  这是联机正确性的前提（客户端只能"预测表现"，不能决定掉血）。
	 */
	AI_API bool ApplyDamageToTarget(AActor* SourceActor, AActor* TargetActor, float DamageAmount, FGameplayTag SourceTag);
}
