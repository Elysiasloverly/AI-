#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemies/Data/RogueFlyingEnemySettings.h"
#include "RogueEnemyProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class APawn;

/**
 * 敌人弹体：只负责移动、碰撞、伤害与生命周期。
 *
 * 外观完全由蓝图决定：C++ 不再创建任何 StaticMesh / 灯光组件，也不写死网格或材质路径。
 * 蓝图子类可以自由添加、删除、替换组件；C++ 只在激活/回收/高负载裁剪时发事件，
 * 由蓝图决定怎么表现。
 */
UCLASS(Blueprintable)
class AI_API ARogueEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARogueEnemyProjectile();

	virtual void Tick(float DeltaSeconds) override;

	/** 从对象池激活。伤害只在服务端结算；客户端表现弹体传入 0 伤害。 */
	void ActivatePooledProjectile(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage, bool bInBossShot, float InLifetime = 0.0f, float InVisualScale = 1.0f, bool bInSimpleVisual = false, const FRogueEnemyProjectileMotion& InMotion = FRogueEnemyProjectileMotion());
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }
    bool IsUsingBatchedMovement() const { return bBatchedMovement; }

    /** 有自定义移动需求时可关闭；复杂弹道和实现了 Event Tick 的蓝图自动使用原移动组件。 */
    UPROPERTY(EditDefaultsOnly, Category="Rogue|Performance", meta=(DisplayName="直线弹幕集中更新"))
    bool bAllowBatchedMovement = true;

	/** 本次激活的视觉缩放（整圈弹幕会传 0.4 这类值）；蓝图可据此缩放自己的组件。 */
	UFUNCTION(BlueprintPure, Category = "Rogue|Projectile")
	float GetVisualScale() const { return VisualScale; }

	UFUNCTION(BlueprintPure, Category = "Rogue|Projectile")
	bool IsBossShot() const { return bBossShot; }

	UFUNCTION(BlueprintPure, Category = "Rogue|Projectile")
	bool IsSimpleVisual() const { return bSimpleVisual; }

	/** 命中特效缩放（世界单位）；蓝图可在类默认值里改。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Projectile", meta = (DisplayName = "命中特效缩放"))
	FVector ImpactEffectScale = FVector(0.24f);

	/** 未命中时的存活时间；调用方可以缩短，敌方弹幕统一最多存活 15 秒。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Pooling", meta = (DisplayName = "未命中存活时间", ClampMin = "0.1", ClampMax = "15", Units = "s"))
	float PooledLifetime = 15.0f;

	/** 本次激活的 Boss 标记；蓝图可据此换材质/特效。 */
	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Projectile")
	bool bBossShot = false;

	/** 整圈弹幕的简化表现标记；蓝图可据此隐藏装饰组件。 */
	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Projectile")
	bool bSimpleVisual = false;

	/** 本次激活的视觉缩放；蓝图可据此缩放组件。 */
	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Projectile")
	float VisualScale = 1.0f;

protected:
	virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** 激活完成、碰撞开启前触发；蓝图在这里设置外观。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Projectile")
	void OnProjectileActivated(bool bInBossShot, bool bInSimpleVisual, float InVisualScale);

	/** 回池前触发；蓝图在这里清理动画/粒子。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Projectile")
	void OnProjectileDeactivated();

	/** 高负载裁剪状态变化时触发；蓝图可隐藏拖尾/灯光等装饰。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Projectile")
	void OnProjectileCulled(bool bCulled);

private:
    friend class URogueEnemyBulletSubsystem;
    bool CanBatchMovement() const;
    void AdvanceBatchedMovement(float DeltaSeconds,TArray<FHitResult>& Hits);
    bool bBatchedMovement=false;
    uint64 BatchGeneration=0;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	void ResolveImpact(AActor* OtherActor, const FVector& ImpactLocation, const FVector& ImpactNormal);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** 蓝图可改碰撞球半径（组件上的 Sphere Radius）；这里记录激活前的基础值。 */
	float BaseCollisionRadius = 22.0f;

    FRogueEnemyProjectileMotion PatternMotion;
    float PatternAge=0;
    bool bPatternLaunched=true, bPatternReturned=false;
    FVector PatternVelocity=FVector::ZeroVector;
    FGuid PatternOwnerCycle;
	float Damage = 10.0f;
	float RemainingLifetime = 0.0f;
	bool bResolvedImpact = false;
	bool bVisualsSimplified = false;
	bool bPoolAvailable = true;
	bool bWasActivated = false;
};
