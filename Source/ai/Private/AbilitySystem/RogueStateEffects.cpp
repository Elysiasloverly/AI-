#include "AbilitySystem/RogueStateEffects.h"

#include "AbilitySystem/RogueAttributeSet.h"
#include "Core/RogueGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

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
