#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"

#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

// ===========================================================
//  公共基类：统一的"构造期授予标签"写法
// ===========================================================

URogueStateTagEffect::URogueStateTagEffect()
{
	// 构造期使用具名默认子对象；AddComponent 内部的 NewObject(NAME_None) 不适合此处。
	// GEComponents 持有组件；OnGameplayEffectChanged 会清空并从组件重建授予标签缓存。
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
}

void URogueStateTagEffect::GrantStateTag(FGameplayTag StateTag)
{
	if (!StateTag.IsValid())
	{
		return;
	}

	// 从 GEComponents 找到基类创建的标签组件；派生类可多次调用以添加不同状态标签。
	UTargetTagsGameplayEffectComponent* TargetTags = nullptr;
	for (UGameplayEffectComponent* Component : GEComponents)
	{
		if (UTargetTagsGameplayEffectComponent* AsTargetTags = Cast<UTargetTagsGameplayEffectComponent>(Component))
		{
			TargetTags = AsTargetTags;
			break;
		}
	}

	if (TargetTags == nullptr)
	{
		// 组件缺失时记录配置异常。C++ 派生类会自动执行基类构造，不会因漏写调用而跳过。
		UE_LOG(LogTemp, Error, TEXT("[Rogue] %s 缺少 TargetTags 组件：状态标签 %s 不会被授予"),
			*GetName(), *StateTag.ToString());
		return;
	}

	// 通过组件更新授予标签缓存，不依赖已弃用字段的旧资产升级逻辑。
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(StateTag);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}

// ===========================================================
//  死亡：State.Dead（无限时长，复活时显式移除）
// ===========================================================

URogueDeathEffect::URogueDeathEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	GrantStateTag(RogueGameplayTags::State_Dead);
}

// ===========================================================
//  刚受伤：State.RecentlyDamaged（时长 = ArmorRechargeDelay 属性）
// ===========================================================

URogueRecentlyDamagedEffect::URogueRecentlyDamagedEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 时长 = 持有者的 ArmorRechargeDelay 属性（受伤后要等多久护甲才开始回复）
	FAttributeBasedFloat DurationFromAttribute;
	DurationFromAttribute.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(
		URogueAttributeSet::GetArmorRechargeDelayAttribute(),
		EGameplayEffectAttributeCaptureSource::Target,
		/*bSnapshot=*/ true);
	DurationFromAttribute.AttributeCalculationType = EAttributeBasedFloatCalculationType::AttributeMagnitude;
	DurationFromAttribute.Coefficient = FScalableFloat(1.0f);

	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationFromAttribute);

	GrantStateTag(RogueGameplayTags::State_RecentlyDamaged);
}

// ===========================================================
//  冲刺冷却：Cooldown.Dash（时长 = DashCooldownDuration 属性）
// ===========================================================

URogueDashCooldownEffect::URogueDashCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 从目标快照捕获本次冷却时长，之后属性变化不改变这份已捕获的值。
	FAttributeBasedFloat CooldownFromAttribute;
	CooldownFromAttribute.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(
		URogueAttributeSet::GetDashCooldownDurationAttribute(),
		EGameplayEffectAttributeCaptureSource::Target,
		/*bSnapshot=*/ true);
	CooldownFromAttribute.AttributeCalculationType = EAttributeBasedFloatCalculationType::AttributeMagnitude;
	CooldownFromAttribute.Coefficient = FScalableFloat(1.0f);

	DurationMagnitude = FGameplayEffectModifierMagnitude(CooldownFromAttribute);

	// CheckCooldown 从冷却 GE 的授予标签判断 Cooldown.Dash 是否存在。
	GrantStateTag(RogueGameplayTags::Cooldown_Dash);
}

// ===========================================================
//  武器开火冷却：Cooldown.Weapon.AutoAttack（时长 = SetByCaller(Data.Cooldown)）
// ===========================================================

URogueWeaponCooldownEffect::URogueWeaponCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 各武器在施加时传入生效秒数；统一使用原生 Data.Cooldown Tag 作为键。
	// FName 与 Tag 的缺值行为都由 GetSetByCallerMagnitude 的告警/默认值参数决定。
	FSetByCallerFloat DurationFromCaller;
	DurationFromCaller.DataTag = RogueGameplayTags::Data_Cooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationFromCaller);

	GrantStateTag(RogueGameplayTags::Cooldown_WeaponAutoAttack);
}

// ===========================================================
//  敌人攻击冷却：Cooldown.Enemy.Attack（时长 = SetByCaller(Data.Cooldown)）
// ===========================================================

URogueEnemyAttackCooldownEffect::URogueEnemyAttackCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 与武器冷却同构：时长由施加方给出（三种攻击的冷却字段各不相同，见头文件说明）。
	FSetByCallerFloat DurationFromCaller;
	DurationFromCaller.DataTag = RogueGameplayTags::Data_Cooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationFromCaller);

	GrantStateTag(RogueGameplayTags::Cooldown_EnemyAttack);
}

// ===========================================================
//  濒死镜像：State.Downed（生命周期状态负责显式移除）
// ===========================================================

URogueDownedEffect::URogueDownedEffect()
{
	// GAS 只承载濒死标签和表现，期限由角色生命周期快照与服务端定时器管理。
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	GrantStateTag(RogueGameplayTags::State_Downed);
	FGameplayEffectCue DownedCue(RogueGameplayTags::GameplayCue_State_Downed, 0.0f, 1.0f);
	GameplayCues.Add(DownedCue);
}

// ===========================================================
//  复活无敌：State.Invulnerable（时长 = SetByCaller(Data.Duration)）
// ===========================================================

URogueInvulnerableEffect::URogueInvulnerableEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat DurationFromCaller;
	DurationFromCaller.DataTag = RogueGameplayTags::Data_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationFromCaller);

	GrantStateTag(RogueGameplayTags::State_Invulnerable);
}



// ===========================================================
//  自然回复：生命 / 护甲（Periodic GE，替代两个逐帧函数）
// ===========================================================

namespace
{
	/** 自然回复的结算周期（秒）。
	 *
	 *  写成具名常量是因为它要用在**两处**，而且两处必须一致：
	 *    · GE 的 `Period`（多久结算一次）
	 *    · 修饰器量级的换算系数（`速率 × Period` = 每周期增量）
	 *  两处不一致的话，回复总量会整体偏大或偏小，且不会有任何报错。 */
	constexpr float RegenPeriodSeconds = 0.25f;
}

URoguePeriodicRegenEffect::URoguePeriodicRegenEffect()
{
	// Infinite：回复是常驻状态，用标签条件开关，而不是"到期再重挂"。
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 正周期 0.25 秒：每秒四次结算。Period=0 表示无周期，不会逐帧执行。
	// Period 是 FScalableFloat；定义数值用 GetValue，Spec 中计算后的周期用 GetPeriod。
	Period = FScalableFloat(RegenPeriodSeconds);

	// 不在应用后额外安排一次立即执行，第一次回复等待周期计时器。
	bExecutePeriodicEffectOnApplication = false;
}

void URoguePeriodicRegenEffect::ConfigureRegen(FGameplayAttribute TargetAttribute, FGameplayAttribute RateAttribute)
{
	// 捕获目标速率并乘以周期。非快照捕获用于跟随后续速率属性变化；
	// 若改为快照，现有常驻效果将继续使用施加时捕获的速率。
	FAttributeBasedFloat RateFromAttribute;
	RateFromAttribute.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(
		RateAttribute,
		EGameplayEffectAttributeCaptureSource::Target,
		/*bSnapshot=*/ false);
	RateFromAttribute.AttributeCalculationType = EAttributeBasedFloatCalculationType::AttributeMagnitude;
	RateFromAttribute.Coefficient = FScalableFloat(RegenPeriodSeconds);

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = TargetAttribute;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(RateFromAttribute);
	Modifiers.Add(Modifier);

	// 上限钳制不在这里做：URogueAttributeSet::PreAttributeChange 已经把
	// Health 钳到 [0, MaxHealth]、Armor 钳到 [0, MaxArmor]，
	// 所以"到上限还继续加"会被静默钳掉，GE 侧不需要写"满了就停"。
}

void URoguePeriodicRegenEffect::BlockRegenWhileTagPresent(FGameplayTag BlockingTag)
{
	if (!BlockingTag.IsValid())
	{
		return;
	}

	// 与 URogueStateTagEffect 同一个教训（见本文件顶部）：
	// 构造期只能用 CreateDefaultSubobject 建组件，而且必须挂进 GEComponents，
	// 否则 PostLoad 之后条件就丢了。这里也**不要**用已废弃的裸成员
	// `UGameplayEffect::OngoingTagRequirements`。
	UTargetTagRequirementsGameplayEffectComponent* TagRequirements = nullptr;
	for (UGameplayEffectComponent* Component : GEComponents)
	{
		if (UTargetTagRequirementsGameplayEffectComponent* Existing =
			Cast<UTargetTagRequirementsGameplayEffectComponent>(Component))
		{
			TagRequirements = Existing;
			break;
		}
	}

	if (TagRequirements == nullptr)
	{
		TagRequirements = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("RegenTagRequirements"));
		GEComponents.Add(TagRequirements);
	}

	// IgnoreTags 的语义是"身上**不能**有这个标签"。
	TagRequirements->OngoingTagRequirements.IgnoreTags.AddTag(BlockingTag);
}

URogueHealthRegenEffect::URogueHealthRegenEffect()
{
	ConfigureRegen(
		URogueAttributeSet::GetHealthAttribute(),
		URogueAttributeSet::GetHealthRegenPerSecondAttribute());

	// State.Dead 期间抑制生命回复。
	BlockRegenWhileTagPresent(RogueGameplayTags::State_Dead);
}

URogueArmorRechargeEffect::URogueArmorRechargeEffect()
{
	ConfigureRegen(
		URogueAttributeSet::GetArmorAttribute(),
		URogueAttributeSet::GetArmorRechargeRateAttribute());

	BlockRegenWhileTagPresent(RogueGameplayTags::State_Dead);

	// "刚受伤"期间不回复。这个标签本身带 ArmorRechargeDelay 秒的时长
	// （见 URogueRecentlyDamagedEffect），所以两条合起来就是
	// "受伤后等 N 秒 → 标签消失 → 周期自动恢复"这条完整规则。
	BlockRegenWhileTagPresent(RogueGameplayTags::State_RecentlyDamaged);
}
