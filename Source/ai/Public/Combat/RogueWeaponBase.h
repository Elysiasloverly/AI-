// 武器系统基类 —— 所有武器的公共接口与 DataTable 行结构体
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Core/RogueTypes.h"
#include "RogueWeaponBase.generated.h"

class ARogueCharacter;
class ARogueEnemy;
class ARogueWeaponBase;
class ARogueProjectile;
class ARogueMortarProjectile;
class ARogueRocketProjectile;
class ARogueLaserBeam;
class ARogueOrbitingBlade;
class URogueAttributeSet;
struct FRogueCombatPoolPrewarmClasses;

// ============================================================
// DataTable 行结构体 —— 在编辑器中配置每种武器的属性
// ============================================================
USTRUCT(BlueprintType)
struct AI_API FRogueWeaponTableRow : public FTableRowBase
{
	GENERATED_BODY()

	// ---- 通用属性 ----

	/** 武器显示名称（HUD 用） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "通用", meta = (DisplayName = "武器名称"))
	FString DisplayName;

	/** 武器逻辑类，优先于角色蓝图里的 WeaponClasses 数组 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "通用", meta = (DisplayName = "武器类"))
	TSubclassOf<ARogueWeaponBase> WeaponClass;

	/** 初始数量（0 = 未解锁，1 = 开局拥有） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "通用", meta = (DisplayName = "初始数量"))
	int32 Count = 0;

	/** 基础伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "通用", meta = (DisplayName = "基础伤害"))
	float Damage = 20.0f;

	/** 攻击间隔 / 冷却（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "通用", meta = (DisplayName = "冷却时间"))
	float Cooldown = 1.0f;

	/** 索敌范围 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "通用", meta = (DisplayName = "索敌范围"))
	float Range = 1600.0f;

	/** 对应的"数量升级"枚举类型 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "升级", meta = (DisplayName = "数量升级类型"))
	ERogueUpgradeType CountUpgradeType = ERogueUpgradeType::ProjectileCount;

	// ---- 弹道随机化（弹体 & 火箭通用） ----

	/** 是否启用弹道随机平面偏转 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "弹道随机", meta = (DisplayName = "启用弹道偏转"))
	bool bEnableTrajectoryDeviation = false;

	/** 弹道偏转最大角度（度），实际偏转在 [-MaxAngle, +MaxAngle] 范围内随机 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "弹道随机", meta = (DisplayName = "弹道偏转最大角度", EditCondition = "bEnableTrajectoryDeviation", ClampMin = "0.0", ClampMax = "90.0"))
	float TrajectoryDeviationMaxAngle = 5.0f;

	/** 是否启用弹速随机增幅 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "弹道随机", meta = (DisplayName = "启用弹速随机"))
	bool bEnableSpeedRandomization = false;

	/** 弹速随机增幅比例（0~1），实际弹速 = 基础弹速 × (1 ± Ratio) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "弹道随机", meta = (DisplayName = "弹速随机比例", EditCondition = "bEnableSpeedRandomization", ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedRandomizationRatio = 0.15f;

	// ---- 弹体武器专属 ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通子弹", meta = (DisplayName = "弹体类"))
	TSubclassOf<ARogueProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通子弹", meta = (DisplayName = "弹体速度"))
	float ProjectileSpeed = 1800.0f;

	// ---- 迫击炮武器专属 ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "迫击炮", meta = (DisplayName = "迫击炮弹体类"))
	TSubclassOf<ARogueMortarProjectile> MortarProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "迫击炮", meta = (DisplayName = "爆炸半径"))
	float MortarExplosionRadius = 470.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "迫击炮", meta = (DisplayName = "发射速度"))
	float MortarLaunchSpeed = 760.0f;

	// ---- 镰刀武器专属 ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镰刀", meta = (DisplayName = "刀片类"))
	TSubclassOf<ARogueOrbitingBlade> BladeClass;

	/** 环绕半径。默认 120（原为 220）。
	 *  为什么改小：刀片环能打到的径向范围 = 半径 ± (刀片球半径 46 + 敌人胶囊半径 34)，
	 *  220 时是 141…299；但敌人（Direct 模型）没有保持距离，会一路贴到玩家身上（径向≈0），
	 *  也就是**刀片环整个落在敌群外面**，镰刀几乎砍不到人。120 时范围是 41…199，
	 *  刚好罩住贴脸的敌群，同时 120 − 46 = 74 > 玩家胶囊半径 42，不会打到玩家自己。
	 *  ⚠️ 注意：这个值在 `DT_WeaponConfig` 的每一行里都被显式存了一份（行值优先于这里的默认值），
	 *  所以只改这里不会生效——还要把表里 Scythe 行的"环绕半径"一起改成 120。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镰刀", meta = (DisplayName = "环绕半径"))
	float OrbitRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镰刀", meta = (DisplayName = "旋转速度"))
	float RotationSpeed = 130.0f;

	// ---- 火箭武器专属 ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "火箭", meta = (DisplayName = "火箭类"))
	TSubclassOf<ARogueRocketProjectile> RocketClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "火箭", meta = (DisplayName = "爆炸半径"))
	float ExplosionRadius = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "火箭", meta = (DisplayName = "火箭速度"))
	float RocketSpeed = 900.0f;

	// ---- 激光武器专属 ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "激光", meta = (DisplayName = "激光类"))
	TSubclassOf<ARogueLaserBeam> BeamClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "激光", meta = (DisplayName = "折射次数"))
	int32 RefractionCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "激光", meta = (DisplayName = "折射范围"))
	float RefractionRange = 950.0f;

	// ---- 地狱塔武器专属 ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "地狱塔", meta = (DisplayName = "基础伤害(塔)"))
	float BaseDamage = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "地狱塔", meta = (DisplayName = "每跳伤害递增"))
	float DamageRampPerTick = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "地狱塔", meta = (DisplayName = "伤害跳间隔"))
	float DamageTickInterval = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "地狱塔", meta = (DisplayName = "光束刷新间隔"))
	float BeamRefreshInterval = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "地狱塔", meta = (DisplayName = "更新间隔"))
	float UpdateInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "地狱塔", meta = (DisplayName = "高负载更新间隔"))
	float HeavyCombatUpdateInterval = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "地狱塔", meta = (DisplayName = "高负载光束刷新惩罚"))
	float HeavyCombatBeamRefreshPenalty = 0.05f;

	/** 收集此武器可能用到的对象池类，避免 GameMode 直接知道每种武器字段。 */
	void CollectPoolPrewarmClasses(FRogueCombatPoolPrewarmClasses& OutClasses) const;
};

// ============================================================
// 武器基类 Actor
// ============================================================
UCLASS(Abstract)
class AI_API ARogueWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	ARogueWeaponBase();

	// ---- 生命周期 ----

	/** 由 Character 在 BeginPlay 时调用，传入 DataTable 行数据 */
	void InitializeWeapon(ARogueCharacter* InOwnerCharacter, const FRogueWeaponTableRow& InConfig);

	/** 由 Character 每帧调用 */
	virtual void WeaponTick(float DeltaSeconds) PURE_VIRTUAL(ARogueWeaponBase::WeaponTick, );

	/** 角色死亡时调用，子类可重写做清理 */
	virtual void OnOwnerDied();

	// ---- 联机表现：本人预测 + 服务端广播给别人 ----

	/**
	 * 服务端：把"我刚开了这一发"广播给其他客户端。
	 *
	 * 调用点应该在**权威那一发已经生成之后**（服务端自己有一份真实体，
	 * 本人客户端自己预测了一份），所以广播只用于补上"队友那一屏"。
	 * 在客户端调用它什么也不会发生（客户端本来就是接收方）。
	 */
	void BroadcastCosmeticFire(const FRogueWeaponCosmeticFire& Fire) const;

	/**
	 * 客户端：把这一发**纯表现**地画出来，不结算任何伤害。
	 * 只有"既不是服务端、也不是本人"的客户端会走到这里。
	 * 默认空实现 —— 事件型武器各自重写（见 NeedsCosmeticTick 的说明）。
	 */
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire);

	/**
	 * 本武器在"既不权威、也不是本人"的机器上是否仍需要 tick。
	 *
	 * · 事件型武器（子弹 / 火箭 / 迫击炮 / 光束）：返回 false。
	 *   它们的表现是"一发"，服务端广播一发就够了，本地再算一遍只会各显示各的。
	 * · 持续型武器（环绕刀片）：返回 true。
	 *   它的表现是一条**连续轨道**，没有"某一发"可以让服务端广播；
	 *   而且它的伤害走 ApplyDamageToTarget（服务端权威），本地跑只影响视觉。
	 */
	virtual bool NeedsCosmeticTick() const { return false; }

	// ---- 升级接口 ----
	// **升级不直接改武器配置**（那是自研版本的做法，已删除）。
	// 升级统一落成玩家 ASC 上 URogueAttributeSet 的属性加成，
	// 武器通过上面的 GetEffective* 读取 —— 所以这里没有 OnUpgradeApplied / ApplyShared*Bonus。
	//
	// 新增武器时：基础值放 DT_WeaponConfig，加成从 GetWeaponBonusValues() 读，
	// 若本武器的换算系数与默认不同，重写对应的 GetEffective*（见火箭/迫击炮/激光/地狱塔/镰刀）。

	// ---- 查询接口 ----

	UFUNCTION(BlueprintPure)
	int32 GetCount() const { return Config.Count; }

	/** 生效数量 = 数据表基础数量 + 武器加成（见 GetWeaponBonusValues）。
	 *  历史：以前升级会直接改 Config.Count，所以这里只读 Config 就够了；
	 *  现在升级只改 GAS 属性，武器配置**保持数据表原值不变**。 */
	UFUNCTION(BlueprintPure)
	int32 GetEffectiveCount() const;

	UFUNCTION(BlueprintPure)
	float GetDamage() const { return Config.Damage; }

	UFUNCTION(BlueprintPure)
	float GetRange() const { return Config.Range; }

	// ---- 生效数值（数据表基础值 + GAS 加成）----
	// 武器开火一律用这些函数，而不是直接读 Config.X —— 这条约定就是"所有数值都来自 GAS"的落地点。
	//
	// 为什么是 virtual：**同一个升级在不同武器上的换算系数不一样**（这是自研版本的行为，必须原样保留）：
	//   攻速：多数武器是 `冷却 − 加成`（下限 0.15），火箭是 `冷却 − 加成×2.5`（下限 0.55），
	//         地狱塔是把伤害跳间隔按 0.08 缩放，镰刀是转速 +加成×280
	//   弹速：主武器全额、火箭 ×0.75、迫击炮 ×0.5
	//   伤害：地狱塔是 `塔伤害 +加成×0.125`（每跳递增 +加成×0.0625）
	// 所以"加成数值只有一个来源（玩家 ASC 上的属性集）"，而"怎么用这个加成"由各武器自己决定。

	/** 单发伤害 = Config.Damage + AttackPowerBonus */
	virtual float GetEffectiveDamage() const;

	/** 攻击间隔 = max(0.15, Config.Cooldown − AttackSpeedBonus)
	 *  0.15 秒下限沿用自研版本的行为（防止升级堆到 0 冷却）。 */
	virtual float GetEffectiveCooldown() const;

	/** 索敌范围 = Config.Range + AttackRangeBonus */
	virtual float GetEffectiveRange() const;

	/** 弹速 = BaseSpeed + ProjectileSpeedBonus × GetProjectileSpeedBonusScale()
	 *  @param BaseSpeed 该武器自己的基础弹速（主武器 ProjectileSpeed / 火箭 RocketSpeed / 迫击炮 MortarLaunchSpeed） */
	float GetEffectiveProjectileSpeed(float BaseSpeed) const;

	/** 弹速加成系数：默认全额（1.0），火箭 0.75、迫击炮 0.5。 */
	virtual float GetProjectileSpeedBonusScale() const { return 1.0f; }

	/** 本武器的"数量升级"类型 —— 决定它读哪一种数量加成。
	 *
	 *  ⚠️ 为什么不直接用 `Config.CountUpgradeType`（数据表那一列）：
	 *  实测（自检日志）里多数行的那一列是**默认值**，于是所有武器都会去读同一个加成 ——
	 *  一张"弹体数量+1"的卡会一次性给 5 把武器各 +1。
	 *  自研版本的做法是每把武器**硬编码**自己的数量升级类型（OnUpgradeApplied 里各自判断），
	 *  这里沿用同一口径：子类重写本函数，数据列只在没重写时兜底。 */
	virtual ERogueUpgradeType GetCountUpgradeType() const { return Config.CountUpgradeType; }

	/** 激光折射次数 = Config.RefractionCount + LaserRefractionBonus（旧实现夹在 0..8） */
	int32 GetEffectiveRefractionCount() const;

	/** 获取当前武器的完整配置（可读写） */
	FRogueWeaponTableRow& GetConfig() { return Config; }
	const FRogueWeaponTableRow& GetConfig() const { return Config; }

	/** 武器加成（生效值）：来自拥有者 PlayerState 上的**复制快照**，各端读同一份。
	 *  拿不到时返回一份全 0 的静态空值 —— 调用方不需要判空。 */
	const FRogueWeaponBonusValues& GetWeaponBonusValues() const;

protected:
	// ---- 辅助函数（子类可用） ----

	/** 获取拥有者角色 */
	ARogueCharacter* GetOwnerCharacter() const { return OwnerCharacter; }

	/** 找最近的敌人 */
	ARogueEnemy* FindNearestEnemy(float MaxRange) const;

	/** 找最近的敌人（排除列表） */
	ARogueEnemy* FindNearestEnemyExcluding(const FVector& Origin, const TArray<TObjectPtr<ARogueEnemy>>& Excluded, float MaxRange) const;

	/** 收集范围内敌人 */
	void CollectEnemiesInRange(float InRange, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults = 0) const;

	/** 获取拥有者位置 */
	FVector GetOwnerLocation() const;

	/** 获取摄像机右方向（水平） */
	FVector GetOwnerCameraRightVector() const;

	/** 是否高负载战斗 */
	bool IsHeavyCombat() const;

	/** 生成激光 Beam（通用辅助） */
	void SpawnLaserBeam(const FVector& Start, const FVector& End, bool bInfernoStyle = false, bool bImpactEffect = true, float Lifetime = 0.10f);

	/** 光束类武器（Laser / HellTower）共用的表现回放实现，子类 PlayCosmeticFire 直接转调它 */
	void PlayCosmeticBeam(const FRogueWeaponCosmeticFire& Fire);

	/** 武器配置数据 */
	FRogueWeaponTableRow Config;

private:
	UPROPERTY()
	TObjectPtr<ARogueCharacter> OwnerCharacter;
};
