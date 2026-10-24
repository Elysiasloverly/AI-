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

/** A weak class bucket never owns actors; the reflected master arrays retain them. */
struct FRoguePoolClassRuntime
{
	TArray<TWeakObjectPtr<AActor>> Actors;
	int32 Cursor = 0;
	uint64 Hits = 0;
	uint64 Misses = 0;
	uint64 Scanned = 0;
};

/** 单个池的运行时诊断数据（不参与反射，纯 C++ 统计） */
struct FRoguePoolRuntime
{
	/** Exact-class buckets avoid scanning actors of unrelated classes. */
	TMap<TWeakObjectPtr<UClass>, FRoguePoolClassRuntime> Classes;

	/** 池内未命中后尝试运行时 SpawnActor 的累计次数（含生成失败）。需结合类匹配、占用率与内存预算判断是否增加预热 */
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

	/** 打印池大小、记录的峰值与运行时生成尝试次数；用于定位容量或类配置问题。 */
	void LogPoolDiagnostics();

private:
	friend struct FRogueReliabilityTestAccess;
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
