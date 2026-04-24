#pragma once

#include "CoreMinimal.h"
#include "Core/RogueGameModeConfig.h"
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
};
