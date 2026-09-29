#include "AbilitySystem/Abilities/RogueEnemyAttackAbility.h"

#include "AbilitySystem/Abilities/RogueAbilityTask_AttackTick.h"
#include "AbilitySystem/RogueStateEffects.h"
#include "AbilitySystemComponent.h"
#include "Core/RogueGameplayTags.h"
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

	// 敌人没有"本人客户端"：攻击表现走已有的复制通道（弹体 Multicast / 地面波复制半径），
	// 能力只需要服务端那一份。副作用很划算：客户端上敌人**一个技能实例都没有**。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// 一次性攻击不允许重入（否则同一帧能连打两发）。
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

	// 显式带上这次激活的预测键 —— 与武器冷却同一条踩坑教训（见 RogueWeaponAbility::StampWeaponCooldown）：
	// 引擎要求"非权威端施加 GE 必须带有效预测键"，不带就会静默拿到空句柄。
	// 敌人是 ServerOnly + 服务端权威，这里本来就通过；写成同一个形状是为了
	// 将来万一有客户端侧的攻击能力时不会再踩一遍。
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

	// 冷却要在结束前给：EndAbility 之后实例就"没在跑"了，但 GE 与能力实例无关，
	// 什么时候给都可以；放在这里是让"打完之后进入冷却"这件事一眼可见。
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

	// 波结束 → 能力结束并进入冷却。
	// 改造前这段是 AI 里的 `if (bGroundWaveActive) UpdateGroundWave()` +
	// 波结束时的 `GroundWaveAttackTimer = GroundWave.AttackCooldown`，
	// 现在归能力所有：AI 只需要知道"我要不要起一波"。
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
