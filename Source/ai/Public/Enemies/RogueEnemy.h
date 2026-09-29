#pragma once

#include "CoreMinimal.h"
#include "Enemies/RogueEnemyArchetypes.h"
#include "Enemies/RogueEnemyVisualResources.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Core/RogueTypes.h"
#include "RogueEnemy.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class ARogueCharacter;
class AAIController;
class UDataTable;
class URogueAbilitySystemComponent;
class URogueAttributeSet;

/**
 * 敌人基类 —— 使用 C++ Tick 中的原型驱动逻辑控制移动和攻击
 *
 * 蓝图子类继承此类后，可以：
 * - 重写 BlueprintNativeEvent 钩子来自定义初始化、受伤、死亡等行为
 * - 调用 Action 接口（MoveInDirection、FireProjectileAtPlayer 等）驱动行为
 */
UCLASS(Blueprintable, BlueprintType)
class AI_API ARogueEnemy : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARogueEnemy();

	virtual void Tick(float DeltaSeconds) override;

	// ---- GAS ----
	// 敌人的 ASC 挂在**敌人自己**身上（OwnerActor = AvatarActor = this）：
	// 敌人没有 PlayerState 那一层，也不需要跨 Pawn 重生保留状态。
	// ReplicationMode = Minimal（在 URogueAbilitySystemComponent 构造里设）：
	//   服务端权威跑 AI 与全部 GE 结算；
	//   客户端只收属性同步（血条）与 GameplayCue（特效），不收 GE 明细。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 服务端 → 所有客户端：在本地生成一发**纯表现**的敌方弹体。
	 *
	 *  为什么不直接复制弹体 actor：弹体数量大、生命周期短、而且走对象池，
	 *  逐个复制 actor 会和池复用冲突（回收/复用的瞬间状态会乱）。
	 *  而弹体的**逻辑**（命中、伤害、回收）现在全在服务端，
	 *  客户端只需要"看起来有一发弹从敌人身上飞出去"，所以用广播让各端各自本地生成。
	 *
	 *  Unreliable：掉一发纯表现弹体无所谓；用 Reliable 反而会在丢包时堆积重传。 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SpawnCosmeticProjectile(FVector_NetQuantize SpawnLocation, FVector_NetQuantizeNormal Direction, float Speed, bool bLargeVisual);

	URogueAttributeSet* GetRogueAttributeSet() const { return AttributeSet; }

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

	/** 获取**当前锁定的目标玩家**（联机下 = 活着的玩家里最近的那一个） */
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
	//  GAS 组件
	// ===========================================================

	/** 敌人的能力系统组件。Minimal 复制模式（构造里由 ConfigureAsEnemy 设定）。 */
	UPROPERTY(VisibleAnywhere, Category = "Enemy|GAS")
	TObjectPtr<URogueAbilitySystemComponent> AbilitySystemComponent;

	/** 属性集。与玩家共用同一个 URogueAttributeSet —— 敌人只用到其中
	 *  Health / MaxHealth / Damage(元属性) / DamageReductionPercent 这几项，
	 *  其余玩家专属属性在敌人身上恒为 0。
	 *  取舍：拆成"共享血量集 + 玩家专属集"更干净，但要动已经跑通的玩家侧；
	 *  当前规模下一套共用集更省事，代价只是敌人多带几个恒定空的浮点。 */
	UPROPERTY(VisibleAnywhere, Category = "Enemy|GAS")
	TObjectPtr<URogueAttributeSet> AttributeSet;

	/** 把原型里的血量写进 GAS 属性（服务端权威）。
	 *
	 *  方向：**AttributeSet 是真相源**，CurrentHealth 只是给 HUD / 追踪器 / 奖励结算读的镜像
	 *  （由 HandleHealthChanged 维护）。这个函数只在"初始化 / 池复用"时调用一次，
	 *  把上一只残留的数值覆盖掉，并顺带清空护甲、减伤、元属性伤害。 */
	void SyncHealthToAbilitySystem();

	/** 属性集里的 Health 变化 → 刷新 CurrentHealth 镜像并弹伤害飘字。
	 *  CurrentHealth 是**只读镜像**：真相源是 AttributeSet。 */
	void HandleHealthChanged(const struct FOnAttributeChangeData& ChangeData);

	/** State.Dead 标签出现 → 安排死亡处理。
	 *  不在这里直接调 Die()：这个回调是在"施加死亡 GE"的过程中被触发的，
	 *  而 Die() → DeactivateToPool() → ResetForPool() 会去移除那个 GE，
	 *  在它自己的标签回调里移除它就是重入。所以延到本帧末尾处理。 */
	void HandleDeathTagChanged(FGameplayTag Tag, int32 NewCount);

	/** 按"离玩家多远"分档调整网络复制频率（只有服务端有效）。
	 *  敌人数量最多、单体最不重要，是复制预算里最先要压的一类：
	 *  预热量 160、后期存活上限 250+，按默认 100Hz 会产生 2.5 万次/秒量级的更新尝试。 */
	void UpdateReplicationBudget(float DistanceToPlayer);

	/** 延后执行的死亡处理（本帧末尾） */
	void ProcessDeath();

	/** 死亡处理已排队（防止标签回调与 Tick 重复触发） */
	bool bPendingDeath = false;

	/** 初始化/池复用期间抑制"伤害飘字"。
	 *  为什么需要：InitializeEnemy 会把新一只的血量写进 AttributeSet，
	 *  而 AttributeSet 里还留着上一只的数值。若上一只残留 5 点、新一只是 3 点上限，
	 *  属性从 5 变 3 会被 HandleHealthChanged 解读成"受到 2 点伤害"，凭空弹一个飘字。
	 *  初始化写入不是战斗伤害，必须排除掉。 */
	bool bSuppressDamageFeedback = false;

	/** 弹一次伤害飘字（带 0.12s 节流 + 窗口内累加） */
	void ShowDamageNumber(float AppliedDamage);

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

	/** 是否让敌人之间互不阻挡（默认开启）。
	 *  关闭它时会退回"所有 Pawn 互相阻挡"的默认行为，几十只敌人挤向玩家时会互相顶死。
	 *  注意：玩家也是 Pawn，所以开启后玩家也不再被敌群挡住（可以穿过去）。
	 *  详见 ARogueEnemy 构造函数里的说明。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "敌人|碰撞", meta = (DisplayName = "敌人之间互不阻挡"))
	bool bIgnoreOtherPawnCollision = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "敌人|震荡柱|视觉", meta = (DisplayName = "震荡柱材质"))
	TObjectPtr<UMaterialInterface> ShockPillarMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "敌人|震荡柱|视觉", meta = (DisplayName = "地面波材质"))
	TObjectPtr<UMaterialInterface> ShockWaveMaterial;

	// ---- 战斗数值（**每次从池里激活时被 FRogueEnemyProfile 覆盖**）----
	// ⚠️ 这些值不由蓝图默认值决定：ARogueEnemy::InitializeEnemy() 会用刷怪方传进来的
	// FRogueEnemyProfile（来自 GameMode / DT_FRogueEnemyArchetypeRow）重写它们，
	// 所以想调数值请改数据表或平衡资产，改这里的默认值不会有任何效果。
	// 保留 EditAnywhere 是为了运行时调试与蓝图特例（例如某个 Boss 想临时放大血量）。

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

	// ---- 地面波的**表现态**（复制给客户端）----
	// 为什么这几个要复制：AI 现在只在服务端跑（见 Tick），客户端的震荡柱不会再自己
	// 推进地面波；但那条扩散的波纹必须看得见，所以把"扩散到哪了"作为状态同步过去，
	// 客户端据此自己更新环形视觉。
	//
	// 复制的是半径/透明度而不是发 RPC：地面波是**连续变化**的，
	// 逐帧广播 RPC 既费带宽又会被不可靠通道丢包导致视觉跳变；
	// 复制状态则由引擎按变化量自动节流，丢包后下一帧自然追上。

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Enemy|Runtime")
	bool bGroundWaveActive = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Enemy|Runtime")
	float GroundWaveCurrentRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Enemy|Runtime")
	float GroundWaveAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Enemy|Runtime")
	float GroundWaveEffectiveMaxRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Enemy|Runtime")
	float GroundWaveEffectiveDuration = 0.0f;

	float LastDamageNumberTime = -100.0f;

	/** 伤害飘字节流窗口内攒下的伤害，窗口打开时一起弹出（保证飘字总量 == 实际掉血量） */
	float PendingDamageNumber = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	TWeakObjectPtr<ARogueCharacter> CachedPlayerCharacter;

	/** 目标重选节流（秒）。
	 *  选目标要在"所有玩家"里比距离，没必要每帧做：0.2s 的过期在 900uu/s 下
	 *  只有不到 200uu 的位置误差，对追赶型 AI 完全够用；
	 *  而 250 只怪 × 每帧一次全玩家扫描是纯浪费。 */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|AI")
	float TargetRefreshInterval = 0.2f;

	float TargetRefreshCooldown = 0.0f;

	FRogueEnemyVisualResourceLibrary VisualResourceLibrary;

private:
	/** 基类默认行为 */
	void HandleDefaultBehavior(float DeltaSeconds);
	void TouchPlayer(float DeltaSeconds);
	void HandleRangedAttack(ARogueCharacter* PlayerCharacter, float DistanceToPlayer, float DeltaSeconds);
	void HandleGroundWaveAttack(ARogueCharacter* PlayerCharacter, float DistanceToPlayer, float DeltaSeconds);
	void StartGroundWave();
	void UpdateGroundWave(float DeltaSeconds);
	void UpdateGroundWaveRingVisual(float Radius);
	void HideGroundWave();
	bool CanGroundWaveHitPlayer(const ARogueCharacter* PlayerCharacter) const;
	void FireRangedShot(ARogueCharacter* PlayerCharacter);
	void Die(AActor* Killer);
	void ApplyEnemyStyle();
	FVector GetMovementDirection(const FVector& ToPlayer, float DistanceToPlayer, float DeltaSeconds);

	/**
	 * 重新挑选目标玩家。**只在服务端调用**（AI 只在服务端跑）。
	 *
	 * 联机的点在这里：目标 = 活着的玩家里最近的那一个。
	 * 旧实现固定取 GetPlayerCharacter(this, 0)，在 2–4 人合作里是错的 ——
	 * 所有敌人永远只追玩家 0，其他人站着不动也不会被打。
	 * 以后想换成仇恨/威胁锁定，只需要改这一个函数。
	 */
	void RefreshTargetPlayer();

	/**
	 * 收集当前所有活着的玩家。
	 * 接触伤害、地面波这类"范围判定"必须遍历全部玩家，
	 * 不能只看当前锁定的那一个目标（否则队友站在同一圈里反而无伤）。
	 * 用 TInlineAllocator<4> 是为了不做堆分配：联机人数上限就是 4。
	 */
	void GatherAlivePlayers(TArray<ARogueCharacter*, TInlineAllocator<4>>& OutPlayers) const;

	/**
	 * 本轮地面波已经命中过的玩家。
	 * 单机时它等价于原来的 bool bGroundWaveDamagedPlayer；
	 * 联机时必须逐人记录 —— 否则先被波扫到的玩家会"吃掉"整轮伤害。
	 * 用弱引用是为了玩家中途退出/销毁后不会留下野指针。
	 */
	TArray<TWeakObjectPtr<ARogueCharacter>, TInlineAllocator<4>> GroundWaveHitPlayers;

	// ---- 联机：把"敌人在不在场"这**一位**复制给客户端 ----
	//
	// 为什么必须复制：ActivatePooledEnemy / DeactivateToPool 改的全是**本机状态** ——
	//   SetActorTickEnabled / SetActorEnableCollision / bPoolAvailable / bDead，
	//   这些都不是复制属性。于是客户端上的敌人一辈子停在"在池里 + 已死"：
	//     · Tick 被关 → UpdateGroundWaveVisualFromReplication() 永不执行
	//       （复制过来的地面波状态没人用 → 客户端看不到震荡柱那条扩散波纹）；
	//     · bDead 恒真 → HandleDeathTagChanged 提前 return（客户端没有死亡表现）；
	//     · 碰撞被关 → 客户端本地生成的弹体直接穿过敌人（打中了也没有命中特效）。
	//   而敌人的显隐（bHidden）是复制属性，所以客户端**看得见**敌人，
	//   只是它的"活动状态"全是错的 —— 表现上就是"主机一切正常、客队那边怪像纸片"。
	//
	// 只复制这一位：具体怎么表现（开 tick / 开碰撞 / 清 bDead）由各端自己决定。
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedActive)
	bool bReplicatedActive = false;

	UFUNCTION()
	void OnRep_ReplicatedActive();

	/** 把复制来的"在场"状态翻译成本机状态（只在客户端做，服务端自己就是权威）。 */
	void ApplyReplicatedActiveState();

	// ---- 出池时复用的材质实例 ----
	// 这两个材质在本敌人身上只用一套**常量**参数（颜色/自发光），
	// 而 ApplyEnemyStyle 每次出池都会跑 —— 每次新建 MID 等于每波数次 UObject 分配与 GC 垃圾
	// （一局下来是数千个）。建一次缓存起来即可。
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedShockPillarMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedShockWaveMID;

	/** 客户端是否正在显示地面波视觉（用于"从有到无"时收尾调用 HideGroundWave）。
	 *  只在客户端分支使用，不复制。 */
	bool bClientGroundWaveVisible = false;

	/** 客户端：根据复制过来的地面波状态刷新环形视觉（AI 不在客户端跑） */
	void UpdateGroundWaveVisualFromReplication();
};
