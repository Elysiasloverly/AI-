#include "Framework/RogueGameState.h"
#include "Enemies/RogueEnemy.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Cues/RogueGameplayCue_Impact.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Player/RoguePlayerState.h"
#include "Net/UnrealNetwork.h"

ARogueGameState::ARogueGameState()
{
	// 为低频局内显示设置复制候选频率；属性无变化不发送该字段的变化值，Actor仍可有其他网络流量。
	// UE 5.5 已弃用公开频率字段的直接访问，使用 setter 表达调度意图。
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(2.0f);
}

void ARogueGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// COND_None：整局状态所有客户端都要看，不是某个玩家私有
	DOREPLIFETIME(ARogueGameState, BossHealthView);
	DOREPLIFETIME(ARogueGameState, BattleStage);
	DOREPLIFETIME_CONDITION(ARogueGameState, CurrentWave, COND_None);
	DOREPLIFETIME_CONDITION(ARogueGameState, EnemiesDefeated, COND_None);
	DOREPLIFETIME_CONDITION(ARogueGameState, RunTimeSeconds, COND_None);
	DOREPLIFETIME_CONDITION(ARogueGameState, bGameOver, COND_None);
	DOREPLIFETIME_CONDITION(ARogueGameState, bBossAlive, COND_None);
}

void ARogueGameState::SetRunStateSnapshot(int32 InCurrentWave, int32 InEnemiesDefeated, float InRunTimeSeconds, bool bInGameOver, bool bInBossAlive)
{
	// 只有服务端有权威。客户端调用会被忽略，避免"客户端改波次"这类问题。
	if (!HasAuthority())
	{
		return;
	}

	CurrentWave = InCurrentWave;
	EnemiesDefeated = InEnemiesDefeated;
	RunTimeSeconds = InRunTimeSeconds;
	bGameOver = bInGameOver;
	bBossAlive = bInBossAlive;
}

UAbilitySystemComponent* ARogueGameState::ResolveCueSourceASC(AActor* SourceActor)
{
	if (!IsValid(SourceActor))
	{
		return nullptr;
	}

	// 1) 敌人：ASC 就挂在自己身上
	if (UAbilitySystemComponent* OwnComponent = SourceActor->FindComponentByClass<UAbilitySystemComponent>())
	{
		return OwnComponent;
	}

	// 2) 玩家：本项目的 ASC 在 PlayerState 上，角色只是承载者（AvatarActor）
	if (const APawn* Pawn = Cast<APawn>(SourceActor))
	{
		if (const ARoguePlayerState* PlayerState = Pawn->GetPlayerState<ARoguePlayerState>())
		{
			if (UAbilitySystemComponent* PlayerComponent = PlayerState->GetAbilitySystemComponent())
			{
				return PlayerComponent;
			}
		}
	}

	// 3) 通用兜底：走引擎标准查询（IAbilitySystemInterface / 组件）
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(SourceActor, /*LookForComponent=*/true);
}

void ARogueGameState::PlayImpactEffect(AActor* SourceActor, const FVector& Location, const FRotator& Rotation, ERogueImpactVisualStyle Style, const FVector& Scale)
{
	const FGameplayTag CueTag = RogueGameplayTags::GetImpactCueTag(Style);
	if (!CueTag.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* SourceASC = ResolveCueSourceASC(SourceActor))
	{
		FGameplayCueParameters Parameters;
		Parameters.Location = FVector_NetQuantize10(Location);
		Parameters.Normal = FVector_NetQuantizeNormal(Rotation.Vector());

		// 本项目用 RawMagnitude 传平面缩放、NormalizedMagnitude 传高度比例；不编码独立的 Y 缩放。
		// 这是项目约定，不是 FGameplayCueParameters 只能容纳一个浮点值。
		Parameters.RawMagnitude = Scale.X;
		Parameters.NormalizedMagnitude = Scale.X > KINDA_SMALL_NUMBER ? (Scale.Z / Scale.X) : 1.0f;

		Parameters.Instigator = SourceActor;
		Parameters.EffectCauser = SourceActor;
		Parameters.SourceObject = SourceActor;

		// 补注册原生 Cue 后交给 ASC 分发；接收和本地执行条件由 GAS 与网络路径决定。
		RogueGameplayCues::EnsureNativeImpactCuesRegistered();
		SourceASC->ExecuteGameplayCue(CueTag, Parameters);
		return;
	}

	// 兜底：解析不到 ASC（纯表现对象、没有 ASC 的测试场景）时只在本机生成。
	// 不静默丢弃 —— 否则排查时会把"没特效"误判成 tag 配错。
	ARogueImpactEffect::SpawnImpactEffect(
		IsValid(SourceActor) ? SourceActor->GetWorld() : nullptr,
		Location,
		Rotation,
		Style,
		Scale,
		0.18f,
		SourceActor);
}

void ARogueGameState::SetBattleStage(const FRogueBattleStageView& View)
{
    if (HasAuthority()) { BattleStage = View; }
}


void ARogueGameState::RefreshBossHealthView()
{
    if (!HasAuthority()) { return; }
    FRogueBossHealthView Next;
    const URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
    ARogueEnemy* Selected = nullptr;
    if (!bGameOver && Tracker)
    {
        for (const TWeakObjectPtr<ARogueEnemy>& Ref : Tracker->GetActiveEnemies())
        {
            ARogueEnemy* Enemy = Ref.Get();
            if (!IsValid(Enemy) || Enemy->IsAvailableInPool() || Enemy->IsDead() || !Enemy->IsBoss()
                || Enemy->GetCurrentHealth() <= 0 || Enemy->GetMaxHealth() <= 0) { continue; }
            if (!Selected) { Selected = Enemy; }
            // 多 Boss 时持续显示当前目标，直至死亡再换下一位，避免每帧跳条。
            if (Enemy->GetSpawnCycleId() == BossHealthView.SpawnId) { Selected = Enemy; break; }
        }
    }
    if (Selected)
    {
        Next.bVisible = true;
        Next.SpawnId = Selected->GetSpawnCycleId();
        Next.MaxHealth = Selected->GetMaxHealth();
        Next.CurrentHealth = FMath::Clamp(Selected->GetCurrentHealth(), 0.f, Next.MaxHealth);
    }
    const bool bVisibilityChanged = Next.bVisible != BossHealthView.bVisible || Next.SpawnId != BossHealthView.SpawnId;
    BossHealthView = Next;
    if (bVisibilityChanged) { ForceNetUpdate(); }
}
