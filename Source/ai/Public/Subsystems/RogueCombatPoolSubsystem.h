// 对象池子系统 —— 管理所有战斗对象的池化分配
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/RogueCombatPools.h"
#include "Core/RogueGameModeConfig.h"
#include "RogueCombatPoolSubsystem.generated.h"

class ARogueEnemy;
class ARogueProjectile;
class ARogueEnemyProjectile;
class ARogueRocketProjectile;
class ARogueExperiencePickup;
class ARogueImpactEffect;
class ARogueLaserBeam;
class ARogueOrbitingBlade;

UCLASS()
class AI_API URogueCombatPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 预热所有对象池 */
	void Prewarm(AActor* OwnerActor, const FRoguePoolSettings& Settings, TSubclassOf<ARogueEnemy> EnemyClass, TSubclassOf<ARogueExperiencePickup> ExperiencePickupClass);

	/** 获取敌人 */
	ARogueEnemy* AcquireEnemy(TSubclassOf<ARogueEnemy> EnemyClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation);

	/** 获取玩家弹体 */
	ARogueProjectile* AcquirePlayerProjectile(TSubclassOf<ARogueProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation);

	/** 获取敌人弹体 */
	ARogueEnemyProjectile* AcquireEnemyProjectile(TSubclassOf<ARogueEnemyProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation);

	/** 获取火箭弹体 */
	ARogueRocketProjectile* AcquireRocketProjectile(TSubclassOf<ARogueRocketProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation);

	/** 获取经验拾取物 */
	ARogueExperiencePickup* AcquireExperiencePickup(TSubclassOf<ARogueExperiencePickup> PickupClass, AActor* OwnerActor, const FVector& SpawnLocation);

	/** 获取冲击特效 */
	ARogueImpactEffect* AcquireImpactEffect(const FVector& SpawnLocation, const FRotator& SpawnRotation, AActor* OwnerActor);

	/** 获取激光束 */
	ARogueLaserBeam* AcquireLaserBeam(TSubclassOf<ARogueLaserBeam> BeamClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation);

	/** 获取环绕刀片 */
	ARogueOrbitingBlade* AcquireOrbitingBlade(TSubclassOf<ARogueOrbitingBlade> BladeClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation);

private:
	UPROPERTY()
	FRogueCombatPools CombatPools;
};