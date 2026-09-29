// 敌人生成子系统 —— 管理敌人生成、波次推进与生成位置计算
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/RogueGameModeConfig.h"
#include "Core/RogueTypes.h"
#include "RogueSpawnSubsystem.generated.h"

class ARogueEnemy;
class ARogueArena;
class ARogueCharacter;
class URogueGameBalanceAsset;
class URogueCombatPoolSubsystem;
class URogueEnemyTrackerSubsystem;
class UDataTable;

UCLASS()
class AI_API URogueSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 配置生成参数（由 GameMode 在 BeginPlay 时调用）
	 *  InEnemyArchetypeTable：敌人原型表（`DT_FRogueEnemyArchetypeRow`），
	 *  用于读取每种敌人的"每波刷新上限"；传 nullptr 则视为全部不限。 */
	void Configure(
		TSubclassOf<ARogueEnemy> InDefaultEnemyClass,
		const TMap<ERogueEnemyType, TSubclassOf<ARogueEnemy>>& InEnemyClassMap,
		const FRogueSpawnSettings& InSpawnSettings,
		const FRogueBossSettings& InBossSettings,
		URogueGameBalanceAsset* InBalanceAsset,
		UDataTable* InEnemyArchetypeTable = nullptr);

	/** 每帧调度生成（由 GameMode 在 Tick 中调用） */
	void TickSpawning(float DeltaSeconds, FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena);

	/** 生成普通敌人 */
	bool SpawnEnemy(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena);

	/** 生成 Boss */
	void SpawnBoss(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena);

private:
	ARogueEnemy* SpawnConfiguredEnemy(const FRogueEnemyProfile& EnemyProfile, float DesiredDistance, float MinimumDistance, const ARogueArena* Arena, ARogueCharacter* Character);
	FVector FindSpawnLocation(float DesiredDistance, float MinimumDistance, const ARogueArena* Arena, ARogueCharacter* Character) const;

	/**
	 * 候选出生点是否离**所有**活着的玩家都足够远。
	 * 联机下的关键点：MinimumDistance 本来只对着"参照玩家"算，
	 * 于是怪可以合法地刷在队友脸上（参照玩家是主机时尤其明显）。
	 * "不刷脸上"这个约束必须对每个玩家都成立。
	 */
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

	/** 敌人原型表：每种敌人的"每波刷新上限"从这里读 */
	UPROPERTY()
	TObjectPtr<UDataTable> EnemyArchetypeTable;

	/** 本波各类型已刷出的累计只数；波次切换时 Reset()。Boss 不计入。 */
	TMap<ERogueEnemyType, int32> SpawnedCountThisWave;

	/** SpawnedCountThisWave 对应的波次号，用来判断"跨波了没有" */
	int32 CountedWave = 0;

	/** 已为本波打过日志的（类型）；两道闸门各一套，保证每种怪每波每原因只刷一条日志 */
	TSet<ERogueEnemyType> AliveCapLoggedTypes;
	TSet<ERogueEnemyType> WaveBudgetLoggedTypes;

	FRogueSpawnSettings SpawnSettings;
	FRogueBossSettings BossSettings;
};
