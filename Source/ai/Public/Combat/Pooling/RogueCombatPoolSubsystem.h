// 当前 World 的战斗对象池；Acquire 只取用/定位，调用方随后执行对应 ActivatePooled*。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Pooling/RogueCombatPools.h"
#include "Framework/RogueGameModeConfig.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "RogueCombatPoolSubsystem.generated.h"

class ARogueEnemy;
class ARogueProjectile;
class ARogueEnemyProjectile;
class ARogueRocketProjectile;
class ARogueExperiencePickup;
class ARogueImpactEffect;
class ARogueLaserBeam;
class ARogueOrbitingBlade;
class ARogueCombatAccentEffect;
class ARogueIceFlameField;
struct FRogueIceFlameCombatSettings;
struct FRogueCombatAccentSettings;

UCLASS()
class AI_API URogueCombatPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 预热所有对象池 */
	void Prewarm(
		AActor* OwnerActor,
		const FRoguePoolSettings& Settings,
		TSubclassOf<ARogueEnemy> EnemyClass,
		TSubclassOf<ARogueExperiencePickup> ExperiencePickupClass,
		const FRogueCombatPoolPrewarmClasses& AdditionalClasses);

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

    /** 复用仅本机显示的施法/受击短特效，专用服务器不创建。 */
    UFUNCTION(BlueprintCallable,Category="Rogue|特效")
    ARogueCombatAccentEffect* PlayCombatAccent(const FRogueCombatAccentSettings& Settings,const FVector& Location,const FVector& Direction,bool bImpact);

    /** 权威地面区域：同一玩家落在同一小块地面时刷新，空闲实例复用；不限制真实伤害区域数量。 */
    ARogueIceFlameField* SpawnIceFlameField(TSubclassOf<ARogueIceFlameField> Class,AActor* Source,const FVector& Location,const FVector& Normal,float Radius,float DPS,const FRogueIceFlameCombatSettings& Settings);

	/** 打印池大小、峰值与运行时生成尝试数（含失败）。
	 *  在日志中搜索 [Pool]；增加预热前先检查占用率、类配置与内存成本。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Pool")
	void LogPoolDiagnostics();

private:
	UPROPERTY()
	FRogueCombatPools CombatPools;
    UPROPERTY() TArray<TObjectPtr<ARogueCombatAccentEffect>> AccentPool;
    UPROPERTY() TArray<TObjectPtr<ARogueIceFlameField>> IceFlamePool;
};
