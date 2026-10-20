#include "AbilitySystem/RogueAbilitySystemLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/GameplayEffects/RogueDamageEffect.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Player/RogueCharacter.h"

bool RogueAbilitySystem::ApplyDamageToTarget(AActor* SourceActor, AActor* TargetActor, float DamageAmount, FGameplayTag SourceTag, float* OutDamageDealt)
{
	if (OutDamageDealt != nullptr)
	{
		*OutDamageDealt = 0.0f;
	}

	if (DamageAmount <= 0.0f || !IsValid(TargetActor))
	{
		return false;
	}

	// 玩家生命状态是权威判据，不能依赖死亡 GE 是否已经施加或清理完毕。
	const ARogueCharacter* TargetCharacter = Cast<ARogueCharacter>(TargetActor);
	if (TargetCharacter != nullptr && !TargetCharacter->IsAlive())
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

	// 死亡、濒死、无敌提前拒绝，避免创建不需要的 Spec。
	// GE 标签要求在 UE 5.5 可用 TargetTagRequirements 组件配置；这里保留显式业务检查。
	if (TargetASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dead)
		|| TargetASC->HasMatchingGameplayTag(RogueGameplayTags::State_Downed)
		|| TargetASC->HasMatchingGameplayTag(RogueGameplayTags::State_Invulnerable))
	{
		return false;
	}

	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddSourceObject(SourceActor);

	// Context 初始来自目标 ASC。仅 Pawn 来源在这里覆盖 Instigator/EffectCauser；
	// 武器/弹体等非 Pawn 只写入了 SourceObject，EffectCauser 仍可能是目标 Avatar。
	// 因此当前上下文还不能为所有来源提供可靠的击杀归属。
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

	// 通过访问器添加动态资产标签；它们描述此次 Spec，不等于授予目标的状态标签。
	if (SourceTag.IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(SourceTag);
	}

	const URogueAttributeSet* TargetAttributes = TargetASC->GetSet<URogueAttributeSet>();
	const float BeforeTotal = TargetAttributes != nullptr
		? TargetAttributes->GetHealth() + TargetAttributes->GetArmor()
		: 0.0f;

	const bool bApplied = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()).WasSuccessfullyApplied();

	if (OutDamageDealt != nullptr && TargetAttributes != nullptr)
	{
		const float AfterTotal = TargetAttributes->GetHealth() + TargetAttributes->GetArmor();
		*OutDamageDealt = bApplied ? FMath::Max(0.0f, BeforeTotal - AfterTotal) : 0.0f;
	}

	return bApplied;
}
