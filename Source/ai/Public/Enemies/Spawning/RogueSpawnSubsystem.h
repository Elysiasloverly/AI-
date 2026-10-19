// 敌人生成子系统 —— 管理敌人生成、波次推进与生成位置计算
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Framework/RogueGameModeConfig.h"
#include "Shared/RogueTypes.h"
#include "RogueSpawnSubsystem.generated.h"

class ARogueEnemy;
class ARogueEnemySpawnRegion;
class ARogueCharacter;
class URogueGameBalanceAsset;
class URogueCombatPoolSubsystem;
class URogueEnemyTrackerSubsystem;
class UDataTable;
struct FCollisionQueryParams;

UCLASS()
class AI_API URogueSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 配置生成参数（由 GameMode 在 BeginPlay 时调用）
	 *  InEnemyArchetypeTable：敌人原型表（`DT_EnemyArchetypes`），
	 *  用于读取各类型的每波累计与同时存活上限；传 nullptr 时这两类上限视为不限。 */
	void Configure(
		TSubclassOf<ARogueEnemy> InDefaultEnemyClass,
		const TMap<ERogueEnemyType, TSubclassOf<ARogueEnemy>>& InEnemyClassMap,
		const FRogueSpawnSettings& InSpawnSettings,
		const FRogueBossSettings& InBossSettings,
		URogueGameBalanceAsset* InBalanceAsset,
		UDataTable* InEnemyArchetypeTable = nullptr);

	/** 每帧调度生成（由 GameMode 在 Tick 中调用） */
	void TickSpawning(float DeltaSeconds, FRogueRunState& RunState, ARogueCharacter* Character, const ARogueEnemySpawnRegion* Region);

	/** 生成普通敌人 */
	bool SpawnEnemy(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueEnemySpawnRegion* Region);

	/** 生成 Boss */
	void SpawnBoss(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueEnemySpawnRegion* Region);

private:
	ARogueEnemy* SpawnConfiguredEnemy(const FRogueEnemyProfile& EnemyProfile, float DesiredDistance, float MinimumDistance, const ARogueEnemySpawnRegion* Region, ARogueCharacter* Character);
	bool FindSpawnLocation(float DesiredDistance, float MinimumDistance, const ARogueEnemySpawnRegion* Region,
		ARogueCharacter* Character, const ARogueEnemy* Enemy, FVector& OutLocation) const;

	/** 向下寻找可站立且能阻挡胶囊的地面，按实际胶囊尺寸放置，并检查头顶及周围空间。 */
	bool TryGroundSpawnLocation(const FVector& Candidate, const ARogueEnemy* Enemy,
		const FCollisionQueryParams& GroundQuery, const ARogueEnemySpawnRegion* Region, FVector& OutLocation) const;

	/** 所有候选点（包括最后回退点）均需远离存活玩家并通过地面检测。 */
	bool IsSpawnLocationClearOfPlayers(const FVector& CandidateLocation, float MinimumDistance) const;

	ERogueEnemyType PickEnemyTypeForCurrentWave(int32 Wave) const;

	/** 波次切换时清空"本波已刷数量"，等于把所有类型的名额回满 */
	void ResetWaveSpawnCounts(int32 Wave);

	/** 根据敌人类型查找对应的蓝图子类，未配置则回退到默认类 */
	TSubclassOf<ARogueEnemy> ResolveEnemyClass(ERogueEnemyType EnemyType) const;

	UPROPERTY()
	TSubclassOf<ARogueEnemy> DefaultEnemyClass;

	/** 敌人类型 → 蓝图子类映射 */
	UPROPERTY()
	TMap<ERogueEnemyType, TSubclassOf<ARogueEnemy>> EnemyClassMap;

	UPROPERTY()
	TObjectPtr<URogueGameBalanceAsset> BalanceAsset;

	/** 敌人原型表：各类型每波累计和同时存活上限从这里读取 */
	UPROPERTY()
	TObjectPtr<UDataTable> EnemyArchetypeTable;

	/** 本波各类型已刷出的累计只数；波次切换时 Reset()。Boss 不计入。 */
	TMap<ERogueEnemyType, int32> SpawnedCountThisWave;

	/** SpawnedCountThisWave 对应的波次号，用来判断"跨波了没有" */
	int32 CountedWave = 0;
	float BossSpawnRetryTimer = 0.0f;

	/** 已为本波打过日志的（类型）；两道闸门各一套，保证每种怪每波每原因只刷一条日志 */
	TSet<ERogueEnemyType> AliveCapLoggedTypes;
	TSet<ERogueEnemyType> WaveBudgetLoggedTypes;

	FRogueSpawnSettings SpawnSettings;
	FRogueBossSettings BossSettings;
};
