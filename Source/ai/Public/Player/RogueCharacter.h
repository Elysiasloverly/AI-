#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/RogueCharacterVisualComponent.h"
#include "Core/RogueTypes.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemInterface.h"
#include "RogueCharacter.generated.h"

class ARogueWeaponBase;
class ARogueWeapon_Projectile;
class ARogueWeapon_Laser;
class ARogueWeapon_Mortar;
class ARogueEnemy;
class ARogueHUD;
class ARogueRocketProjectile;
class ARogueShopTerminal;
class UCameraComponent;
class USpringArmComponent;
class UDataTable;
class URogueCharacterVisualComponent;
class URoguePlayerBalanceAsset;
class URogueAbilitySystemComponent;
class URogueAttributeSet;

UCLASS()
class AI_API ARogueCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARogueCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- GAS 接入 ----
	// ASC 挂在 PlayerState 上，所以 Pawn 这一侧必须做两件事：
	//   服务端：PossessedBy 时把 Avatar 指到自己，并灌入属性基础值
	//   客户端：OnRep_PlayerState 时把 Avatar 指到自己（客户端不灌属性，等同步）
	// 这是 "ASC on PlayerState" 这个模式的固定配套动作，缺一半就会
	// 出现"服务端正常、客户端技能无效"这类只在联机下暴露的问题。
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** 取玩家 ASC（可能为空：在 PlayerState 复制到客户端之前）。 */
	URogueAbilitySystemComponent* GetRogueAbilitySystemComponent() const;

	/** 取玩家属性集（可能为空）。 */
	URogueAttributeSet* GetRogueAttributeSet() const;

	/** 本帧的冲刺方向（相机朝向 × 输入 合成，已归一到水平面）。
	 *  冲刺技能要用它 —— 方向和输入这些"玩家意图"只有 Pawn 侧知道，
	 *  所以技能通过这个接口向 Avatar 索取，而不是自己去读输入。 */
	FVector GetDashDirection() const;

	/** 死亡处理。改为 public：死亡现在由 GAS 的 State.Dead 标签驱动，
	 *  AttributeSet 施加死亡效果后需要有人来做善后，而"善后"属于角色自己的事。 */
	void Die();

	/** 设置当前生命/护甲。**这是唯一的写入点** —— 属性集是真相源，
	 *  运行时字段 CurrentHealth / CurrentArmor 只由属性变化回调去更新，
	 *  任何地方都不要直接给它们赋值。 */
	void SetCurrentHealth(float NewHealth);
	void SetCurrentArmor(float NewArmor);

	/** 清除"护甲回复延迟"状态（升级护甲上限时旧行为会立即开始回复） */
	void ClearRecentlyDamagedState();

	/** 清除冲刺冷却（拿到"冲刺冷却缩减"升级时，让玩家不必继续等旧的更长冷却）。
	 *  旧实现是把剩余冷却钳到新的时长；这里直接清掉 —— 对玩家更宽松，
	 *  也避免去改写一个已经生效的 GE 的时长。 */
	void ClearDashCooldown();

	/** 请求选择第 Index 张升级卡（服务端与客户端都可用，内部走 PlayerState 的 ServerRPC） */
	void RequestUpgradeSelection(int32 UpgradeIndex);

	/** 通过 GAS 施加一次"属性类升级"。
	 *  @return 是否成功（非属性类升级、非服务端、ASC 未就绪都会返回 false）
	 *
	 *  实现要点：一份 GE 承载全部 8 种属性升级 ——
	 *  "改哪个属性"用动态资源 Tag 传（Upgrade.Attribute.*），"改多少"用 SetByCaller 传。 */
	bool ApplyUpgradeEffectViaGAS(ERogueUpgradeType UpgradeType, float Magnitude);

	/** 把属性集的值同步到运行时字段（只读缓存）。
	 *  运行时字段（MaxHealth / MoveSpeed / ...）现在是**属性集的缓存**，
	 *  唯一写入点是属性集本身（初始化、升级 GE、SetCurrentHealth 等）。 */
	void SyncAttributeMirrors();

	void AddExperience(int32 ExperienceAmount);
	void AddMoney(int32 Amount);
	void ApplyUpgrade(const FRogueUpgradeOption& Upgrade);
	void SetMenuInteractionInput(bool bEnabled);
	void SetUpgradeSelectionInput(bool bEnabled) { SetMenuInteractionInput(bEnabled); }

	/** IAbilitySystemInterface 实现：把 ASC 查询转发给 PlayerState。	 *
	 *  **为什么必须实现**：本项目的 ASC 挂在 PlayerState 上（联机 GAS 的标准做法），
	 *  而"通过 Actor 找 ASC"的通用入口（UAbilitySystemGlobals::GetAbilitySystemComponentFromActor
	 *  以及引擎各处对 IAbilitySystemInterface 的查询）只看 Actor 自己：
	 *  既没有接口、Actor 上也没有 ASC 组件 → 返回 nullptr。
	 *  漏了它的后果极其隐蔽：**敌人打玩家永远不掉血**（ApplyDamageToTarget 拿不到目标 ASC 就静默返回），
	 *  而玩家打敌人正常（敌人的 ASC 就在自己身上）。Lyra 里角色同样实现了这个接口，就是为此。 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ---- 复活（联机合作：倒地等待后回出生点重来）----

	/** 安排一次复活：DelaySeconds 秒后自动复活（服务端）。传负值 = 取消（团灭时用）。
	 *
	 *  为什么计时器放在角色自己身上而不是 GameMode：这样"谁的复活什么时候到"只有一个
	 *  明确的归属，GameMode 只负责在死亡/团灭时下指令，Tick 里的检查也在本地完成。 */
	void ScheduleRevive(float DelaySeconds);

	/** 立刻复活：清死亡标签、回血、回出生点、恢复输入与移动，并给一小段无敌保护。
	 *  只在服务端有效（死亡状态是权威的）。 */
	void Revive();

	/** 当前服务器世界时间。复活/无敌这些截止时刻统一用它，避免客户端拿本机时间比较。 */
	float GetServerTimeSeconds() const;

	/** 本机是不是"权威侧"（单机 / 监听服 / 专用服）。
	 *
	 *  ⚠️ 绝对不要用 `HasAuthority()` 代替它来判断"该不该做服务端逻辑"：
	 *  客户端上本机拥有的角色 Role 也是 ROLE_Authority，`HasAuthority()` 会返回 true。
	 *  这个坑对玩家角色是致命的（客户端会自己排复活、自己回血），而敌人不受影响
	 *  （敌人客户端是 SimulatedProxy，Role 不是 Authority）。 */
	bool IsAuthoritySide() const;

	/** 按 bDead 切换本机的倒地/正常表现（输入、移动、碰撞）。服务端与客户端共用。 */
	void ApplyDownedState();
	bool TrySpendMoney(int32 Amount);
	void SetNearbyShopTerminal(ARogueShopTerminal* ShopTerminal);
	void ClearNearbyShopTerminal(const ARogueShopTerminal* ShopTerminal);
	bool GetShopPromptWorldLocation(FVector& OutLocation) const;

	// ---- 武器系统接口 ----

	/** 获取所有武器 Actor */
	const TArray<TObjectPtr<ARogueWeaponBase>>& GetWeapons() const { return Weapons; }

	/** 获取武器配置表，供对象池预热和调试工具读取实际配置类 */
	UDataTable* GetWeaponDataTable() const { return WeaponDataTable; }

	/** 按类型查找武器 */
	template<typename T>
	T* FindWeapon() const
	{
		for (ARogueWeaponBase* Weapon : Weapons)
		{
			if (T* Typed = Cast<T>(Weapon))
			{
				return Typed;
			}
		}
		return nullptr;
	}

	/** 全局武器伤害加成 */
	void ApplySharedWeaponDamageBonus(float Magnitude);

	/** 全局武器攻速加成 */
	void ApplySharedWeaponSpeedBonus(float Magnitude);

	/** 全局武器范围加成 */
	void ApplySharedWeaponRangeBonus(float Magnitude);

	/** 将武器专属升级分发给所有武器 */
	bool DispatchWeaponUpgrade(ERogueUpgradeType UpgradeType, float Magnitude);

	/** 服务端 → 所有客户端：同步一次武器类升级。
	 *
	 *  为什么需要这个广播：武器实例是**每台机器各自生成**的（InitializeWeapons 从同一张
	 *  DT_WeaponConfig 建），而 `ARogueWeaponBase::Config` 是普通结构体、**没有复制**。
	 *  升级只改服务器那一份的话，客户端看到/用到的武器数值就永远停在初始值。
	 *
	 *  为什么不是"复制 Config"而是"广播升级让各端各自应用"：
	 *    · Config 结构体很大，整体复制成本高；
	 *    · 升级序列是确定性的 —— 各端按同样顺序应用同一个升级，结果必然一致。
	 *  Reliable 保证顺序，不会出现"先加数量后加伤害"被乱序的情况。 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplyWeaponUpgrade(ERogueUpgradeType UpgradeType, float Magnitude);

	/** 服务端 → 非本人客户端：广播一次武器开火的**纯表现**。
	 *
	 *  为什么需要它：武器 tick 在联机下分了工（见 TickWeapons）——
	 *  客户端只跑自己的武器（本地预测，开火零延迟），
	 *  别人的武器不再在本地模拟（否则每台机器各算一份，弹道与锁敌目标各显示各的）。
	 *  队友那一屏的弹道就靠这条广播补上。
	 *
	 *  为什么 Unreliable：纯表现，丢一两发只表现为"少看见一颗子弹"，
	 *  不会造成任何结算差异；用 Reliable 反而会在丢包时堆积重传。
	 *
	 *  传 WeaponIndex + WeaponClass 两个字段是为了安全：下标是各端一致的
	 *  （InitializeWeapons 从同一张 DT_WeaponConfig 按同样顺序建），
	 *  但万一不一致，类检查会让它静默失败，而不是把火箭画成子弹。 */
	// 注意：批量化之后不再带 WeaponClass 做校验（每条只带武器下标），
	// 因为"下标各端一致"这一点本来就由 InitializeWeapons 的确定性保证；
	// 省下的每包几字节乘以每秒上千条，才是这次改动的意义。
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_WeaponCosmetics(const TArray<FRogueWeaponCosmeticBatchEntry>& Fires);

	/** 服务端内部入口：把这一发表现塞进**本帧待发表**（供 ARogueWeaponBase 调用）。
	 *  真正的发送在 TickWeapons 末尾的 FlushWeaponCosmetics() —— 每帧至多一条 RPC。 */
	void BroadcastWeaponCosmetic(const ARogueWeaponBase* Weapon, const FRogueWeaponCosmeticFire& Fire);

	/** 把本帧攒下的武器表现合成一条 RPC 发出去（服务端）。 */
	void FlushWeaponCosmetics();


	// ---- 角色属性查询 ----

	UFUNCTION(BlueprintPure)
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure)
	float GetExperiencePercent() const;

	UFUNCTION(BlueprintPure)
	float GetPickupRadius() const { return PickupRadius; }

	UFUNCTION(BlueprintPure)
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure)
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure)
	float GetCurrentArmor() const { return CurrentArmor; }

	UFUNCTION(BlueprintPure)
	float GetMaxArmor() const { return MaxArmor; }

	UFUNCTION(BlueprintPure)
	float GetArmorPercent() const { return MaxArmor > 0.0f ? CurrentArmor / MaxArmor : 0.0f; }

	UFUNCTION(BlueprintPure)
	bool HasArmor() const { return MaxArmor > 0.0f; }

	UFUNCTION(BlueprintPure)
	int32 GetPlayerLevel() const { return PlayerLevel; }

	UFUNCTION(BlueprintPure)
	int32 GetCurrentExperience() const { return CurrentExperience; }

	UFUNCTION(BlueprintPure)
	int32 GetExperienceToNextLevel() const { return ExperienceToNextLevel; }

	UFUNCTION(BlueprintPure)
	int32 GetMoney() const { return Money; }

	UFUNCTION(BlueprintPure)
	bool CanInteractWithShop() const { return NearbyShopTerminal.IsValid(); }

	UFUNCTION(BlueprintPure)
	float GetExperienceMultiplier() const { return ExperienceMultiplier; }

	UFUNCTION(BlueprintPure)
	float GetDashCooldownDuration() const { return DashCooldownDuration; }

	UFUNCTION(BlueprintPure)
	float GetDashCooldownRemaining() const { return DashCooldownRemaining; }

	UFUNCTION(BlueprintPure)
	bool IsDashReady() const { return !bDashActive && DashCooldownRemaining <= 0.0f; }

	UFUNCTION(BlueprintPure)
	bool IsDead() const { return bDead; }

	// ---- 武器聚合查询 ----
	// 把"所有武器里的同类数值"汇总成一个数（总伤害、总攻速、各类武器数量）。
	// 消费方：HUD 的调试面板、升级规则表（例如"镰刀数量"要按实际生效数量判断）。
	// 为什么不直接暴露武器数组让调用方自己遍历：遍历逻辑（含"哪把武器才算有效"）
	// 一旦散到各处就会各自漂移，这里集中一份。

	UFUNCTION(BlueprintPure)
	float GetAttackDamage() const;

	UFUNCTION(BlueprintPure)
	float GetAttackInterval() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveProjectileCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveScytheCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveRocketLauncherCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveLaserCannonCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveHellTowerCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveMortarCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetLaserRefractionCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetScytheCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetRocketLauncherCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetLaserCannonCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetHellTowerCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetMortarCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetProjectileCount() const;

protected:
	virtual void BeginPlay() override;

private:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnCamera(float Value);
	void PitchCamera(float Value);
	void ZoomCamera(float Value);
	void TogglePauseMenu();
	void Interact();
	void Dash();
	void StartJump();
	void StopJump();
	void ChooseUpgradeOne();
	void ChooseUpgradeTwo();
	void ChooseUpgradeThree();
	void HandleDash(float DeltaSeconds);
	void HandleArmorRecharge(float DeltaSeconds);
	void HandleRecovery(float DeltaSeconds);
	void ApplyBalanceAsset();

	/** 把 URoguePlayerBalanceAsset 的基础数值灌进 GAS 属性集的 BaseValue。
	 *  这是旧系统 BaseGroup 的替代品：GAS 用 SetNumericAttributeBase 写一次绝对值，
	 *  升级走 GE 修饰器，两者不会像旧系统那样叠加翻倍。
	 *  只在服务端调用（属性会自动同步到客户端）。 */
	void InitializeGASAttributes();

	void InitializeWeapons();
	void TickWeapons(float DeltaSeconds);
	void UpdateVisualPresentation(float DeltaSeconds);
	void LevelUp();

	// ---- GAS 事件回调 ----

	/** 订阅死亡标签。只会真正绑定一次（PossessedBy 与 OnRep_PlayerState 都会调）。
	 *  只有死亡走事件：它需要在"标签出现"的那一刻被捕获，不能等到下一帧轮询。
	 *  属性镜像则统一由 SyncAttributeMirrors 每帧同步（见那里为什么不用委托）。 */
	void BindAbilitySystemEvents();

	/** State.Dead 标签出现 → 安排死亡处理 */
	void HandleDeathTagChanged(FGameplayTag Tag, int32 NewCount);

	/** 延后到帧末执行的死亡处理（避免在死亡 GE 自己的标签回调里移除它） */
	void ProcessDeath();

	/** 死亡处理已排队 */
	bool bPendingDeath = false;

	/** GAS 事件是否已绑定（防止双路径重复绑定导致回调触发两次） */
	bool bAbilitySystemEventsBound = false;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URogueCharacterVisualComponent> Visuals;


	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSoftObjectPtr<URoguePlayerBalanceAsset> PlayerBalanceAsset;

	/** 武器配置数据表 —— 每行对应一种武器 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TObjectPtr<UDataTable> WeaponDataTable;

	/** 武器子类列表 —— 旧资源仍按顺序使用，缺失项会由代码默认武器补齐 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TArray<TSubclassOf<ARogueWeaponBase>> WeaponClasses;

	/** 当 DataTable / WeaponClasses 里还没正式接入迫击炮时，代码兜底生成用的武器类 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Fallback")
	TSubclassOf<ARogueWeaponBase> DefaultMortarWeaponClass;

	/** 当 DataTable 里还没正式填入迫击炮弹体时，代码兜底生成用的迫击炮弹体类 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Fallback")
	TSubclassOf<ARogueRocketProjectile> DefaultMortarProjectileClass;

	/** 运行时生成的武器 Actor 实例 */
	UPROPERTY()
	TArray<TObjectPtr<ARogueWeaponBase>> Weapons;

	/** 本帧待发的武器表现（服务端）。FlushWeaponCosmetics() 会把它合成一条 RPC 发出去。 */
	TArray<FRogueWeaponCosmeticBatchEntry> PendingWeaponCosmetics;

	UPROPERTY()
	TObjectPtr<URoguePlayerBalanceAsset> LoadedPlayerBalanceAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraTurnSpeed = 2.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraPitchSpeed = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraZoomStep = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MinCameraPitch = -75.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MaxCameraPitch = -35.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MinCameraDistance = 700.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MaxCameraDistance = 1400.0f;

	// "升到下一级需要多少经验"。标 Replicated 是必须的：它会随升级变化，
	// 而拥有者客户端的 HUD 要读它画经验条（不复制的话客户端永远显示初始值）。
	// 走 COND_OwnerOnly（见 GetLifetimeReplicatedProps）：别人的等级进度与我无关。
	UPROPERTY(Replicated, EditDefaultsOnly, Category = "Progression")
	int32 ExperienceToNextLevel = 6;

	// ---- 属性镜像（**不可编辑**）----
	// 这些字段是 GAS 属性集的只读缓存，每帧由 SyncAttributeMirrors() 写入。
	// ⚠️ 不要在这里调数值：真相源是 URoguePlayerBalanceAsset::BaseStats（开局灌入属性集）
	// 与升级 GE；在这里改只会被下一帧覆盖。所以刻意不标 EditDefaultsOnly ——
	// 之前标着它，在蓝图里改这些值毫无效果，是排查起来很费时间的坑。
	// 需要 Inspector 里看当前值时用 VisibleInstanceOnly（运行时实例上可见）。

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float MaxArmor = 25.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float MoveSpeed = 650.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float PickupRadius = 260.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float ExperienceMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Mobility")
	float DashCooldownDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Mobility")
	float DashMinCooldown = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Mobility")
	float DashDuration = 0.16f;

	UPROPERTY(EditDefaultsOnly, Category = "Mobility")
	float DashSpeed = 4200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	float JumpZVelocity = 720.0f;

	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	float AirControl = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	float GravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	int32 JumpMaxCountConfig = 1;

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float CurrentArmor = 0.0f;

	// ---- 成长数据（复制给**拥有者**）----
	// 这几个是"只该由服务端改、但拥有者客户端必须看到"的数据：
	// 升级抽卡、击杀奖励、商店扣费全在服务端结算，客户端只是显示。
	// 不复制的话客户端 HUD 会永远停在 0 金币 / 0 经验 / 1 级。
	// 用 COND_OwnerOnly：别的玩家不需要知道你的钱包。
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Progression")
	int32 CurrentExperience = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Progression")
	int32 PlayerLevel = 1;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Progression")
	int32 Money = 0;

	// ⚠️ 注意：下面这个 float 是**普通成员**，不要给它加 UPROPERTY 宏。
	// 这里曾经残留过一个"孤儿 UPROPERTY(Replicated, ...)"（重复声明去重时
	// 只删了成员行、没删宏），宏于是粘到了 ExperienceRemainder 上，
	// 而真正需要复制的 ExperienceToNextLevel 反而没标 —— 结果是运行期致命错误：
	//   "Attempt to replicate property ... that was not tagged to replicate!"
	// （静态编译完全查不出来，只有真正跑起来才会崩。）
	float ExperienceRemainder = 0.0f;
	float DashCooldownRemaining = 0.0f;
	float DashTimeRemaining = 0.0f;
	// 注：旧的 ArmorRechargeDelayRemaining float 计时器已删除 ——
	// "受伤后护甲延迟回复"现在由 State.RecentlyDamaged 这个带时长的 GE 承担。
	float ForwardInputValue = 0.0f;
	float RightInputValue = 0.0f;
	FVector DashDirection = FVector::ForwardVector;
	TWeakObjectPtr<ARogueShopTerminal> NearbyShopTerminal;
	bool bDashActive = false;

	/** 是否已阵亡（联机合作里 = 倒地等待）。**必须复制**。
	 *
	 *  ⚠️ 为什么不能像原来那样"在客户端由 State.Dead 标签推导"：
	 *    · 客户端上**本机拥有**的角色 `HasAuthority()` 也是 true（本地控制的 Pawn 由客户端自己模拟），
	 *      而"我是不是服务端"必须用 GetNetMode() 判断（见 IsAuthoritySide()）；
	 *    · 实测客户端从来没有进入过倒地表现：HUD 读的 `IsDead()` 在客户端恒为 false，
	 *      于是倒地者在自己屏幕上依然"活着"（没有倒计时、死亡界面也弹不出来），
	 *      而服务端早就不让他动、怪物也早就不索敌他了 —— 两边表现完全脱节。
	 *  现在以服务端复制过来的这一位为准，收到后由 OnRep_Dead 切换本机表现。 */
	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	/** bDead 的客户端回调：只做本机表现（输入 / 移动 / 碰撞），不改数值与位置。 */
	UFUNCTION()
	void OnRep_Dead();

	/** 复活等待时间（秒）。联机合作下死亡 = 倒地等待，而不是出局。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Death")
	float ReviveDelaySeconds = 10.0f;

	/** 复活后的无敌时长（秒）。0 = 关闭该保护。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Death")
	float ReviveInvulnerableSeconds = 1.5f;

	/** 复活用的定时器（服务端）。放在角色上，GameMode 只负责下指令。
	 *  ⚠️ 它只是"三条路里的一条"：实测这条在 PIE 里没有触发过，
	 *  真正的保证是 Tick 里的截止时刻轮询 + GameMode::TickReviveQueue()。 */
	FTimerHandle ReviveTimerHandle;

	/** 复活无敌的到期时刻（服务器世界时间）。0 = 当前没有无敌。
	 *  刻意**不做成复制字段**：伤害只在服务端结算，客户端不需要知道。 */
	float ReviveInvulnerableUntilTime = 0.0f;

	/** 无敌到期检查（服务端，Tick 调用）。 */
	void TickReviveInvulnerability();

	/** 立刻结束无敌（死亡时调用，避免"尸体还带无敌标签"）。 */
	void ClearReviveInvulnerability();
};
