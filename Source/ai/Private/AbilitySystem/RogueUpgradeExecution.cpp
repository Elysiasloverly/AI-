#include "AbilitySystem/RogueUpgradeExecution.h"

#include "AbilitySystem/RogueAttributeSet.h"
#include "Core/RogueGameplayTags.h"
#include "GameplayEffect.h"

namespace
{
	/**
	 * 载荷 Tag → 目标属性 的映射表。
	 * 放在这里而不是散在 switch 里，是为了让"支持哪些属性升级"一眼可见；
	 * 加新属性时只要在这里加一行 + 在 RogueGameplayTags 里加一个 Tag。
	 */
	struct FUpgradeAttributeMapping
	{
		FGameplayTag Tag;
		FGameplayAttribute Attribute;
	};

	const TArray<FUpgradeAttributeMapping>& GetUpgradeAttributeMappings()
	{
		static const TArray<FUpgradeAttributeMapping> Mappings =
		{
			{ RogueGameplayTags::Upgrade_Attribute_MaxHealth,       URogueAttributeSet::GetMaxHealthAttribute() },
			{ RogueGameplayTags::Upgrade_Attribute_MoveSpeed,       URogueAttributeSet::GetMoveSpeedAttribute() },
			{ RogueGameplayTags::Upgrade_Attribute_MaxArmor,        URogueAttributeSet::GetMaxArmorAttribute() },
			{ RogueGameplayTags::Upgrade_Attribute_DashCooldown,    URogueAttributeSet::GetDashCooldownDurationAttribute() },
			{ RogueGameplayTags::Upgrade_Attribute_PickupRadius,    URogueAttributeSet::GetPickupRadiusAttribute() },
			{ RogueGameplayTags::Upgrade_Attribute_HealthRegen,     URogueAttributeSet::GetHealthRegenPerSecondAttribute() },
			{ RogueGameplayTags::Upgrade_Attribute_DamageReduction, URogueAttributeSet::GetDamageReductionPercentAttribute() },
			{ RogueGameplayTags::Upgrade_Attribute_ExperienceGain,  URogueAttributeSet::GetExperienceMultiplierAttribute() },
		};
		return Mappings;
	}

	/** 在映射表里按 Tag 找目标属性；找不到返回无效属性 */
	FGameplayAttribute FindUpgradeAttribute(const FGameplayTagContainer& PayloadTags)
	{
		for (const FUpgradeAttributeMapping& Mapping : GetUpgradeAttributeMappings())
		{
			if (PayloadTags.HasTag(Mapping.Tag))
			{
				return Mapping.Attribute;
			}
		}
		return FGameplayAttribute();
	}
}

URogueUpgradeExecution::URogueUpgradeExecution()
{
	// 只做"写"，不读任何属性，所以不声明捕获定义。
	// 如果将来需要"按当前值做上限判断"，再在这里加 RelevantAttributesToCapture。
}

void URogueUpgradeExecution::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// 载荷：这次升级要改哪个属性
	const FGameplayAttribute TargetAttribute = FindUpgradeAttribute(Spec.GetDynamicAssetTags());
	if (!TargetAttribute.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[RogueUpgradeExecution] 升级 GE 没有携带有效载荷 Tag（Upgrade.Attribute.*），本次升级被忽略。"));
		return;
	}

	// 数值：由调用方通过 SetByCaller 写入。
	// 注意"冲刺冷却"这类是**负向**升级（减少冷却），调用方传负数即可，
	// 不需要在 Execution 里硬编码符号。
	const float Magnitude = Spec.GetSetByCallerMagnitude(RogueGameplayTags::Data_Magnitude, /*WarnIfNotFound=*/false, 0.0f);
	if (FMath::IsNearlyZero(Magnitude))
	{
		return;
	}

	// Additive：属性当前值 + Magnitude。
	// 对 Instant GE 来说，这会把结果写进属性的 **BaseValue**（永久生效），
	// 正是"一局之内永久变强"需要的语义。
	OutExecutionOutput.AddOutputModifier(
		FGameplayModifierEvaluatedData(TargetAttribute, EGameplayModOp::Additive, Magnitude));
}
