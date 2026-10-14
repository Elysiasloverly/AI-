#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "RogueGameState.generated.h"

/**
 * 整局运行状态（服务端权威 + 复制到所有客户端）。
 *
 * **为什么必须存在这个类** —— 这是把单机项目改成联机时最典型的一处结构性修正：
 *   原来这些数据全在 `ARogueGameMode::RunState` 上，而 **GameMode 是服务端独占对象**，
 *   客户端调 `GetWorld()->GetAuthGameMode()` 得到 nullptr。
 *   于是客户端 HUD 画不出波次、击杀数、存活时间，结算界面也是空的 —— 不是"少写了个 RPC"，
 *   而是**状态放错了容器**。
 *
 * 分工原则（联机下应当始终按这条线切）：
 *   · 只有服务端需要的逻辑与算法   → GameMode
 *   · 整局共享、客户端也要看的状态 → GameState（本类）
 *   · 每个玩家自己的状态           → PlayerState
 *
 * 复制频率：存活时间每帧都在变，若按默认频率复制会白白占带宽。
 * 这里把 NetUpdateFrequency 降到 10Hz —— HUD 显示的是整秒，10Hz 完全够，
 * 而且 UE 只在属性真正变化时才发，静止帧不会产生流量。
 */
UCLASS()
class AI_API ARogueGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARogueGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Rogue|Run")
	int32 GetCurrentWave() const { return CurrentWave; }

	UFUNCTION(BlueprintPure, Category = "Rogue|Run")
	int32 GetEnemiesDefeated() const { return EnemiesDefeated; }

	UFUNCTION(BlueprintPure, Category = "Rogue|Run")
	float GetRunTimeSeconds() const { return RunTimeSeconds; }

	UFUNCTION(BlueprintPure, Category = "Rogue|Run")
	bool IsGameOver() const { return bGameOver; }

	UFUNCTION(BlueprintPure, Category = "Rogue|Run")
	bool IsBossAlive() const { return bBossAlive; }

	/** 服务端每帧写入快照（由 GameMode 调用）。
	 *  一次性写全部字段而不是提供分开的 setter：状态是同一帧的一致快照，
	 *  分开写会让客户端可能读到"波次已更新、击杀数还没"的中间态。 */
	void SetRunStateSnapshot(int32 InCurrentWave, int32 InEnemiesDefeated, float InRunTimeSeconds, bool bInGameOver, bool bInBossAlive);

	// ===========================================================
	//  表现层广播（"服务端生成、所有客户端都该看到"的特效）
	// ===========================================================
	//  为什么需要：敌人 AI 现在只在服务端跑（见 ARogueEnemy::Tick），
	//  于是它产生的枪口特效、地面波起始特效在客户端完全不会出现。
	//
	//  ---- 打击/开火特效：走 GAS 的 GameplayCue ----
	//
	//  历史：这里原来是一个手写的 `Multicast_PlayImpactEffect`（GameState 天然复制到所有客户端，
	//  是"世界级广播"的现成载体）。迁移到 GameplayCue 之后它被删掉了，原因：
	//    · cue 的多播、相关性过滤、预测去重都由 AbilitySystem 负责，
	//      以后每加一种特效不用再加一条 RPC 和一个枚举值；
	//    · 敌人 ASC 用的是 Minimal 复制模式，而 **Minimal 模式天生就复制 GameplayCue**
	//      （这正是它的设计目的：只同步"属性 + 表现事件"）；
	//    · 特效和"谁造成的"绑在一起（Instigator / EffectCauser），
	//      击杀反馈、命中分流这类需求以后直接从参数里读。

	/** 服务端调用：用 GameplayCue 播一次打击/开火特效。
	 *
	 *  只有权威端需要调用 —— cue 的多播由 ASC 负责，客户端会各自收到并在本地生成。
	 *  服务端自己也会执行一次（多播在服务端同样执行），所以调用方**不要**再自己调
	 *  ARogueImpactEffect::SpawnImpactEffect，否则服务端双份。
	 *
	 *  SourceActor 是"这次特效是谁造成的"，必须能解析到 ASC：
	 *    · 敌人        → 自己身上的 ASC
	 *    · 玩家角色     → PlayerState 上的 ASC（本项目的形态）
	 *  解析不到 ASC 时退回"只在本机生成"，不会静默丢特效。
	 *
	 *  Scale 语义：统一缩放；Z 分量会被当作"压扁比例"传给 cue
	 *  （GameplayCue 参数里能承载形状的自由浮点只有一个，见 URogueGameplayCue_Impact）。 */
	static void PlayImpactEffect(AActor* SourceActor, const FVector& Location, const FRotator& Rotation, ERogueImpactVisualStyle Style, const FVector& Scale = FVector(1.0f));

	/** 从"特效来源"解析出驱动 cue 的 ASC；解析不到返回 nullptr。 */
	static class UAbilitySystemComponent* ResolveCueSourceASC(AActor* SourceActor);

private:
	UPROPERTY(Replicated) int32 CurrentWave = 0;
	UPROPERTY(Replicated) int32 EnemiesDefeated = 0;
	UPROPERTY(Replicated) float RunTimeSeconds = 0.0f;
	UPROPERTY(Replicated) bool bGameOver = false;
	UPROPERTY(Replicated) bool bBossAlive = false;
};
