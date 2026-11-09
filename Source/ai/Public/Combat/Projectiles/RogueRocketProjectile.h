#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Weapons/RogueWeaponCombatSettings.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "RogueRocketProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UParticleSystem;
class APawn;
class ARogueIceFlameField;


/** 池化范围伤害弹体；命中或阻挡后爆炸，超时只回池。
 *  类本身不复制，客户端可创建零伤害表现副本；伤害入口检查目标 ASC 权限。 */
UCLASS()
class AI_API ARogueRocketProjectile : public AActor
{
	GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="外观|开火特效", meta=(DisplayName="开火闪光与碎光"))
    FRogueCombatAccentSettings FireAccent;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|命中特效",meta=(DisplayName="命中特效风格"))
    ERogueWeaponImpactStyle HitVisualStyle=ERogueWeaponImpactStyle::Default;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|命中特效",meta=(DisplayName="爆炸中心特效"))
    FRogueCombatAccentSettings ExplosionAccent;

	ARogueRocketProjectile();

	virtual void Tick(float DeltaSeconds) override;
	virtual void ActivatePooledRocket(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage, float InExplosionRadius, const FRogueProjectileCombatSettings& Combat = FRogueProjectileCombatSettings(18.f));
	virtual void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

    /** 只缩放根碰撞体下的外观分支，不改命中球；回池后恢复蓝图尺寸。由陨星武器显式调用，霜玉不受影响。 */
    void SetExplosionVisualScale(float Scale);
    /** 仅陨星武器显式开启；每次激活和回池关闭，霜玉与零伤害表现弹不会生成伤害区域。 */
    void ConfigureGroundFlame(const FRogueIceFlameCombatSettings& Settings) { GroundFlame=Settings; }
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|落地蓝焰",meta=(DisplayName="地面蓝焰蓝图"))
    TSubclassOf<ARogueIceFlameField> GroundFlameClass;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	void InitializeRocket(const FVector& InDirection, float InSpeed, float InDamage, float InExplosionRadius);
	void Explode();
    void LeaveGroundFlame(const FVector& Location);
    FRogueIceFlameCombatSettings GroundFlame;
    void RestoreVisualScale();
    TMap<TWeakObjectPtr<USceneComponent>, FVector> AuthoredVisualScales;











	UPROPERTY()
	float CollisionRadius = 18.0f;

	/** 爆炸冲击效果的生命周期 */
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float ImpactEffectLifetime = 0.16f;

	/** 保留的粒子配置字段；当前 C++ 爆炸路径未读取它（使用 ARogueImpactEffect），蓝图可自行使用 */
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	TObjectPtr<UParticleSystem> ExplosionBurstEffect;

	UPROPERTY()
	float PooledLifetime = 30.0f;

	float Damage = 40.0f;
	float ExplosionRadius = 280.0f;
	bool bExploded = false;
	float RemainingLifetime = 0.0f;
	bool bPoolAvailable = true;
};
