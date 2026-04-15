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

UCLASS()
class AI_API URogueSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 配置生成参数（由 GameMode 在 BeginPlay 时调用） */
	void Configure(
		TSubclassOf<ARogueEnemy> InDefaultEnemyClass,
		const TMap<ERogueEnemyType, TSubclassOf<ARogueEnemy>>& InEnemyClassMap,
		const FRogueSpawnSettings& InSpawnSettings,
		const FRogueBossSettings& InBossSettings,
		URogueGameBalanceAsset* InBalanceAsset);

	/** 每帧调度生成（由 GameMode 在 Tick 中调用） */
	void TickSpawning(float DeltaSeconds, FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena);

	/** 生成普通敌人 */
	void SpawnEnemy(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena);

	/** 生成 Boss */
	void SpawnBoss(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena);

private:
	ARogueEnemy* SpawnConfiguredEnemy(const FRogueEnemyProfile& EnemyProfile, float DesiredDistance, float MinimumDistance, const ARogueArena* Arena, ARogueCharacter* Character);
	FVector FindSpawnLocation(float DesiredDistance, float MinimumDistance, const ARogueArena* Arena, ARogueCharacter* Character) const;
	ERogueEnemyType PickEnemyTypeForCurrentWave(int32 Wave) const;

	/** 根据敌人类型查找对应的蓝图子类，未配置则回退到默认类 */
	TSubclassOf<ARogueEnemy> ResolveEnemyClass(ERogueEnemyType EnemyType) const;

	UPROPERTY()
	TSubclassOf<ARogueEnemy> DefaultEnemyClass;

	/** 敌人类型 → 蓝图子类映射 */
	UPROPERTY()
	TMap<ERogueEnemyType, TSubclassOf<ARogueEnemy>> EnemyClassMap;

	UPROPERTY()
	TObjectPtr<URogueGameBalanceAsset> BalanceAsset;

	FRogueSpawnSettings SpawnSettings;
	FRogueBossSettings BossSettings;
};
