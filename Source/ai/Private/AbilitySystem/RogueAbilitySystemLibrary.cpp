#include "AbilitySystem/RogueAbilitySystemLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/RogueDamageEffect.h"
#include "Core/RogueGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"

bool RogueAbilitySystem::ApplyDamageToTarget(AActor* SourceActor, AActor* TargetActor, float DamageAmount, FGameplayTag SourceTag)
{
	if (DamageAmount <= 0.0f || !IsValid(TargetActor))
	{
		return false;
	}

	// GetAbilitySystemComponentFromActor 会走 IAbilitySystemInterface，
	// 所以玩家（ASC 在 PlayerState 上）和敌人（ASC 在自己身上）都能取到。
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::Get().GetAbilitySystemComponentFromActor(TargetActor);
	if (TargetASC == nullptr)
	{
		return false;
	}

	// 服务端权威。注意用的是 IsOwnerActorAuthoritative() 而不是 Actor 上的 HasAuthority()：
	// 玩家的 ASC 挂在 PlayerState 上，权威性由 OwnerActor 决定。
	if (!TargetASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	// 死了 / 无敌 → 伤害不生效。
	// 刻意用代码判定而不是 GE 的 ApplicationTagRequirements：
	//   · 后者在本引擎版本已被标记 DeprecatedProperty（GameplayEffect.h:2307）；
	//   · 代码判定还能省掉一次 Spec 构造 —— 在 200+ 敌人的场景里这有实际意义。
	if (TargetASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(RogueGameplayTags::State_Invulnerable))
	{
		return false;
	}

	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddSourceObject(SourceActor);

	// Instigator 与 Causer 分开传：击杀归属用它，避免"谁打的"这个信息丢失。
	// 旧实现里 7 处调用的这两个参数经常传 nullptr，导致归因信息不可靠。
	if (APawn* InstigatorPawn = Cast<APawn>(SourceActor))
	{
		Context.AddInstigator(InstigatorPawn, SourceActor);
	}

	FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(URogueDamageEffect::StaticClass(), 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Damage, DamageAmount);

	// 伤害来源写成动态资源 Tag。
	// 注意：DynamicAssetTags 成员自 5.0 起已废弃，要用 AddDynamicAssetTag / GetDynamicAssetTags。
	if (SourceTag.IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(SourceTag);
	}

	return TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()).WasSuccessfullyApplied();
}
