#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "Framework/RogueBattleStage.h"
#include "Framework/RogueBossHealthView.h"
#include "RogueGameState.generated.h"

/**
 * 整局共享状态的复制载体，GameMode 在服务端写入，客户端 HUD 读取。
 * CurrentWave 等字段独立复制，不构成客户端原子快照。
 * 10Hz/2Hz 是复制调度参数；实际到达频率取决于网络调度、相关性和属性变化。
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

	/** 服务端集中写入当前整局字段；这些独立 UPROPERTY 的客户端到达不是原子事务。 */
	void SetRunStateSnapshot(int32 InCurrentWave, int32 InEnemiesDefeated, float InRunTimeSeconds, bool bInGameOver, bool bInBossAlive);

	// 表现入口：通过来源 ASC 分发 GameplayCue；仅承载外观，不参与伤害与生命周期判定。

	/** 按调用方约定由服务端触发来源 ASC 的 Cue；函数自身不拦截客户端本地调用。
	 *  Cue 是表现通路，接收范围受 ASC Actor 相关性和网络条件限制，不保证所有客户端收到。
	 *  解析不到 ASC 时只尝试本地生成；不要再重复生成同一份表现。
	 *  Scale.X 作为平面统一缩放，Scale.Z / Scale.X 作为高度比例；Y 不单独传输。 */
	static void PlayImpactEffect(AActor* SourceActor, const FVector& Location, const FRotator& Rotation, ERogueImpactVisualStyle Style, const FVector& Scale = FVector(1.0f));

	/** 从"特效来源"解析出驱动 cue 的 ASC；解析不到返回 nullptr。 */
	static class UAbilitySystemComponent* ResolveCueSourceASC(AActor* SourceActor);

    UFUNCTION(BlueprintPure, Category="Rogue|Run")
    const FRogueBattleStageView& GetBattleStage() const { return BattleStage; }
    void SetBattleStage(const FRogueBattleStageView& View);
    UFUNCTION(BlueprintPure, Category="Rogue|Boss")
    const FRogueBossHealthView& GetBossHealthView() const { return BossHealthView; }
    void RefreshBossHealthView();
private:
    UPROPERTY(Replicated) FRogueBossHealthView BossHealthView;
    UPROPERTY(Replicated) FRogueBattleStageView BattleStage;
	UPROPERTY(Replicated) int32 CurrentWave = 0;
	UPROPERTY(Replicated) int32 EnemiesDefeated = 0;
	UPROPERTY(Replicated) float RunTimeSeconds = 0.0f;
	UPROPERTY(Replicated) bool bGameOver = false;
	UPROPERTY(Replicated) bool bBossAlive = false;
};
