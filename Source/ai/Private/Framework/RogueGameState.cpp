#include "Framework/RogueGameState.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Cues/RogueGameplayCue_Impact.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Player/RoguePlayerState.h"
#include "Net/UnrealNetwork.h"

ARogueGameState::ARogueGameState()
{
	// 存活时间每帧都在变。默认的 100Hz 复制频率对这种"客户端只需整秒显示"的数据是浪费，
	// 降到 10Hz；UE 仍然只在值真正变化时才发包，所以静止时不会有流量。
	//
	// 注意：NetUpdateFrequency 在 5.5 已标记 UE_DEPRECATED（Actor.h:831），
	// 直接对公有成员赋值只能编过但会告警 —— 必须用 SetNetUpdateFrequency()。
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(2.0f);
}

void ARogueGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// COND_None：整局状态所有客户端都要看，不是某个玩家私有
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

		// 形状信息的承载方式：RawMagnitude = 统一缩放，NormalizedMagnitude = Z 压扁比例。
		// cue 参数里能承载"形状"的自由浮点只有一个，而本项目实际用到的非等比缩放
		// 只有"贴地压扁"这一种（震荡柱地面波），压成一个比例既够用又省带宽。
		Parameters.RawMagnitude = Scale.X;
		Parameters.NormalizedMagnitude = Scale.X > KINDA_SMALL_NUMBER ? (Scale.Z / Scale.X) : 1.0f;

		Parameters.Instigator = SourceActor;
		Parameters.EffectCauser = SourceActor;
		Parameters.SourceObject = SourceActor;

		// 权威端调用：本机执行一次 + 多播给所有客户端（客户端各自本地生成特效）。
		// 这一行就是原来那条手写 Multicast 的全部替代品。
		//
		// 注册兜底：万一某台机器错过了启动期的注册（管理器回退、热重载…），
		// 这里再补一次 —— 幂等且只在"没注册上"时才真的做事。
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
