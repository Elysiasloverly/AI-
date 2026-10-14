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
	// ⚠️ 构造期只能用 CreateDefaultSubobject：UGameplayEffect::AddComponent<>() 内部是
	// `NewObject(this, NAME_None, ...)`（GameplayEffect.h:2429），在 UObject 构造函数里非法，
	// 会直接 Fatal（"NewObject with empty name can't be used to create default subobjects"）。
	//
	// 而且必须挂进 GEComponents：引擎的 OnGameplayEffectChanged() 会先 Reset CachedGrantedTags
	// 再按 GEComponents 重建（GameplayEffect.cpp:353-359），不挂进去的话标签在 PostLoad 之后就丢了。
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);
}

void URogueStateTagEffect::GrantStateTag(FGameplayTag StateTag)
{
	if (!StateTag.IsValid())
	{
		return;
	}

	// 从 GEComponents 里取回基类构造时建好的组件。
	// 为什么不存一个成员指针：组件已经由 GEComponents 持有（那是 UPROPERTY 数组），
	// 再存一个裸指针只会多一份"谁拥有它"的歧义，而这里每个类只在构造期调用一次。
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
		// 只可能发生在"派生类忘了先跑基类构造"这种编译期就写错的情况，留个兜底提醒。
		UE_LOG(LogTemp, Error, TEXT("[Rogue] %s 缺少 TargetTags 组件：状态标签 %s 不会被授予"),
			*GetName(), *StateTag.ToString());
		return;
	}

	// 注意：不是往 InheritableOwnedTagsContainer 里塞（5.3 起废弃，塞了也不会授予），
	// 而是交给 GE 组件去重建 CachedGrantedTags。
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

	// 时长 = 持有者的 DashCooldownDuration 属性值。
	// Target：GE 是施加到自己身上的，Source 与 Target 是同一个 ASC，用 Target 语义更贴切。
	// bSnapshot = true：施加瞬间把属性值快照下来当时长，否则中途属性变化会让剩余冷却漂移。
	FAttributeBasedFloat CooldownFromAttribute;
	CooldownFromAttribute.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(
		URogueAttributeSet::GetDashCooldownDurationAttribute(),
		EGameplayEffectAttributeCaptureSource::Target,
		/*bSnapshot=*/ true);
	CooldownFromAttribute.AttributeCalculationType = EAttributeBasedFloatCalculationType::AttributeMagnitude;
	CooldownFromAttribute.Coefficient = FScalableFloat(1.0f);

	DurationMagnitude = FGameplayEffectModifierMagnitude(CooldownFromAttribute);

	// 这一条是冲刺冷却能否生效的关键：能力的 GetCooldownTags() 读的就是冷却 GE 的授予标签。
	// 曾经因为标签没真的授予（废弃容器那个坑），冲刺**完全没有冷却**且没有任何报错。
	GrantStateTag(RogueGameplayTags::Cooldown_Dash);
}

// ===========================================================
//  武器开火冷却：Cooldown.Weapon.AutoAttack（时长 = SetByCaller(Data.Cooldown)）
// ===========================================================

URogueWeaponCooldownEffect::URogueWeaponCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 时长由施加方用 SetByCaller 给出（各武器的冷却算法不同，见武器侧的 GetAbilityCooldown）。
	// 用 SetByCaller 而不是绑属性：属性里存的是**加成**，不是"这把武器的冷却"，
	// "加成怎么换算成冷却"是武器的知识（火箭 ×2.5/下限 0.55、迫击炮 ×2.0/下限 0.7、激光 ×2.0/下限 0.35）。
	// UE 5.5 里 FSetByCallerFloat 有 DataName(FName) 与 DataTag(FGameplayTag) 两个字段，
	// 用 DataTag —— 施加方走 `Spec->SetByCallerTagMagnitudes`，走 Tag 不会有字符串拼写风险。
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

	// 每 0.25 秒结算一次。两个不选：
	//   · Period = 0（逐帧）—— 那正是迁移前的老路，等于换个地方写 Tick；
	//   · Period = 1（每秒）—— 护甲回复速率默认 45/秒，1 秒一跳会让护甲条明显一格一格跳。
	// 0.25 秒是观感与开销的平衡点（每秒 4 次结算，比"每帧约 60 次"少一个半数量级）。
	//
	// ⚠️ `UGameplayEffect::Period` 是 **FScalableFloat**，不是 float ——
	//    直接当 float 用（例如塞进 UE_LOG 的可变参数）会编译失败。
	//    要读数值用 `Period.GetValue()`；`FGameplayEffectSpec::GetPeriod()` 返回的才是 float。
	Period = FScalableFloat(RegenPeriodSeconds);

	// 施加当帧**不**立刻结算：这个 GE 在开局/复活时挂上，若立刻跳一次，
	// 初始生命与护甲会先超出初始值再被钳回来（观感上"满的"，语义上不该发生）。
	bExecutePeriodicEffectOnApplication = false;
}

void URoguePeriodicRegenEffect::ConfigureRegen(FGameplayAttribute TargetAttribute, FGameplayAttribute RateAttribute)
{
	// 量级 = 持有者的速率属性 × Period（把"每秒速率"换算成"每周期增量"）。
	//
	// ⚠️ bSnapshot 必须是 false：true 会在**施加瞬间**把速率快照下来，
	//    之后升级改 HealthRegenPerSecond / ArmorRechargeRate 就再也不生效 ——
	//    这是这个 GE 最容易踩、且完全没有报错的坑。
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

	// 死亡期间不回复。迁移前这条是 ARogueCharacter::Tick 里的 `if (bDead) return;` ——
	// 那时"死亡"是一个 C++ bool；现在是 State.Dead 标签，条件因此可以对 GE 声明。
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
