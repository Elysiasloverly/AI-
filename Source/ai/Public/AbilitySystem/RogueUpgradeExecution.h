#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "RogueUpgradeExecution.generated.h"

/**
 * 升级效果的执行计算：把"改哪个属性"从 GE 资产里解耦出来。
 *
 * 背景问题：GE 的修饰器（Attribute + Op + Magnitude）是**烤在 CDO 上的**，
 * 一份 GE 只能改一个固定属性。项目有 8 种数值升级，如果照直做就是
 * 8 个 GE 类或 8 个 GE 蓝图资产 —— 而且每加一种属性都要再造一个。
 *
 * 解法：用 ExecutionCalculation 在**运行时**输出修饰器。
 *   · 载荷（改哪个属性）通过 Spec 的动态资源 Tag 传入：`Upgrade.Attribute.*`
 *   · 数值通过 SetByCaller(Tag = Data.Magnitude) 传入
 * 于是整个项目只需要 **一个** 升级 GE 类，属性映射表在 C++ 里维护、编译期就能查错。
 *
 * 为什么输出修饰器不需要把目标属性加进捕获列表：
 *   捕获（RelevantAttributesToCapture）只用于**读**输入值；
 *   输出用 `OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(...))`，
 *   目标属性是运行时决定的。这里只做"写"，所以不声明任何捕获。
 */
UCLASS()
class AI_API URogueUpgradeExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	URogueUpgradeExecution();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
