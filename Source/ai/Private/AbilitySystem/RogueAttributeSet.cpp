#include "AbilitySystem/RogueAttributeSet.h"

#include "AbilitySystem/RogueStateEffects.h"
#include "Core/RogueGameplayTags.h"
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
	// 全部属性的"零值"就是 0。
	// GAS 里绝对值的唯一来源是 SetNumericAttributeBase（由玩家平衡资产灌入），
	// 所以这里不需要、也不应该写任何默认数值 —— 写了就会和基础值写入叠加。
}

void URogueAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 说明：这里先用朴素的 Replicated（客户端只同步数值，没有 OnRep 回调），
	// 因为 HUD 是每帧轮询属性来画血条/护甲的。
	// 等需要"客户端收到伤害时做受击反馈/屏幕血渍"时，再把对应属性改成
	// ReplicatedUsing=OnRep_Xxx 并在 RepNotify 里用 GAMEPLAYATTRIBUTE_REPNOTIFY 分发。
	DOREPLIFETIME_CONDITION(URogueAttributeSet, Health,               COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, MaxHealth,            COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, Armor,                COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, MaxArmor,             COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, HealthRegenPerSecond, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ArmorRechargeDelay,   COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ArmorRechargeRate,    COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, DamageReductionPercent, COND_None);

	// 武器加成（升级落点）。与其它属性一样 COND_None：数值要发给所有端，
	// 客户端的武器也要按同样的数值算表现（见头文件里"为什么并入主属性集"的说明）。
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
	DOREPLIFETIME_CONDITION(URogueAttributeSet, MoveSpeed,            COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, DashCooldownDuration, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, MinDashCooldown,      COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, DashDuration,         COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, DashSpeed,            COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, ExperienceMultiplier, COND_None);
	DOREPLIFETIME_CONDITION(URogueAttributeSet, PickupRadius,         COND_None);

	// Damage 是元属性：不复制（它是服务端中转值，算完即清零）
}

void URogueAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	// 武器加成：全部不允许为负（负数量会让武器彻底不工作、负范围会让索敌失效）。
	// 数量类只做取整，最终数量由武器侧"数据表基础值 + 加成"合并后再取整。
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
		|| Attribute == GetMortarBlastRadiusBonusAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);

		if (Attribute == GetProjectileCountBonusAttribute()
			|| Attribute == GetScytheCountBonusAttribute()
			|| Attribute == GetRocketCountBonusAttribute()
			|| Attribute == GetLaserCountBonusAttribute()
			|| Attribute == GetHellTowerCountBonusAttribute()
			|| Attribute == GetMortarCountBonusAttribute()
			|| Attribute == GetLaserRefractionBonusAttribute())
		{
			NewValue = FMath::RoundToFloat(NewValue);
		}
	}
	Super::PreAttributeChange(Attribute, NewValue);

	// PreAttributeChange 只负责"钳制"，不做任何游戏逻辑（引擎文档明确不建议在这里改别的属性）。
	// 注意它用的是属性的**当前值**做上界：MaxHealth 自己的修正走到这里时，
	// Health 还没有被重新钳制 —— 所以 MaxHealth 变化后需要在 PostGameplayEffectExecute
	// 或属性变化回调里再钳一次 Health。

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
		// 关键钳制：这是比例（0~1），不是百分数。
		// 不钳的话升级能把它堆过 1，`(1 - x)` 变负会让玩家免疫全部伤害。
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

	// MaxHealth / MaxArmor 被提升后，当前值要跟着"补满那部分增量"。
	// 语义沿用自研系统：升级最大生命时是
	//     CurrentHealth += (MaxHealth - PreviousMaxHealth);
	// 也就是"上限涨多少就回多少血"，而不是按比例或直接补满。
	//
	// 这里用 Data.EvaluatedData.Magnitude 作为增量。**前提是该 GE 用 Additive 修饰器** ——
	// 若将来有人把上限改写成 Override / Multiply，这里的增量语义就不成立了，
	// 那时应该改成"记录旧上限再取差值"。
	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		const float MaxHealthDelta = Data.EvaluatedData.Magnitude;
		if (MaxHealthDelta > 0.0f)
		{
			SetHealth(GetHealth() + MaxHealthDelta);
		}
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetMaxArmorAttribute())
	{
		const float MaxArmorDelta = Data.EvaluatedData.Magnitude;
		if (MaxArmorDelta > 0.0f)
		{
			SetArmor(GetArmor() + MaxArmorDelta);
		}
		SetArmor(FMath::Clamp(GetArmor(), 0.0f, GetMaxArmor()));
		return;
	}

	if (Data.EvaluatedData.Attribute != GetDamageAttribute())
	{
		return;
	}

	// 记录击杀者：死亡 GE 自己不知道"谁打死的"，而死亡后的掉落/击杀计数需要它。
	LastDamageCauser = Data.EffectSpec.GetContext().GetEffectCauser();

	// ---- 伤害结算链：减伤比例 → 护甲吸收 → 剩余扣血 ----
	// 这是全项目唯一的伤害落地处（所有伤害都经 ApplyDamageToTarget → 这个元属性进来）。
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

	// ---- 受伤后进入"护甲回复延迟"状态 ----
	// 用带时长的 GE 授予 State.RecentlyDamaged —— 状态可查询、可复制，
	// 护甲回复逻辑只需要问一句"身上有没有这个 Tag"。
	// （自研系统里这是一个私有 float 计时器 `ArmorRechargeDelayRemaining`，迁移时已删除。）
	// 重复受伤时重挂一次即可（GAS 会刷新持续时间）。
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

	// 受伤后的另一件事（护甲回复计时重置）由 `State.RecentlyDamaged` 承担，
	// 具体在角色/敌人转换那一步接上。
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

	// 已经在延迟中就重挂一次（刷新时长），不必先移除
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

	// 只有服务端能施加 GE
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
