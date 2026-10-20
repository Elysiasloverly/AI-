#pragma once

#include "CoreMinimal.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
// 攻击能力的句柄：AI 侧只保存句柄、用 TryActivateAbility 激活（见下面的"攻击能力接口"）。
#include "GameplayAbilitySpec.h"
#include "Shared/RogueTypes.h"
#include "RogueEnemy.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class ARogueCharacter;
class AAIController;
class UDataTable;
class URogueAbilitySystemComponent;
class URogueAttributeSet;
class ARogueEnemyProjectile;

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
	// 敌人持有自己的 ASC（OwnerActor = AvatarActor = this），构造时 ConfigureAsEnemy。
	// Minimal 模式不复制活动 GE 明细；标签/Cue 及已声明复制的属性仍可同步。
	// 攻击能力由服务端授予并激活，AI 默认行为也在服务端执行。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 服务端调用的不可靠表现多播；在接收该敌人 RPC 的相关客户端生成零伤害弹体。
	 *  权威弹体单独在服务器模拟，客户端不复制每发弹体 Actor；丢失表现消息可造成少画一发。
	 *  可靠性、相关性与 Actor 通道共同决定接收范围，不保证所有客户端收到。 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SpawnCosmeticProjectile(FVector_NetQuantize SpawnLocation, FVector_NetQuantizeNormal Direction, float Speed, bool bLargeVisual, TSubclassOf<ARogueEnemyProjectile> ProjectileClass);

	/** 整圈只发送一条可靠消息；客户端在本地对象池中重建零伤害弹幕。 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SpawnCosmeticBulletRing(FVector_NetQuantize Center, float StartYaw, float SpawnRadius, const FRogueEnemyBulletRingSettings& Settings, TSubclassOf<ARogueEnemyProjectile> ProjectileClass);

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

	/** 获取当前有效的缓存目标；刷新时选择最近存活玩家，刷新间隔内不保证仍是最近者 */
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
	//  攻击能力接口
	// ===========================================================
	// AI 先检查距离/计时器，再尝试激活攻击能力；伤害执行由能力调用下方接口。
	// 激活失败可能在后续 Tick 重试，调用数不等同于成功攻击数。

	/** 本敌人能用的攻击能力句柄（服务端授予后填写；无效句柄 = 这种攻击不适用）。 */
	FGameplayAbilitySpecHandle ContactAttackAbilityHandle;
	FGameplayAbilitySpecHandle RangedAttackAbilityHandle;

	/** 按当前原型授予攻击能力。**只在服务端**调用，且必须在 ASC->ResetForPool() 之后
	 *  （出池时会清空技能列表，见 ActivatePooledEnemy）。 */
	void GrantAttackAbilities();

	/** 尝试激活某个攻击能力。返回是否真的激活了。 */
	bool TryActivateAttackAbility(const FGameplayAbilitySpecHandle& Handle);

	/** 句柄无效（这只怪根本没有这种攻击能力）时给计时器一个退避，避免 AI 每帧重试注定失败的激活。 */
	static void BackOffMissingAttack(float& Timer, const FGameplayAbilitySpecHandle& Handle);

	/** 这只敌人使用的弹体蓝图类；为空时回退到原生 ARogueEnemyProjectile。
	 *  放在敌人 BP 的类默认值里即可让普通弹、Boss 弹和整圈弹幕共用同一套外观；
	 *  服务端生成真实弹体，客户端按同一个类生成零伤害表现弹体。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (DisplayName = "敌人弹体蓝图类"))
	TSubclassOf<ARogueEnemyProjectile> EnemyProjectileClass;

	/** Boss 专用弹体蓝图类；bIsBoss 为真且配置了它时优先使用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (DisplayName = "Boss 弹体蓝图类"))
	TSubclassOf<ARogueEnemyProjectile> BossProjectileClass;

	/** 取实际使用的弹体类：Boss 优先 BossProjectileClass，其余按 EnemyProjectileClass，最后回退原生类。 */
	TSubclassOf<ARogueEnemyProjectile> GetEnemyProjectileClass() const;

	/** 取 Boss 弹体类（供对象池预热；不依赖当前 bIsBoss）。 */
	TSubclassOf<ARogueEnemyProjectile> GetBossProjectileClass() const;

	/** 是否有玩家处在接触伤害距离内（AI 的决策判据，只算距离、不结算伤害）。 */
	bool HasTouchingPlayer() const;

	// ---- 攻击执行（由 URogueEnemyAttackAbility 调用）----
	// 这些是"打出去"的那几步；决策在 DecideXAttack 里，两者刻意分开。

	/** 接触攻击：对**所有**贴身玩家各结算一次。返回是否打到了人。 */
	bool ExecuteContactAttack();

	/** 远程攻击：按原型发射单弹、扇形或整圈弹幕。返回是否执行了发射。 */
	bool ExecuteRangedAttack();

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

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===========================================================
	//  GAS 组件
	// ===========================================================

	/** 敌人的能力系统组件。Minimal 复制模式（构造里由 ConfigureAsEnemy 设定）。 */
	UPROPERTY(VisibleAnywhere, Category = "Enemy|GAS")
	TObjectPtr<URogueAbilitySystemComponent> AbilitySystemComponent;

	/** 与玩家共用属性集类型；敌人初始化写生命并清零护甲、减伤和 Damage 元属性。
	 *  其余属性仍有属性集默认值，不能假定恒为 0。 */
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
	 *  在它自己的标签回调里移除它就是重入。所以用 SetTimerForNextTick 移出当前调用栈。 */
	void HandleDeathTagChanged(FGameplayTag Tag, int32 NewCount);

	/** 服务端按当前目标距离设置复制频率预算；不是每连接独立频率，也不保证实际发送频率。 */
	void UpdateReplicationBudget(float DistanceToPlayer);

	/** 通过 SetTimerForNextTick 延后执行的死亡处理 */
	void ProcessDeath();

	/** 死亡处理已排队（防止标签回调与 Tick 重复触发） */
	bool bPendingDeath = false;

	/** 初始化/池复用期间抑制"伤害飘字"。
	 *  为什么需要：InitializeEnemy 会把新一只的血量写进 AttributeSet，
	 *  而 AttributeSet 里还留着上一只的数值。若上一只残留 5 点、新一只是 3 点上限，
	 *  属性从 5 变 3 会被 HandleHealthChanged 解读成"受到 2 点伤害"，凭空弹一个飘字。
	 *  初始化写入不是战斗伤害，必须排除掉。 */
	bool bSuppressDamageFeedback = false;

	/** 提交伤害飘字：普通敌人 0.12s、Boss 0.06s 节流，窗口内累加 */
	void ShowDamageNumber(float AppliedDamage);

	// ===========================================================
	//  核心属性（蓝图可读写）
	// ===========================================================

	/** 敌人原型配置表（DataTable），如果设置则优先从表中读取原型数据 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Config", meta = (DisplayName = "敌人原型配置表"))
	TObjectPtr<UDataTable> EnemyArchetypeDataTable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "敌人|组件", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ShockPillarMesh;

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

	// InitializeEnemy 按 Profile 重写血量、移速、接触伤害、类型与奖励；ContactInterval 保留配置值。
	// 实际血量以 AttributeSet 为准，CurrentHealth 为镜像，直接改镜像不等于修改 GAS 血量。

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
	ERogueEnemyType EnemyType = ERogueEnemyType::Rifleman;

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

	float LastDamageNumberTime = -100.0f;

	/** 节流窗口累积伤害，在下一次符合间隔的伤害事件到来时合并显示；回池前未刷出的尾项会清零 */
	float PendingDamageNumber = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Runtime")
	TWeakObjectPtr<ARogueCharacter> CachedPlayerCharacter;

	/** 目标重选间隔；缓存有效时按此节流，失去存活状态时下次 Tick 立即重选。
	 *  目标位置与距离排序在间隔内可能变化，容许误差需按玩法验证。 */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|AI")
	float TargetRefreshInterval = 0.2f;

	float TargetRefreshCooldown = 0.0f;

private:
	/** 基类默认行为 */
	void HandleDefaultBehavior(float DeltaSeconds);

	// ---- 攻击决策（AI 侧：只判断"要不要打"，打出去由能力做）----
	//
	// 接触/远程成功激活后重置各自计时器。
	// 句柄缺失退避 0.5s，其他失败可在后续 Tick 重试。
	void DecideContactAttack(float DeltaSeconds);
	void DecideRangedAttack(ARogueCharacter* PlayerCharacter, float DeltaSeconds);

	void FireRangedShot(ARogueCharacter* PlayerCharacter);
	void SpawnBulletRing(const FVector& Center, float StartYaw, float SpawnRadius, const FRogueEnemyBulletRingSettings& Settings, float ProjectileDamage, TSubclassOf<ARogueEnemyProjectile> ProjectileClass);
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
	 * 接触伤害这类"范围判定"必须遍历全部玩家，
	 * 不能只看当前锁定的那一个目标（否则队友站在同一圈里反而无伤）。
	 * TInlineAllocator<4> 为前 4 项提供内联存储，超过后仍可分配；它不限制联机人数。
	 */
	void GatherAlivePlayers(TArray<ARogueCharacter*, TInlineAllocator<4>>& OutPlayers) const;

	// 复制在场标记，由客户端映射为 Tick、碰撞和本地池/死亡标记。
	// bHidden 也独立复制；不能仅靠显隐推断其他本地活动状态。
	// 快速回池再激活可能在复制间隔内合并，中间转换不保证逐次到达客户端。
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedActive)
	bool bReplicatedActive = false;

	UFUNCTION()
	void OnRep_ReplicatedActive();

	/** 把复制来的"在场"状态翻译成本机状态（只在客户端做，服务端自己就是权威）。 */
	void ApplyReplicatedActiveState();

	/** 模型/配饰只显示，胶囊负责碰撞；补正旧蓝图的零重力配置。构造后与出池时应用。 */
	void ConfigureGroundedCollision();

	// ---- 出池时复用的材质实例 ----
	// 常量颜色/自发光参数的 MID 按敌人实例缓存，减少池复用时的重复 UObject 创建。
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedShockPillarMID;
};
