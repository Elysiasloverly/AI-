#include "AbilitySystem/RogueAttributeSet.h"

#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

namespace
{
	/** 属性钳制：把 NewValue 限制在 [Min, Max] */
	void ClampAttribute(float& NewValue, float Min, float Max)
	{
		NewValue = FMath::Clamp(NewValue, Min, Max);
	}
}

URogueAttributeSet::URogueAttributeSet()
{
	// 基础值由角色/敌人初始化流程写入；这里沿用 FGameplayAttributeData 的零默认值。
	// SetNumericAttributeBase 会替换基础值，构造默认值不会因此自动与新基础值相加。
}

void URogueAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 当前直接复制属性数据，HUD 轮询数值；尚未使用 RepNotify + GAMEPLAYATTRIBUTE_REPNOTIFY。
	// 该宏不仅用于表现通知，还将服务端基础值交回 ASC 聚合器处理预测校正；
	// 因此当前配置不能被视作已完善支持客户端属性预测的复制方案。
	DOREPLIFETIME_CONDITION(URogueAttributeSet, Health,               COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, MaxHealth,            COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, Armor,                COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, MaxArmor,             COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, HealthRegenPerSecond, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ArmorRechargeDelay,   COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ArmorRechargeRate,    COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, DamageReductionPercent, COND_None);

	// 武器加成发往所有相关客户端，供本地武器计算表现。
	DOREPLIFETIME_CONDITION(URogueAttributeSet, AttackPowerBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, AttackSpeedBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, AttackRangeBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ProjectileSpeedBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ProjectileCountBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ScytheCountBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, RocketCountBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, LaserCountBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, HellTowerCountBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, MortarCountBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, LaserRefractionBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, MortarBlastRadiusBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ScytheSizeBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ScytheLifestealBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ProjectileBurstBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, RocketBlastRadiusBonus, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, HellTowerDamageRampBonus, COND_None);

	DOREPLIFETIME_CONDITION(URogueAttributeSet, MoveSpeed,            COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, DashCooldownDuration, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, MinDashCooldown,      COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, DashDuration,         COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, DashSpeed,            COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ExperienceMultiplier, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, PickupRadius,         COND_None);

	// Damage 是元属性：不复制（它是服务端中转值，算完即清零）
}

float URogueAttributeSet::GetWeaponBonusCapFor(const FGameplayAttribute& Attribute) const
{
	// 所有字段默认 -1 = **不设上限**，所以"没在资产里配过"天然等于"不限制"，
	// 不需要额外的"是否已配置"标志。
	if (Attribute == GetAttackPowerBonusAttribute())       { return WeaponBonusCaps.AttackPower; }
	if (Attribute == GetAttackSpeedBonusAttribute())       { return WeaponBonusCaps.AttackSpeed; }
	if (Attribute == GetAttackRangeBonusAttribute())       { return WeaponBonusCaps.AttackRange; }
	if (Attribute == GetProjectileSpeedBonusAttribute())   { return WeaponBonusCaps.ProjectileSpeed; }
	if (Attribute == GetProjectileCountBonusAttribute())   { return WeaponBonusCaps.ProjectileCount; }
	if (Attribute == GetScytheCountBonusAttribute())       { return WeaponBonusCaps.ScytheCount; }
	if (Attribute == GetRocketCountBonusAttribute())       { return WeaponBonusCaps.RocketCount; }
	if (Attribute == GetLaserCountBonusAttribute())        { return WeaponBonusCaps.LaserCount; }
	if (Attribute == GetHellTowerCountBonusAttribute())    { return WeaponBonusCaps.HellTowerCount; }
	if (Attribute == GetLaserRefractionBonusAttribute())   { return WeaponBonusCaps.LaserRefraction; }
	if (Attribute == GetMortarCountBonusAttribute())       { return WeaponBonusCaps.MortarCount; }
	if (Attribute == GetMortarBlastRadiusBonusAttribute()) { return WeaponBonusCaps.MortarBlastRadius; }
	if (Attribute == GetProjectileBurstBonusAttribute()) { return WeaponBonusCaps.ProjectileBurst; }
	if (Attribute == GetRocketBlastRadiusBonusAttribute()) { return WeaponBonusCaps.RocketBlastRadius; }
	if (Attribute == GetHellTowerDamageRampBonusAttribute()) { return WeaponBonusCaps.HellTowerDamageRamp; }
	if (Attribute == GetScytheSizeBonusAttribute())        { return WeaponBonusCaps.ScytheSize; }
	if (Attribute == GetScytheLifestealBonusAttribute())   { return WeaponBonusCaps.ScytheLifesteal; }

	// 不是武器加成属性之一 → 返回 -1（不限制），
	// 这样调用方不需要自己判断"这个属性是否适用上限"。
	return -1.0f;
}

void URogueAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	// 加成先钳非负、再应用可选上限；数量类随后取整。
	// 最终生效数量还会由武器合并基础值和加成后计算。
	if (Attribute == GetAttackPowerBonusAttribute()
		|| Attribute == GetAttackSpeedBonusAttribute()
		|| Attribute == GetAttackRangeBonusAttribute()
		|| Attribute == GetProjectileSpeedBonusAttribute()
		|| Attribute == GetProjectileCountBonusAttribute()
		|| Attribute == GetScytheCountBonusAttribute()
		|| Attribute == GetRocketCountBonusAttribute()
		|| Attribute == GetLaserCountBonusAttribute()
		|| Attribute == GetHellTowerCountBonusAttribute()
		|| Attribute == GetMortarCountBonusAttribute()
		|| Attribute == GetLaserRefractionBonusAttribute()
		|| Attribute == GetMortarBlastRadiusBonusAttribute()
		|| Attribute == GetScytheSizeBonusAttribute()
		|| Attribute == GetProjectileBurstBonusAttribute()
		|| Attribute == GetRocketBlastRadiusBonusAttribute()
		|| Attribute == GetHellTowerDamageRampBonusAttribute()
		|| Attribute == GetScytheLifestealBonusAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);

		// ---- 上限（**资产配置，-1 = 不设上限**）----
		// 注意顺序：先夹下限再夹上限。上限只在配置了（>= 0）时才生效，
		// 所以默认（全 -1）行为与"没有上限"完全一致。
		const float Cap = GetWeaponBonusCapFor(Attribute);
		if (Cap >= 0.0f)
		{
			NewValue = FMath::Min(NewValue, Cap);
		}

		if (Attribute == GetProjectileCountBonusAttribute()
			|| Attribute == GetScytheCountBonusAttribute()
			|| Attribute == GetRocketCountBonusAttribute()
			|| Attribute == GetLaserCountBonusAttribute()
			|| Attribute == GetHellTowerCountBonusAttribute()
			|| Attribute == GetMortarCountBonusAttribute()
			|| Attribute == GetProjectileBurstBonusAttribute()
			|| Attribute == GetLaserRefractionBonusAttribute())
		{
			NewValue = FMath::RoundToFloat(NewValue);
		}

		// 吸血比例最多 100%，防止高等级叠加后回复量超过实际伤害。
		if (Attribute == GetScytheLifestealBonusAttribute())
		{
			NewValue = FMath::Min(NewValue, 1.0f);
		}
	}
	Super::PreAttributeChange(Attribute, NewValue);

	// 仅约束正在写入的 CurrentValue，不修改其他属性或原始聚合量级。
	// MaxHealth/MaxArmor 自己变化不会自动触发 Health/Armor 的再次写入；
	// 当前上限提高由角色委托补增量，上限降低后的当前值钳制需另行处理。

	if (Attribute == GetHealthAttribute())
	{
		ClampAttribute(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetArmorAttribute())
	{
		ClampAttribute(NewValue, 0.0f, GetMaxArmor());
	}
	else if (Attribute == GetDamageReductionPercentAttribute())
	{
		// 比例限制在 [0,1]；伤害结算处仍独立钳制防御性读取。
		ClampAttribute(NewValue, 0.0f, 1.0f);
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxArmorAttribute())
	{
		ClampAttribute(NewValue, 0.0f, TNumericLimits<float>::Max());
	}
	else if (Attribute == GetDashCooldownDurationAttribute())
	{
		// 下限来自 MinDashCooldown 属性（而不是写死 0.5），避免阈值两处分叉
		ClampAttribute(NewValue, GetMinDashCooldown(), TNumericLimits<float>::Max());
	}
	else if (Attribute == GetHealthRegenPerSecondAttribute()
		|| Attribute == GetArmorRechargeDelayAttribute()
		|| Attribute == GetArmorRechargeRateAttribute()
		|| Attribute == GetMoveSpeedAttribute()
		|| Attribute == GetDashDurationAttribute()
		|| Attribute == GetDashSpeedAttribute()
		|| Attribute == GetExperienceMultiplierAttribute()
		|| Attribute == GetPickupRadiusAttribute()
		|| Attribute == GetMinDashCooldownAttribute()
		|| Attribute == GetDamageAttribute())
	{
		// 这些量在语义上不能为负
		ClampAttribute(NewValue, 0.0f, TNumericLimits<float>::Max());
	}
}

void URogueAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 上限变化引起的等量回复由角色的 HandleMaxHealthChanged/HandleMaxArmorChanged 处理。
	// 本回调响应执行型修改，包括 Instant GE 与周期 GE 的每次执行；
	// 无周期 Infinite 升级只加入当前值聚合器，不通过此执行回调。

	if (Data.EvaluatedData.Attribute != GetDamageAttribute())
	{
		return;
	}

	// 记录击杀者：死亡 GE 自己不知道"谁打死的"，而死亡后的掉落/击杀计数需要它。
	LastDamageCauser = Data.EffectSpec.GetContext().GetEffectCauser();

	// 本项目常规 GAS 伤害在此结算。直接改 Health 的其他路径不会自动经过此伤害链。
	const float LocalDamage = GetDamage();
	SetDamage(0.0f);   // 元属性用完立刻清零，避免下一次结算重复计入

	if (LocalDamage <= 0.0f)
	{
		return;
	}

	const float MitigatedDamage = LocalDamage * (1.0f - FMath::Clamp(GetDamageReductionPercent(), 0.0f, 1.0f));
	float RemainingDamage = MitigatedDamage;

	if (RemainingDamage > 0.0f && GetArmor() > 0.0f)
	{
		const float ArmorAbsorbed = FMath::Min(GetArmor(), RemainingDamage);
		SetArmor(GetArmor() - ArmorAbsorbed);
		RemainingDamage -= ArmorAbsorbed;
	}

	if (RemainingDamage > 0.0f)
	{
		SetHealth(FMath::Max(0.0f, GetHealth() - RemainingDamage));
	}

	// 每次有效受伤重新施加一个带时长的 GE；该类未开启 Stacking，旧实例会独立到期。
	// 标签计数只在最后一个实例移除后归零，因而回复延迟至少覆盖最近一次受伤。
	if (MitigatedDamage > 0.0f)
	{
		ApplyRecentlyDamagedEffect();
	}

	// ---- 死亡：血量归零 → 给自己挂一个授予 State.Dead 的 GE ----
	// 不在这里直接调 Die()，理由见 URogueDeathEffect 的注释：
	// 这样死亡成为可复制的 GAS 状态，且 AttributeSet 不需要认识具体是玩家还是敌人。
	if (GetHealth() <= 0.0f)
	{
		TryApplyDeathEffect();
	}

	// State.RecentlyDamaged 阻断护甲回复 GE；生命周期切换由死亡标签监听者处理。
}

void URogueAttributeSet::ApplyRecentlyDamagedEffect()
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (ASC == nullptr || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	// 敌人没有护甲回复体系，不需要这个状态。
	// 目前以 MaxArmor 作为判据：没有护甲上限的单位就跳过，省一次 GE 施加。
	if (GetMaxArmor() <= 0.0f)
	{
		return;
	}

	// 不移除旧效果，直接新增延迟实例；最后一份标签到期后护甲回复才恢复。
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(GetOwningActor());

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(URogueRecentlyDamagedEffect::StaticClass(), 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void URogueAttributeSet::TryApplyDeathEffect()
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	// 同一帧内可能被多个来源打中并多次结算，已经死了就不再重复施加
	if (ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dead))
	{
		return;
	}

	// 本死亡效果只允许权威端施加；GAS 的其他效果可以在有效预测窗口中由客户端预测。
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(GetOwningActor());

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(URogueDeathEffect::StaticClass(), 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}
