#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueRocketProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UParticleSystem;
class APawn;


/** 池化范围伤害弹体；命中或阻挡后爆炸，超时只回池。
 *  类本身不复制，客户端可创建零伤害表现副本；伤害入口检查目标 ASC 权限。 */
UCLASS()
class AI_API ARogueRocketProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARogueRocketProjectile();

	virtual void Tick(float DeltaSeconds) override;
	virtual void ActivatePooledRocket(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage, float InExplosionRadius);
	virtual void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

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











	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float CollisionRadius = 18.0f;

	/** 爆炸冲击效果的生命周期 */
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float ImpactEffectLifetime = 0.16f;

	/** 保留的粒子配置字段；当前 C++ 爆炸路径未读取它（使用 ARogueImpactEffect），蓝图可自行使用 */
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	TObjectPtr<UParticleSystem> ExplosionBurstEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Pooling")
	float PooledLifetime = 5.0f;

	float Damage = 40.0f;
	float ExplosionRadius = 280.0f;
	bool bExploded = false;
	float RemainingLifetime = 0.0f;
	bool bPoolAvailable = true;
};
