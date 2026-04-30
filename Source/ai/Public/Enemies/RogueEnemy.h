#pragma once

#include "CoreMinimal.h"
#include "Enemies/RogueEnemyArchetypes.h"
#include "Enemies/RogueEnemyVisualResources.h"
#include "GameFramework/Character.h"
#include "Core/RogueTypes.h"
#include "RogueEnemy.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class ARogueCharacter;
class AAIController;
class UDataTable;

/**
 * 敌人基类 —— 使用 C++ Tick 中的原型驱动逻辑控制移动和攻击
 *
 * 蓝图子类继承此类后，可以：
 * - 重写 BlueprintNativeEvent 钩子来自定义初始化、受伤、死亡等行为
 * - 调用 Action 接口（MoveInDirection、FireProjectileAtPlayer 等）驱动行为
 */
UCLASS(Blueprintable, BlueprintType)
class AI_API ARogueEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	ARogueEnemy();

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// ===========================================================
	//  生命周期接口（由生成系统调用）
	// ===========================================================

	/** 初始化敌人属性（由生成系统调用） */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Lifecycle")
	void InitializeEnemy(const FRogueEnemyProfile& InProfile);

	/** 从对象池激活敌人 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Lifecycle")
	void ActivatePooledEnemy(AActor* InOwner, const FVector& SpawnLocation, const FRotator& SpawnRotation);

	/** 回收到对象池 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Lifecycle")
	void DeactivateToPool();

	/** 是否在对象池中可用 */
	UFUNCTION(BlueprintPure, Category = "Enemy|Lifecycle")
	bool IsAvailableInPool() const { return bPoolAvailable; }

	// ===========================================================
	//  状态查询接口（蓝图/行为树可读取）
	// ===========================================================

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	int32 GetExperienceReward() const { return ExperienceReward; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	bool IsBoss() const { return bIsBoss; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	FVector GetHealthBarWorldLocation() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	ERogueEnemyType GetEnemyType() const { return EnemyType; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetMoveSpeed() const { return MoveSpeed; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetContactDamage() const { return ContactDamage; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetContactInterval() const { return ContactInterval; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	const FRogueEnemyArchetype& GetCurrentArchetype() const { return CurrentArchetype; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	const FRogueEnemyMovementArchetype& GetMovementArchetype() const { return CurrentArchetype.Movement; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	const FRogueEnemyRangedArchetype& GetRangedArchetype() const { return CurrentArchetype.Ranged; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	bool HasRangedAttack() const { return CurrentArchetype.Ranged.bUsesRangedAttack; }

	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	ERogueEnemyMovementModel GetMovementModel() const { return CurrentArchetype.Movement.Model; }

	/** 获取远程攻击冷却剩余时间 */
	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetRangedAttackCooldownRemaining() const { return RangedAttackTimer; }

	/** 获取地面波攻击冷却剩余时间 */
	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetGroundWaveAttackCooldownRemaining() const { return GroundWaveAttackTimer; }

	/** 地面波是否正在扩散 */
	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	bool IsGroundWaveActive() const { return bGroundWaveActive; }

	/** 当前地面波半径，给震荡柱蓝图视觉使用 */
	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetGroundWaveCurrentRadius() const { return GroundWaveCurrentRadius; }

	/** 当前地面波进度，0-1 */
	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetGroundWaveAlpha() const { return GroundWaveAlpha; }

	/** 获取爆发冲刺冷却剩余时间 */
	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetBurstCooldownRemaining() const { return BurstCooldown; }

	/** 获取爆发冲刺剩余持续时间 */
	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetBurstTimeRemaining() const { return BurstTimeRemaining; }

	/** 是否正在爆发冲刺中 */
	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	bool IsBurstCharging() const { return BurstTimeRemaining > 0.0f; }

	/** 获取环绕方向（1.0 或 -1.0） */
	UFUNCTION(BlueprintPure, Category = "Enemy|State")
	float GetOrbitDirection() const { return OrbitDirection; }

	// ===========================================================
	//  行为树/蓝图可调用的动作接口
	// ===========================================================

	/** 获取玩家角色引用 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Action")
	ARogueCharacter* GetPlayerCharacter() const;

	/** 获取到玩家的 2D 距离 */
	UFUNCTION(BlueprintPure, Category = "Enemy|Action")
	float GetDistanceToPlayer() const;

	/** 获取朝向玩家的 2D 方向向量（归一化） */
	UFUNCTION(BlueprintPure, Category = "Enemy|Action")
	FVector GetDirectionToPlayer() const;

	/** 获取到玩家的向量（未归一化） */
	UFUNCTION(BlueprintPure, Category = "Enemy|Action")
	FVector GetVectorToPlayer() const;

	/** 检查玩家是否在指定范围内 */
	UFUNCTION(BlueprintPure, Category = "Enemy|Action")
	bool IsPlayerInRange(float Range) const;

	/** 检查是否在远程攻击范围内 */
	UFUNCTION(BlueprintPure, Category = "Enemy|Action")
	bool IsInRangedAttackRange() const;

	/** 沿指定方向移动（行为树 Task 调用） */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Action")
	void MoveInDirection(const FVector& Direction, float SpeedMultiplier = 1.0f, float DeltaSeconds = 0.0f);

	/** 执行接触伤害检测（行为树 Task/Service 调用） */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Action")
	void PerformContactDamage(float DeltaSeconds);

	/** 向玩家发射弹幕（行为树 Task 调用） */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Action")
	void FireProjectileAtPlayer();

	/** 计算基于原型的移动方向（行为树 Task 调用，返回归一化方向） */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Action")
	FVector CalculateArchetypeMovement(float DeltaSeconds);

	/** 更新爆发冲刺计时器（行为树中 BurstCharge 类型需要每帧调用） */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Action")
	void UpdateBurstTimers(float DeltaSeconds);

	/** 更新远程攻击冷却计时器 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Action")
	void UpdateRangedAttackCooldown(float DeltaSeconds);

	/** 重置远程攻击冷却到下一次射击间隔 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Action")
	void ResetRangedAttackCooldown();

	/** 设置远程攻击冷却时间 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Action")
	void SetRangedAttackCooldown(float NewCooldown);

	// ===========================================================
	//  蓝图可重写的事件/钩子（BlueprintNativeEvent）
	// ===========================================================

	/** 敌人初始化完成后调用（蓝图中可重写以执行自定义初始化） */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Event")
	void OnEnemyInitialized(const FRogueEnemyProfile& Profile);

	/** 敌人从对象池激活时调用 */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Event")
	void OnEnemyActivated();

	/** 敌人回收到对象池时调用 */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Event")
	void OnEnemyDeactivated();

	/** 敌人受到伤害时调用（返回实际伤害值，蓝图可修改伤害逻辑） */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Event")
	float OnDamageTaken(float DamageAmount, AActor* DamageCauser);

	/** 敌人死亡时调用（蓝图中可重写以播放死亡特效等） */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Event")
	void OnEnemyDeath(AActor* Killer);

	/** 每帧行为更新（蓝图中可重写以完全自定义行为，返回 true 表示已处理，跳过基类 Tick 逻辑） */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Event")
	bool OnBehaviorTick(float DeltaSeconds);

	/** 应用外观样式时调用（蓝图中可重写以自定义外观） */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Event")
	void OnApplyVisualStyle();

	/** 地面波开始扩散时调用，震荡柱蓝图可用来播放起手特效 */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|GroundWave")
	void OnGroundWaveStarted(float MaxRadius, float ExpansionDuration);

	/** 地面波扩散中每帧调用，震荡柱蓝图可用 Radius/Alpha 驱动材质或缩放 */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|GroundWave")
	void OnGroundWaveUpdated(float Radius, float Alpha);

	/** 地面波结束时调用，震荡柱蓝图可用来隐藏自定义波纹 */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|GroundWave")
	void OnGroundWaveFinished();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===========================================================
	//  核心属性（蓝图可读写）
	// ===========================================================

	/** 敌人原型配置表（DataTable），如果设置则优先从表中读取原型数据 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Config", meta = (DisplayName = "敌人原型配置表"))
	TObjectPtr<UDataTable> EnemyArchetypeDataTable;

	//UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Enemy|Components")
	//TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "敌人|组件", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ShockPillarMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "敌人|组件", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> GroundWaveMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "敌人|组件", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInstancedStaticMeshComponent> GroundWaveRingSegments;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "敌人|震荡柱|视觉", meta = (DisplayName = "使用内置震荡柱视觉"))
	bool bUseBuiltInShockPillarVisual = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "敌人|震荡柱|视觉", meta = (DisplayName = "震荡柱材质"))
	TObjectPtr<UMaterialInterface> ShockPillarMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "敌人|震荡柱|视觉", meta = (DisplayName = "地面波材质"))
	TObjectPtr<UMaterialInterface> ShockWaveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float MaxHealth = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float CurrentHealth = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float MoveSpeed = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float ContactDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float ContactInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	int32 ExperienceReward = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	FRogueEnemyArchetype CurrentArchetype;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	bool bPoolAvailable = true;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	bool bDead = false;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	bool bIsBoss = false;

	// ===========================================================
	//  内部运行时状态
	// ===========================================================

	UPROPERTY(BlueprintReadWrite, Category = "Enemy|Runtime")
	float BurstCooldown = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Enemy|Runtime")
	float BurstTimeRemaining = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Enemy|Runtime")
	float OrbitDirection = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Enemy|Runtime")
	float ContactTimer = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Enemy|Runtime")
	float RangedAttackTimer = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	float GroundWaveAttackTimer = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	float GroundWaveElapsed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	float GroundWaveCurrentRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	float GroundWaveAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	float GroundWaveEffectiveMaxRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	float GroundWaveEffectiveDuration = 0.0f;

	float LastDamageNumberTime = -100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	TWeakObjectPtr<ARogueCharacter> CachedPlayerCharacter;

	FRogueEnemyVisualResourceLibrary VisualResourceLibrary;

private:
	/** 基类默认行为 */
	void HandleDefaultBehavior(float DeltaSeconds);
	void TouchPlayer(float DeltaSeconds);
	void HandleRangedAttack(ARogueCharacter* PlayerCharacter, float DistanceToPlayer, float DeltaSeconds);
	void HandleGroundWaveAttack(ARogueCharacter* PlayerCharacter, float DistanceToPlayer, float DeltaSeconds);
	void StartGroundWave();
	void UpdateGroundWave(float DeltaSeconds, ARogueCharacter* PlayerCharacter);
	void UpdateGroundWaveRingVisual(float Radius);
	void HideGroundWave();
	bool CanGroundWaveHitPlayer(const ARogueCharacter* PlayerCharacter) const;
	void FireRangedShot(ARogueCharacter* PlayerCharacter);
	void Die(AActor* Killer);
	void ApplyEnemyStyle();
	FVector GetMovementDirection(const FVector& ToPlayer, float DistanceToPlayer, float DeltaSeconds);

	bool bGroundWaveActive = false;
	bool bGroundWaveDamagedPlayer = false;
};
