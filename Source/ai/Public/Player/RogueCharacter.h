#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/EngineTypes.h"
#include "Player/RoguePlayerLifeState.h"
#include "Player/RogueCharacterVisualComponent.h"
#include "Shared/RogueTypes.h"
#include "GameplayTagContainer.h"
// 倒地等待 / 复活无敌两个状态 GE 的句柄（只用于调试与失效报错，不参与判定）。
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

	/** 设置当前生命。**这是唯一的写入点** —— 属性集是真相源，
	 *  运行时字段 CurrentHealth / CurrentArmor 只由属性变化回调去更新，
	 *  任何地方都不要直接给它们赋值。
	 *
	 *  注：护甲的所有变化（伤害扣减、自然回复）也由 GAS 承担
	 *  （回复走 `URogueArmorRechargeEffect` 这个 Periodic GE），C++ 侧原本不需要写入口 ——
	 *  但"升级护甲上限时等量补甲"的联动后来搬到了 `HandleMaxArmorChanged`，那里需要一个写入口，
	 *  于是 `SetCurrentArmor` 又回来了。 */
	void SetCurrentHealth(float NewHealth);

	/** 设置当前护甲。与 SetCurrentHealth 同为一个写入点，理由见上。 */
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

	/** 立刻把光标/输入模式切到"可操作 UI"。
	 *
	 *  ⚠️ **只负责打开，不负责关闭**（传 false 会被忽略）。
	 *
	 *  为什么：光标与输入模式的**唯一权威**是 `ARogueHUD::UpdateUIInputState` ——
	 *  它每帧按"暂停菜单 / 已阵亡 / 商店开 / 有待选卡"四个本地状态推导，并且天然对客户端生效
	 *  （服务端改 SetInputMode 只影响主机自己那份 PlayerController）。
	 *
	 *  本函数存在的意义只是**同帧反馈**：刚进入某个需要光标的界面时不必等 HUD 下一帧推导。
	 *  它**不能**负责关闭，否则会重现这个真实 bug：
	 *    开着商店时升级 → 关闭商店 → 这里把光标关掉，
	 *    而 HUD 因为"推导出的状态没变"（待选卡还在，一直属于需要光标）而不重设，
	 *    结果鼠标消失、待选的升级卡点不了。 */
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

	// ---- 服务端权威生命周期 ----
	/** 进入十秒濒死状态，重复调用不重置期限。客户端只读取复制状态。 */
	/** 仅在十秒期限到达且整局仍进行时复活。 */
	void Revive();
	/** 团灭：取消复活并保持幽灵，直到重开。 */
	void FinalizeForGameOver();

	// ---- 武器加成的跨端快照 ----
	// 真相源是玩家 ASC 上 URogueAttributeSet 的 12 个加成属性（升级 GE 写入，只有服务端能写）；
	// 这个快照是**同步管道**：实测 PlayerState 上的复制属性（属性集里的那 12 个、以及挂在
	// PlayerState 上的结构体快照）**都到不了客户端**，而**角色自己的复制属性**能到
	// （bDead 就是这么过去的），所以快照挂在这里。
	// 详见 FRogueWeaponBonusValues 的注释与 Docs/GAS-Main-Architecture-Plan.md。

	/** 服务端：把当前加成值推进快照（脏检查，变化才赋值）。 */
	void ApplyWeaponBonusSnapshot(const FRogueWeaponBonusValues& InValues);

	/** 各端：读武器加成快照（武器侧唯一的取值来源）。 */
	const FRogueWeaponBonusValues& GetWeaponBonusSnapshot() const { return WeaponBonusSnapshot; }

	/** 本机是不是"权威侧"（单机 / 监听服 / 专用服）。
	 *
	 *  ⚠️ 绝对不要用 `HasAuthority()` 代替它来判断"该不该做服务端逻辑"：
	 *  客户端上本机拥有的角色 Role 也是 ROLE_Authority，`HasAuthority()` 会返回 true。
	 *  这个坑对玩家角色是致命的（客户端会自己排复活、自己回血），而敌人不受影响
	 *  （敌人客户端是 SimulatedProxy，Role 不是 Authority）。 */
	bool IsAuthoritySide() const;

	/** 按生命周期应用本机幽灵/正常状态；保存并恢复各组件原有碰撞配置。 */
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
	// 武器升级**不再**有专属的分发/RPC：它们和属性升级走同一条路 ——
	// 升级 GE 改玩家 ASC 上 URogueAttributeSet 的加成属性，
	// 武器开火时读「数据表基础值 + 属性加成」（ARogueWeaponBase::GetEffective*）。
	//
	// 被删掉的旧路径（保留这段说明，避免有人再"优化"回来）：
	//   · ApplySharedWeapon{Damage,Speed,Range}Bonus / DispatchWeaponUpgrade
	//   · Multicast_ApplyWeaponUpgrade（Reliable 多播，各端重放加法）
	//   它们的代价：武器配置成了第二个数值真相源、顺序/丢包会让各端漂移、
	//   新增一类升级要同时改分发+多播+每把武器的应用函数。

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

	// ---- 输入绑定 ----
	// 全部走 Enhanced Input（旧的 legacy `BindAxis` / `BindAction` 支路已随
	// DefaultInput.ini 的 19 条旧映射一起删除，见 SetupPlayerInputComponent 的注释）。

	/** 解析输入资产、登记映射上下文、绑定 10 个动作。
	 *
	 *  资产缺失时打 Error 并**不绑定任何输入** —— 不做"能用几个算几个"的部分绑定，
	 *  也不再有回退路径：半套输入（移动正常但冲刺没反应）比彻底没有更难排查。
	 *  资产清单见 `Docs/Enhanced-Input-Migration.md`。 */
	void SetupEnhancedInput(UInputComponent* PlayerInputComponent);

	// Enhanced Input 的处理函数：都是薄适配层，转调上面的 legacy 处理函数，
	// 所以"按 WASD 移动"这条逻辑只有一份实现。
	void HandleMoveInput(const FInputActionValue& Value);
	void HandleLookInput(const FInputActionValue& Value);
	void HandleZoomInput(const FInputActionValue& Value);
	void HandleDashInput();
	void HandleJumpStarted();
	void HandleJumpStopped();

	void HandleDash(float DeltaSeconds);
	void ApplyBalanceAsset();

	/** 挂上生命 / 护甲的自然回复 GE（`URoguePeriodicRegenEffect` 的两个子类）。
	 *
	 *  迁移前这里是两个**逐帧**函数 `HandleRecovery` / `HandleArmorRecharge`：
	 *  每帧读属性、乘 DeltaSeconds、判标签、钳上限。现在回复完全由 GAS 的 Periodic GE 承担。
	 *
	 *  **幂等**：先查同类 GE 在不在身上再挂。角色重生时 Pawn 换了但 PlayerState / ASC 保留，
	 *  `InitializeGASAttributes` 会再跑一次 —— 不查就会挂上第二份，回复速度直接翻倍。 */
	void EnsureRegenEffects();

	/** 把 URoguePlayerBalanceAsset 的基础数值灌进 GAS 属性集的 BaseValue。
	 *  这是旧系统 BaseGroup 的替代品：GAS 用 SetNumericAttributeBase 写一次绝对值，
	 *  升级走 GE 修饰器，两者不会像旧系统那样叠加翻倍。
	 *  只在服务端调用（属性会自动同步到客户端）。 */
	void InitializeGASAttributes();

	void InitializeWeapons();
	void TickWeapons(float DeltaSeconds);

	// ---- 武器技能（开火能力的授予与激活）----

	/** 给每把武器授予并激活它的开火能力（URogueWeaponAbility）。
	 *
	 *  每帧调一次、很便宜、且**幂等**：
	 *    · 授予只在权威端做，而且先查"这个 ASC 上有没有同类能力"，
	 *      所以角色重生（Pawn 换了但 PlayerState/ASC 保留）不会重复授予；
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

	/** 每 10 秒打一行"每把武器的能力是否激活 + 开了多少次火 + 当前生效冷却"。
	 *  为什么需要它：开火已经搬进能力了，光看"数值对不对"证明不了循环真的在跑 ——
	 *  必须同时看到**开火次数在涨**、且服务端与客户端同一玩家的数字量级一致。 */
	void TickWeaponFireReport(float DeltaSeconds);

	float WeaponFireReportNextTime = 0.0f;
	int32 WeaponFireReportCount = 0;

	/** 报告窗口内的帧时采样（用来算"最近 10 秒平均帧时"）。 */
	float FireReportAccumulatedTime = 0.0f;
	int32 FireReportFrameCount = 0;

	void UpdateVisualPresentation(float DeltaSeconds);
	void LevelUp();

	// ---- GAS 事件回调 ----

	/** 订阅死亡标签。只会真正绑定一次（PossessedBy 与 OnRep_PlayerState 都会调）。
	 *  只有死亡走事件：它需要在"标签出现"的那一刻被捕获，不能等到下一帧轮询。
	 *  属性镜像则统一由 SyncAttributeMirrors 每帧同步（见那里为什么不用委托）。 */
	void BindAbilitySystemEvents();

	/** State.Dead 标签出现 → 安排死亡处理 */
	void HandleDeathTagChanged(FGameplayTag Tag, int32 NewCount);

	/** 上限提升时"补满那部分增量"（升级最大生命 / 护甲上限时等量回复）。
	 *
	 *  原本住在 `URogueAttributeSet::PostGameplayEffectExecute`，但那个钩子**只对 Instant GE 触发**，
	 *  而升级 GE 现在是 Infinite + Stacking —— 留原处会静默失效，所以搬到属性变化委托上。
	 *  详见 `BindAbilitySystemEvents` 里的说明。
	 *
	 *  ⚠️ 两个处理函数内部都**只在权威侧生效**：属性变化会随复制到达客户端并同样触发委托，
	 *     在客户端改数值只会与服务端下一次复制打架。 */
	void HandleMaxHealthChanged(const struct FOnAttributeChangeData& Data);
	void HandleMaxArmorChanged(const struct FOnAttributeChangeData& Data);

	/** 延后到帧末执行的死亡处理（避免在死亡 GE 自己的标签回调里移除它） */
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

	// ---- 输入（Enhanced Input）----
	// 留空时按 `/Game/Input/` 下的约定名加载（IMC_Default / IA_Move / …）。
	// ⚠️ **任何一个缺失都会整体回退到 legacy 绑定** —— 半套绑定比全 legacy 更难排查，
	//    启动日志会打印走的是哪一条路径、缺了哪个资产。
	//    资产清单、按键映射与修饰器配置见 Docs/Enhanced-Input-Migration.md。

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

	/** 正常/濒死/出局与复活截止时刻作为同一属性复制，唯一生命周期真值。 */
	UPROPERTY(ReplicatedUsing = OnRep_LifeState)
	FRoguePlayerLifeState LifeState;

	UPROPERTY(Replicated)
	FRogueWeaponBonusValues WeaponBonusSnapshot;

	UFUNCTION()
	void OnRep_LifeState();

	/** 复活保护沿用原有配置，默认 1.5 秒；复活等待固定十秒。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Death")
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
