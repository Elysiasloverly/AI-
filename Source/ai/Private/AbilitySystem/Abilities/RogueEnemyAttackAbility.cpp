#include "AbilitySystem/Abilities/RogueEnemyAttackAbility.h"

#include "AbilitySystem/Tasks/RogueAbilityTask_AttackTick.h"
#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Enemies/RogueEnemy.h"

namespace RogueEnemyAttackStats
{
	// 文件内静态：进程级计数（说明见头文件）。
	static FCounters GCounters;

	const FCounters& Get()
	{
		return GCounters;
	}

	void Reset()
	{
		GCounters = FCounters();
	}

	void CountContactAttempt()
	{
		++GCounters.ContactAttempts;
	}

	void CountRangedAttempt()
	{
		++GCounters.RangedAttempts;
	}

	void CountGroundWaveAttempt()
	{
		++GCounters.GroundWaveAttempts;
	}
}

// ===========================================================
//  基类
// ===========================================================

URogueEnemyAttackAbility::URogueEnemyAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 攻击能力仅服务端执行，客户端通过弹体/波状态等表现通道获知结果。
	// ServerOnly 本身并不禁止所有情况下复制能力 Spec 或创建客户端实例。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// 已激活期间不重触发；瞬时能力结束后的下一次尝试仍由 AI 节流条件控制。
	bRetriggerInstancedAbility = false;

	FGameplayTagContainer AttackTags;
	AttackTags.AddTag(RogueGameplayTags::Ability_Enemy_Attack);
	SetAssetTags(AttackTags);
}

void URogueEnemyAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (GetEnemy() == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/false, /*bWasCancelled=*/false);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

ARogueEnemy* URogueEnemyAttackAbility::GetEnemy() const
{
	return Cast<ARogueEnemy>(GetAvatarActorFromActorInfo());
}

void URogueEnemyAttackAbility::ApplyAttackCooldown()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC == nullptr)
	{
		return;
	}

	const float Seconds = GetAttackCooldownSeconds();
	if (Seconds <= 0.0f)
	{
		return;
	}

	const TSubclassOf<UGameplayEffect> CooldownClass = URogueEnemyAttackCooldownEffect::StaticClass();

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(CooldownClass, GetAbilityLevel(), Context);
	if (!SpecHandle.IsValid() || SpecHandle.Data == nullptr)
	{
		return;
	}

	// 时长走 SetByCaller：三种攻击的冷却字段各不相同，GE 只负责按秒数计时。
	SpecHandle.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Cooldown, Seconds);

	// 当前 ServerOnly，权威端无需有效预测键即可施加效果。
	// 这里携带激活键不代表支持客户端持续预测；若改执行策略需重新设计预测窗口与确认。
	const FPredictionKey ActivationPredictionKey = CurrentActivationInfo.GetActivationPredictionKey();
	const FActiveGameplayEffectHandle NewHandle =
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get(), ActivationPredictionKey);

	if (!CooldownClass || !NewHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[敌人技能] %s 的冷却 GE 没有生效（句柄无效）：敌人=%s 时长=%.2f"),
			*GetClass()->GetName(), *GetNameSafe(GetEnemy()), Seconds);
	}
}

void URogueEnemyAttackAbility::StartTickTask()
{
	TickTask = URogueAbilityTask_AttackTick::RunAttackTick(this);
	if (TickTask != nullptr)
	{
		TickTask->ReadyForActivation();
	}
}

// ===========================================================
//  接触攻击
// ===========================================================

void URogueEnemyContactAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ARogueEnemy* Enemy = GetEnemy();
	if (Enemy != nullptr)
	{
		Enemy->ExecuteContactAttack();
		++RogueEnemyAttackStats::GCounters.Contact;
	}

	// 在能力结束前记录攻击冷却。GE 可独立于能力实例继续存在，
	// 但当前没有将它接入 CheckCooldown；实际激活节流由敌人计时器负责。
	ApplyAttackCooldown();

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/false, /*bWasCancelled=*/false);
}

float URogueEnemyContactAttackAbility::GetAttackCooldownSeconds() const
{
	const ARogueEnemy* Enemy = GetEnemy();
	// 与改造前 TouchPlayer 里 `ContactTimer = ContactInterval` 同一个数值来源。
	return Enemy != nullptr ? Enemy->GetContactInterval() : 0.0f;
}

// ===========================================================
//  远程攻击
// ===========================================================

void URogueEnemyRangedAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ARogueEnemy* Enemy = GetEnemy();
	if (Enemy != nullptr && Enemy->ExecuteRangedAttack())
	{
		++RogueEnemyAttackStats::GCounters.Ranged;
		ApplyAttackCooldown();
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/false, /*bWasCancelled=*/false);
}

float URogueEnemyRangedAttackAbility::GetAttackCooldownSeconds() const
{
	const ARogueEnemy* Enemy = GetEnemy();
	// 改造前是 `RangedAttackTimer = RangedArchetype.NextShotCooldown`。
	return Enemy != nullptr ? Enemy->GetRangedArchetype().NextShotCooldown : 0.0f;
}

// ===========================================================
//  地面波（持续到波结束）
// ===========================================================

void URogueEnemyGroundWaveAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ARogueEnemy* Enemy = GetEnemy();
	if (Enemy == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/false, /*bWasCancelled=*/false);
		return;
	}

	Enemy->ExecuteStartGroundWave();
	++RogueEnemyAttackStats::GCounters.GroundWave;

	// 波一旦开始就由本能力逐帧推进，直到 Enemy 报告"波结束了"（见 TickAttackStep）。
	// 注意这里**不**结束能力：这正是"持续激活"的含义。
	StartTickTask();
}

void URogueEnemyGroundWaveAttackAbility::TickAttackStep(float DeltaSeconds)
{
	ARogueEnemy* Enemy = GetEnemy();
	if (Enemy == nullptr)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
		return;
	}

	Enemy->ExecuteAdvanceGroundWave(DeltaSeconds);

	// 波结束后记录 GE 冷却并结束能力；Enemy 侧仍负责下一次攻击的 AI 节流计时。
	if (!Enemy->IsGroundWaveActive())
	{
		ApplyAttackCooldown();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
	}
}

float URogueEnemyGroundWaveAttackAbility::GetAttackCooldownSeconds() const
{
	const ARogueEnemy* Enemy = GetEnemy();
	// 改造前是波结束时 `GroundWaveAttackTimer = GroundWave.AttackCooldown`。
	return Enemy != nullptr ? Enemy->GetCurrentArchetype().GroundWave.AttackCooldown : 0.0f;
}
