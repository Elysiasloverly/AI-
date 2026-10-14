#pragma once

#include "CoreMinimal.h"
#include "Framework/RogueGameModeConfig.h"
#include "RogueCombatPools.generated.h"

class AActor;
class APawn;
class UWorld;
class ARogueEnemy;
class ARogueProjectile;
class ARogueEnemyProjectile;
class ARogueRocketProjectile;
class ARogueExperiencePickup;
class ARogueImpactEffect;
class ARogueLaserBeam;
class ARogueOrbitingBlade;

struct AI_API FRogueCombatPoolPrewarmClasses
{
	TArray<TSubclassOf<ARogueEnemy>> EnemyClasses;
	TArray<TSubclassOf<ARogueProjectile>> PlayerProjectileClasses;
	TArray<TSubclassOf<ARogueEnemyProjectile>> EnemyProjectileClasses;
	TArray<TSubclassOf<ARogueRocketProjectile>> RocketProjectileClasses;
	TArray<TSubclassOf<ARogueExperiencePickup>> ExperiencePickupClasses;
	TArray<TSubclassOf<ARogueLaserBeam>> LaserBeamClasses;
	TArray<TSubclassOf<ARogueOrbitingBlade>> OrbitingBladeClasses;
};

/** 单个池的运行时诊断数据（不参与反射，纯 C++ 统计） */
struct FRoguePoolRuntime
{
	/** 环形扫描起点：上次命中位置 +1。避免每次都从池尾重扫整个池 */
	int32 Cursor = 0;

	/** 累计"池内未命中 → 运行时新建"的次数。>0 就说明预热不够，需要调高预热量 */
	int32 RuntimeSpawnCount = 0;

	/** 池达到过的最大长度，用来反推"到底该预热多少" */
	int32 PeakSize = 0;
};

USTRUCT()
struct AI_API FRogueCombatPools
{
	GENERATED_BODY()

public:
	void Prewarm(
		UWorld* World,
		AActor* OwnerActor,
		const FRoguePoolSettings& Settings,
		TSubclassOf<ARogueEnemy> EnemyClass,
		TSubclassOf<ARogueExperiencePickup> ExperiencePickupClass,
		const FRogueCombatPoolPrewarmClasses& AdditionalClasses);

	ARogueEnemy* AcquireEnemy(UWorld* World, TSubclassOf<ARogueEnemy> EnemyClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueProjectile* AcquirePlayerProjectile(UWorld* World, TSubclassOf<ARogueProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueEnemyProjectile* AcquireEnemyProjectile(UWorld* World, TSubclassOf<ARogueEnemyProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueRocketProjectile* AcquireRocketProjectile(UWorld* World, TSubclassOf<ARogueRocketProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueExperiencePickup* AcquireExperiencePickup(UWorld* World, TSubclassOf<ARogueExperiencePickup> PickupClass, AActor* OwnerActor, const FVector& SpawnLocation);
	ARogueImpactEffect* AcquireImpactEffect(UWorld* World, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueLaserBeam* AcquireLaserBeam(UWorld* World, TSubclassOf<ARogueLaserBeam> BeamClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueOrbitingBlade* AcquireOrbitingBlade(UWorld* World, TSubclassOf<ARogueOrbitingBlade> BladeClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation);

	/** 打印所有池的运行时诊断（池大小 / 峰值 / 运行时新建次数）。
	 *  排查"池够不够"用：RuntimeSpawnCount > 0 就说明该调高对应的预热量。 */
	void LogPoolDiagnostics();

private:
	UPROPERTY()
	TArray<TObjectPtr<ARogueEnemy>> EnemyPool;

	UPROPERTY()
	TArray<TObjectPtr<ARogueProjectile>> PlayerProjectilePool;

	UPROPERTY()
	TArray<TObjectPtr<ARogueEnemyProjectile>> EnemyProjectilePool;

	UPROPERTY()
	TArray<TObjectPtr<ARogueRocketProjectile>> RocketProjectilePool;

	UPROPERTY()
	TArray<TObjectPtr<ARogueExperiencePickup>> ExperiencePickupPool;

	UPROPERTY()
	TArray<TObjectPtr<ARogueImpactEffect>> ImpactEffectPool;

	UPROPERTY()
	TArray<TObjectPtr<ARogueLaserBeam>> LaserBeamPool;

	UPROPERTY()
	TArray<TObjectPtr<ARogueOrbitingBlade>> OrbitingBladePool;

	// ---- 每个池的运行时诊断数据（非反射成员） ----
	FRoguePoolRuntime EnemyPoolRuntime;
	FRoguePoolRuntime PlayerProjectilePoolRuntime;
	FRoguePoolRuntime EnemyProjectilePoolRuntime;
	FRoguePoolRuntime RocketProjectilePoolRuntime;
	FRoguePoolRuntime ExperiencePickupPoolRuntime;
	FRoguePoolRuntime ImpactEffectPoolRuntime;
	FRoguePoolRuntime LaserBeamPoolRuntime;
	FRoguePoolRuntime OrbitingBladePoolRuntime;
};
