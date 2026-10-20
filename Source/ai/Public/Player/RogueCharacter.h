#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/EngineTypes.h"
#include "Player/RoguePlayerLifeState.h"
#include "Player/RogueCharacterVisualComponent.h"
#include "Shared/RogueTypes.h"
#include "GameplayTagContainer.h"
// 复活保护 GE 句柄；生命阶段和复活期限由 LifeState 与服务端定时器维护。
#include "GameplayEffectTypes.h"
#include "AbilitySystemInterface.h"
// Enhanced Input：处理函数按值语义接收 FInputActionValue（轴值 / 布尔值统一封装）。
// 见 SetupEnhancedInput 与 Docs/Enhanced-Input-Migration.md。
#include "InputActionValue.h"
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
class URogueFloatingComponent;
class URogueTurnSmoothingComponent;
class URoguePlayerBalanceAsset;
class URogueAbilitySystemComponent;
class URogueAttributeSet;
class UInputAction;
class UInputMappingContext;

struct FRoguePlayerCollisionSnapshot
{
	TWeakObjectPtr<class UPrimitiveComponent> Component;
	ECollisionEnabled::Type Collision = ECollisionEnabled::NoCollision;
	bool bGenerateOverlapEvents = false;
	bool bSimulatePhysics = false;
};

UCLASS()
class AI_API ARogueCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARogueCharacter();

	virtual void PreInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_Controller() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- GAS 接入 ----
	// ASC 由 PlayerState 持有，Pawn 是技能作用的 Avatar。
	// 服务端 PossessedBy 刷新 ActorInfo 并初始化基础属性；客户端 OnRep_PlayerState
	// 刷新 ActorInfo 与事件绑定，属性值等待服务端复制。具体调用顺序见对应实现。
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** 取玩家 ASC（可能为空：在 PlayerState 复制到客户端之前）。 */
	URogueAbilitySystemComponent* GetRogueAbilitySystemComponent() const;

	/** 取玩家属性集（可能为空）。 */
	URogueAttributeSet* GetRogueAttributeSet() const;

	/** 按本机缓存输入与相机方向计算水平冲刺方向，无输入时回退到速度或角色朝向。
	 *  输入缓存本身未复制，因此不同端的回退结果不保证相同，需另行验证预测与移动校正。 */
	FVector GetDashDirection() const;

	/** 服务端进入濒死阶段并启动复活等待；死亡标签回调会延后调用，重复请求不重置期限。 */
	void Die();

	/** 经属性集写入当前生命基值；CurrentHealth 镜像在 SyncAttributeMirrors 时刷新。
	 *  调用方负责服务端权限与玩法条件，本函数只将输入钳到非负。 */
	void SetCurrentHealth(float NewHealth);

	/** 经属性集写入当前护甲基值；权限要求和镜像刷新方式同 SetCurrentHealth。 */
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
	 *  RogueUpgradeEffectMap 为每种升级选择独立的 Infinite GE 类，
	 *  目标属性由类固定，量级由 Data.Magnitude 的 SetByCaller 值提供。 */
	bool ApplyUpgradeEffectViaGAS(ERogueUpgradeType UpgradeType, float Magnitude);

	/** 把属性集的值同步到运行时字段（只读缓存）。
	 *  运行时字段（MaxHealth / MoveSpeed / ...）现在是**属性集的缓存**，
	 *  ASC 就绪后按帧读取属性集；初始化、升级与生命结算修改属性集后再刷新这些缓存。 */
	void SyncAttributeMirrors();

	void AddExperience(int32 ExperienceAmount);
	void AddMoney(int32 Amount);
	void ApplyUpgrade(const FRogueUpgradeOption& Upgrade);

	/** 立即开启本机光标和 GameAndUI；传 false 不做处理。
	 *  HUD::UpdateInteractionInputMode 按暂停、商店、升级与生命周期状态决定最终输入模式。
	 *  关闭一个界面时仍可能需要操作另一个界面，因此此入口不直接关闭 UI 输入。 */
	void SetMenuInteractionInput(bool bEnabled);
	void SetUpgradeSelectionInput(bool bEnabled) { SetMenuInteractionInput(bEnabled); }

	/** 将 Actor 上的 ASC 查询转发给 PlayerState。
	 *  UAbilitySystemGlobals 先查询此接口，可选回退也只搜索传入 Actor 的组件，
	 *  不会自动到 PlayerState 查找。PlayerState 尚未就绪时仍可返回 nullptr。 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ---- 服务端权威生命周期 ----
	/** 十秒期限到达、仍待复活且整局未结束时，在同一个 Pawn 上恢复战斗状态。 */
	void Revive();
	/** 团灭：取消复活并保持幽灵，直到重开。 */
	void FinalizeForGameOver();

	// ---- 武器加成的跨端快照 ----
	// 属性集保存权威加成；升级生效后立即刷新 13 项快照，GameMode 每帧补充同步，武器统一读取快照。
	// 快照作为当前实现的独立复制通道，并不意味着 PlayerState 或 AttributeSet 天生不能复制。

	/** 服务端：把当前加成值推进快照（脏检查，变化才赋值）。 */
	void ApplyWeaponBonusSnapshot(const FRogueWeaponBonusValues& InValues);

	/** 服务端：从属性集刷新武器快照，供同帧开火与下一轮升级前置条件读取。 */
	void RefreshWeaponBonusSnapshotFromAttributes();

	/** 各端：读武器加成快照（武器侧唯一的取值来源）。 */
	const FRogueWeaponBonusValues& GetWeaponBonusSnapshot() const { return WeaponBonusSnapshot; }

	/** 是否处于非客户端世界（单机、监听服或专用服）；无 World 时返回 false。
	 *  HasAuthority 判断 Actor 的本地 Role，IsLocallyControlled 判断控制关系，均与 NetMode 不同。
	 *  正常复制的玩家 Pawn 在所属客户端通常是 AutonomousProxy；
	 *  客户端本地创建的 Actor 也可能为 Authority，因此这里显式限定执行环境。 */
	bool IsAuthoritySide() const;

	/** 本机应用幽灵状态；为当前遍历到的 Primitive 保存碰撞、重叠和模拟标志。
	 *  恢复时只处理仍有效的快照组件，移动模式设为 Walking，输入标志重新启用。 */
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

	// ---- 武器升级 ----
	// 属性升级与武器加成都施加到玩家 ASC；武器用数据表基础值和角色加成快照计算生效值。
	// 各端不通过重放升级加法修改武器配置，避免维护两份数值来源。

	/** 服务端按帧广播武器表现；服务端本机和所属客户端跳过回放。
	 *  Unreliable 丢失只影响表现，伤害由服务端独立结算。
	 *  每条仅带 WeaponIndex，各端必须用相同配置与顺序初始化 Weapons；
	 *  接收端只检查下标范围，不校验武器类，配置差异仍可能导致表现错配。 */
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

	/** 当前镰刀吸血比例（0-1）；没有属性集时为 0。 */
	UFUNCTION(BlueprintPure)
	float GetScytheLifestealPercent() const;

	/** 服务端：按镰刀实际造成的伤害回复生命。客户端调用无效。 */
	void ApplyScytheLifesteal(float DamageDealt);

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
	bool IsDead() const { return !IsAlive(); }

	UFUNCTION(BlueprintPure)
	bool IsAlive() const;

	UFUNCTION(BlueprintPure)
	bool IsDowned() const { return LifeState.Phase == ERoguePlayerLifePhase::Downed; }

	UFUNCTION(BlueprintPure)
	bool IsEliminated() const { return LifeState.Phase == ERoguePlayerLifePhase::Eliminated; }

	UFUNCTION(BlueprintPure)
	ERoguePlayerLifePhase GetLifePhase() const { return LifeState.Phase; }

	// ---- 武器聚合查询 ----
	// 为 HUD 和升级规则提供各武器查询。GetAttackDamage/GetAttackInterval 查询主弹体武器，
	// 并非所有武器伤害/攻速总和；数量查询分别提供配置数量与带加成的生效数量。

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

	// ---- 输入绑定 ----
	// 本角色动作通过 Enhanced Input 绑定，未保留 legacy 回退分支。

	/** 解析输入资产、登记映射上下文、绑定 10 个动作。
	 *
	 *  资产缺失时打 Error 并**不绑定任何输入** —— 不做"能用几个算几个"的部分绑定，
	 *  也不再有回退路径：半套输入（移动正常但冲刺没反应）比彻底没有更难排查。
	 *  资产清单见 `Docs/Enhanced-Input-Migration.md`。 */
	void SetupEnhancedInput(UInputComponent* PlayerInputComponent);

	// Enhanced Input 的处理函数转调角色移动、摄像机与技能方法，
	// 所以"按 WASD 移动"这条逻辑只有一份实现。
	void HandleMoveInput(const FInputActionValue& Value);
	void HandleLookInput(const FInputActionValue& Value);
	void HandleZoomInput(const FInputActionValue& Value);
	void HandleDashInput();
	void HandleJumpStarted();
	void HandleJumpStopped();

	void HandleDash(float DeltaSeconds);
	void ApplyBalanceAsset();

	/** 查重后挂载生命和护甲的常驻 Periodic GE，避免重复回复实例。
	 *  当前在基础属性初始化完成后调用；同 Pawn 复活保留原有 ASC 和回复效果。 */
	void EnsureRegenEffects();

	/** 服务端为当前 PlayerState 初始化一次 GAS 基值；未加载平衡资产时使用结构体默认值。
	 *  属性复制需依赖 ASC/AttributeSet 的复制注册；重复调用会由 PlayerState 标志拦截。 */
	void InitializeGASAttributes();

	void InitializeWeapons();
	void TickWeapons(float DeltaSeconds);

	// ---- 武器技能（开火能力的授予与激活）----

	/** 给每把武器授予并激活它的开火能力（URogueWeaponAbility）。
	 *
	 *  每帧调一次、很便宜、且**幂等**：
	 *    · 授予只在 ASC 权威端进行，并按能力类查重；当前复活复用 Pawn，
	 *      重复接管或初始化也不应重复授予同类能力；
	 *    · 激活成功后置 bWeaponAbilitiesSettled，之后就只剩一个 bool 判断。
	 *  为什么必须每帧重试：客户端的能力列表是**复制过来的**，
	 *  到达时刻晚于武器生成，第一次尝试通常会"还没有技能可用"。
	 *
	 *  联机分工（与改造前 TickWeapons 的判据一致）：
	 *    · 本人（主机自己那份 / 客户端自己那份）：本地激活 → LocalPredicted 顺带通知服务端；
	 *    · 服务端上的**远端**玩家：等本人客户端的激活请求（GAS 标准路径）+ 兜底看门狗；
	 *    · 其它客户端：不激活任何人的武器（别人的弹道由服务端广播补上）。
	 */
	void EnsureWeaponAbilities();

	/** 结算完成（全部武器能力都已在跑）—— 之后每帧只做一次 bool 判断 */
	bool bWeaponAbilitiesSettled = false;

	/** 服务端为"远端玩家"等待其客户端激活请求的起始时刻（看门狗用，单位：世界秒） */
	float WeaponAbilityWaitStartTime = -1.0f;

	// ---- 武器技能自检报告（仅 Rogue.SelfTest.WeaponUpgradeChain 打开时输出）----

	/** 开关启用且有 World 时约每十秒输出一组武器报告，单角色最多六组。
	 *  为什么需要它：开火已经搬进能力了，光看"数值对不对"证明不了循环真的在跑 ——
	 *  需结合目标数、激活状态与同玩家日志分析；两端计数不要求逐发相等。 */
	void TickWeaponFireReport(float DeltaSeconds);

	float WeaponFireReportNextTime = 0.0f;
	int32 WeaponFireReportCount = 0;

	/** 报告窗口内的帧时采样（用来算"最近 10 秒平均帧时"）。 */
	float FireReportAccumulatedTime = 0.0f;
	int32 FireReportFrameCount = 0;

	void LevelUp();

	// ---- GAS 事件回调 ----

	/** 每个角色实例绑定一次死亡标签和生命/护甲上限变化委托。
	 *  标签触发后延迟处理濒死；上限委托负责补偿增量，显示镜像仍由逐帧轮询刷新。 */
	void BindAbilitySystemEvents();

	/** State.Dead 标签出现 → 安排死亡处理 */
	void HandleDeathTagChanged(FGameplayTag Tag, int32 NewCount);

	/** 服务端收到上限增加事件时，按增量补当前生命/护甲。
	 *  非周期 Infinite 升级只更新修饰器聚合值，不走执行型 PostGameplayEffectExecute；
	 *  该钩子可用于 Instant 或 Periodic GE 的执行，不能简单等同于所有属性变化。
	 *  当前 AttributeSet 使用普通 Replicated，不能把收到属性复制等同于 GAS 委托触发；
	 *  标准预测通知通常还需在 RepNotify 中调用 GAMEPLAYATTRIBUTE_REPNOTIFY。 */
	void HandleMaxHealthChanged(const struct FOnAttributeChangeData& Data);
	void HandleMaxArmorChanged(const struct FOnAttributeChangeData& Data);

	/** 通过 SetTimerForNextTick 延后死亡处理，避开死亡 GE 的标签回调重入。 */
	void ProcessDeath();

	/** 死亡处理已排队 */
	bool bPendingDeath = false;
	FTimerHandle PendingDeathTimerHandle;

	/** GAS 事件是否已绑定（防止双路径重复绑定导致回调触发两次） */
	bool bAbilitySystemEventsBound = false;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	/** 保留稳定的外观挂接点；子模型与材质在 BP_RogueCharacter 中设置。 */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URogueCharacterVisualComponent> Visuals;

    UPROPERTY(VisibleAnywhere,Category="外观",meta=(DisplayName="漂浮组件"))
    TObjectPtr<URogueFloatingComponent> FloatingMotion;
    UPROPERTY(VisibleAnywhere,Category="外观",meta=(DisplayName="转向平滑组件"))
    TObjectPtr<URogueTurnSmoothingComponent> TurnSmoothing;


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
	float MinCameraPitch = -85.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MaxCameraPitch = -10.0f;

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
	float JumpZVelocity = 680.0f;

	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	float AirControl = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	float GravityScale = 1.8f;

	/** 下落阶段额外乘的重力倍率。 */
	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	float FallGravityMultiplier = 1.45f;

	/** 提前松键时保留的上升速度比例。 */
	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	float JumpCutMultiplier = 0.45f;

	/** 落地前按跳的输入缓冲窗口。 */
	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	float JumpBufferSeconds = 0.12f;

	/** 离开地面后的起跳宽限时间。 */
	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	float CoyoteTimeSeconds = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "跳跃")
	int32 JumpMaxCountConfig = 1;

	/** 每帧维护土狼时间、跳跃输入缓冲和下落重力。 */
	void UpdateJumpFeel(float DeltaSeconds);

	float JumpBufferRemaining = 0.0f;
	float CoyoteTimeRemaining = 0.0f;
	bool bJumpedSinceGrounded = false;

	// ---- 输入（Enhanced Input）----
	// 未指定引用时从 /Game/Input/IMC_Default 与 /Game/Input/IA/IA_* 加载。
	// 任一必需资产缺失就记录错误并取消本次动作绑定；不回退 legacy。
	// 资产清单、按键映射与修饰器配置见 Docs/Enhanced-Input-Migration.md。

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input") TSoftObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TSoftObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TSoftObjectPtr<UInputAction> ZoomAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TSoftObjectPtr<UInputAction> DashAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TSoftObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TSoftObjectPtr<UInputAction> InteractAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TSoftObjectPtr<UInputAction> TogglePauseMenuAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TSoftObjectPtr<UInputAction> ChooseUpgrade1Action;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TSoftObjectPtr<UInputAction> ChooseUpgrade2Action;
	UPROPERTY(EditDefaultsOnly, Category = "Input") TSoftObjectPtr<UInputAction> ChooseUpgrade3Action;

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float CurrentArmor = 0.0f;

	// ---- 成长数据（复制给**拥有者**）----
	// 这几个是"只该由服务端改、但拥有者客户端必须看到"的数据：
	// 升级抽卡、击杀奖励、商店扣费全在服务端结算，客户端只是显示。
	// HUD 从这些本地复制字段读数；服务器操作后需等待对应属性到达。
	// 用 COND_OwnerOnly：别的玩家不需要知道你的钱包。
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Progression")
	int32 CurrentExperience = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Progression")
	int32 PlayerLevel = 1;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Progression")
	int32 Money = 0;

	// 经验倍率计算后的不足一整数部分，仅服务端累计；不向客户端复制。
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

	/** 正常/濒死/出局与复活截止时刻作为同一属性复制，唯一生命周期真值。 */
	UPROPERTY(ReplicatedUsing = OnRep_LifeState)
	FRoguePlayerLifeState LifeState;

	UPROPERTY(Replicated)
	FRogueWeaponBonusValues WeaponBonusSnapshot;

	UFUNCTION()
	void OnRep_LifeState();

	/** 旧蓝图序列化值，仅保留用于迁移检查；实际保护时间由 DA_PlayerBalance.Revival 决定。 */
	UPROPERTY()
	float ReviveInvulnerableSeconds = 1.5f;

	/** 唯一的复活驱动：服务端一次性世界定时器，不依赖 Pawn Tick。 */
	FTimerHandle ReviveTimerHandle;
	FActiveGameplayEffectHandle InvulnerableEffectHandle;
	bool bGhostStateApplied = false;
	bool bActorCollisionBeforeDowned = true;
	TArray<FRoguePlayerCollisionSnapshot> DownedCollisionSnapshots;
	FTransform ReviveSpawnTransform;

	void ApplyLifeStateEffects();
	void StartReviveTimer();
	float GetLifeServerTime() const;
	void RemoveDownedEffect();
	void RemoveInvulnerableEffect();
	friend struct FRoguePlayerLifeCycleTestAccess;
	friend struct FRoguePlayerBalanceTestAccess;
	friend struct FRogueExperiencePickupTestAccess;

public:
	/** 只用于显示，零秒时仍等待服务端 Alive 确认，不在客户端触发复活。 */
	UFUNCTION(BlueprintPure)
	float GetReviveSecondsRemaining() const;
	bool IsRevivePending() const;
	void TryReviveIfExpired();

private:
	/** 开发自检：武器加成的打印次数与上次打印时的值签名（默认关，见 Debug/RogueSelfTest.h）。
	 *  用"值变化才打"而不是"只打一次"：客户端首帧还没收到复制过来的属性值，
	 *  只打一次会看到全 0 并误判成"属性没复制"。 */
	int32 WeaponBonusLogCount = 0;
	FString LastLoggedSelfTestSignature;
};
