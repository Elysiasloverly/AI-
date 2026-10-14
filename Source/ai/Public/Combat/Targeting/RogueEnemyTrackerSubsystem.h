// 敌人追踪子系统 —— 管理敌人注册、空间查询与性能判断
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Targeting/RogueEnemyTracker.h"
#include "Framework/RogueGameModeConfig.h"
#include "Shared/RogueTypes.h"
#include "RogueEnemyTrackerSubsystem.generated.h"

class ARogueEnemy;
class ARogueExperiencePickup;

UCLASS()
class AI_API URogueEnemyTrackerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UTickableWorldSubsystem 接口
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** 配置性能参数（由 GameMode 在 BeginPlay 时调用） */
	void Configure(const FRogueRuntimePerformanceSettings& InSettings);

	// ---- 注册/注销 ----
	void RegisterEnemy(ARogueEnemy* Enemy);
	void UnregisterEnemy(ARogueEnemy* Enemy);
	void RegisterExperiencePickup(ARogueExperiencePickup* Pickup);
	void UnregisterExperiencePickup(ARogueExperiencePickup* Pickup);

	// ---- 空间查询 ----
	ARogueEnemy* FindNearestEnemyInRange(const FVector& Origin, float MaxRange, const TArray<TObjectPtr<ARogueEnemy>>& IgnoredEnemies);
	void CollectEnemiesInRange(const FVector& Origin, float Range, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults = 0, bool bSortByDistance = true);
	ARogueExperiencePickup* FindPickupMergeTarget(const FVector& Origin, float MaxDistance) const;

	// ---- 查询接口 ----
	const TArray<TWeakObjectPtr<ARogueEnemy>>& GetActiveEnemies() const { return EnemyTracker.GetActiveEnemies(); }
	int32 GetActiveEnemyCount() const { return EnemyTracker.GetActiveEnemyCount(); }

	/** 统计当前"存活"的某类型敌人数量（用于"同时存活上限"判据）。
	 *  bIncludeBoss = false 时不计入 Boss —— Boss 有独立的生成条件，不该占用普通怪的名额。 */
	int32 GetActiveEnemyCountOfType(ERogueEnemyType EnemyType, bool bIncludeBoss = false) const;

	/** 是否进入"高负载"档 —— **全项目唯一的高负载判据**。
	 *
	 *  阈值来自 `FRogueRuntimePerformanceSettings::CombatEffectsCullEnemyCount`（默认 52），
	 *  可由 GameMode 的 Config 覆盖；设为 0 表示关闭裁剪。
	 *
	 *  ⚠️ 不要在各处另写 `GetActiveEnemyCount() >= <硬编码值>`：
	 *  武器与 HUD 曾经各写了 `>= 40`，后果有两个 ——
	 *    1) 它抢先命中，配置项调到 40 以上完全不起作用；
	 *    2) 40~52 这个区间里，武器已经降级表现，而弹体/命中特效还全开，
	 *       同一帧里两套阈值并存（正是压测档位最需要一致的时候）。
	 *  所有"高负载相关"的判断一律调这里。 */
	bool ShouldCullCombatEffects() const;

private:
	FRogueEnemyTracker EnemyTracker;
	FRogueRuntimePerformanceSettings PerformanceSettings;
};