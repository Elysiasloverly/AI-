// 武器系统基类 —— 所有武器的公共接口与 DataTable 行结构体
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Shared/RogueTypes.h"
#include "Audio/RogueAttackAudioSettings.h"
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
class URogueWeaponAbility;
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

	// 仅用于一次性迁移旧数据；运行时统一读取 DA_AudioConfig，不再显示于武器配置。
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="请在 DA_AudioConfig 配置武器音效"))
	FRogueAttackAudioSettings AttackAudio;

	/** 对应的"数量升级"枚举类型 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "升级", meta = (DisplayName = "数量升级类型"))
	ERogueUpgradeType CountUpgradeType = ERogueUpgradeType::ProjectileCount;

	// 最终值限制：通常在合并基础值与加成后应用；负值表示不限制，0 是有效限制。
	// 派生武器也统一从本行读取公式系数，不再在 C++ 里写死冷却/范围下限。

	/** 冷却下限（秒），默认 0.01；负值不设配置下限，但结果仍不小于 0。
	 *  Laser/Rocket/Mortar 等派生武器统一读取本字段；0 冷却可使开火循环每次更新都尝试开火。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "上限", meta = (DisplayName = "冷却下限(秒，-1=不限)"))
	float MinCooldown = 0.01f;

	/** 数量上限（**-1 = 不限制**，默认）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "上限", meta = (DisplayName = "数量上限(-1=不限)"))
	int32 MaxCount = -1;

	/** 伤害上限（**-1 = 不限制**，默认）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "上限", meta = (DisplayName = "伤害上限(-1=不限)"))
	float MaxDamage = -1.0f;

	/** 索敌范围上限（**-1 = 不限制**，默认）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "上限", meta = (DisplayName = "范围上限(-1=不限)"))
	float MaxRange = -1.0f;

	/** 弹速上限（**-1 = 不限制**，默认）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "上限", meta = (DisplayName = "弹速上限(-1=不限)"))
	float MaxProjectileSpeed = -1.0f;

	/** 激光折射次数上限（**-1 = 不限制**，默认）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "上限", meta = (DisplayName = "折射上限(-1=不限)"))
	int32 MaxRefractionCount = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "御札连发", meta = (DisplayName = "连发间隔(秒)", ClampMin = "0.02"))
	float ProjectileBurstInterval = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "御札连发", meta = (DisplayName = "连发次数上限(含首发，-1=不限)"))
	int32 MaxProjectileBurstCount = -1;

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

	// ---- 派生武器公式系数（全部从 DataTable 读取） ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "攻速转冷却倍率"))
	float CooldownReductionScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "折射范围加成倍率"))
	float RefractionRangeBonusScale = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "折射伤害倍率"))
	float RefractionDamageScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "火箭索敌额外范围"))
	float RocketSeekRangeBonus = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "火箭爆炸半径范围加成倍率"))
	float RocketExplosionRadiusRangeBonusScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "迫击炮爆炸半径范围加成倍率"))
	float MortarExplosionRadiusRangeBonusScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "地狱塔范围加成倍率"))
	float HellTowerRangeBonusScale = 0.70f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "地狱塔光束寿命补偿"))
	float BeamLifetimePadding = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "多重射击角度步长"))
	float SpreadStepDegrees = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "地狱塔基础伤害加成倍率"))
	float BaseDamageBonusScale = 0.125f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "地狱塔每跳伤害加成倍率"))
	float DamageRampBonusScale = 0.0625f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "公式", meta = (DisplayName = "地狱塔伤害间隔缩减倍率"))
	float DamageTickIntervalReductionScale = 0.08f;

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

	/** 刀片轨道的基础半径。实际接触范围还取决于刀片/敌人的碰撞形状。
	 *  已保存的数据表行会覆盖这里的结构体默认值；调参时需检查实际行值。 */
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "光束", meta = (DisplayName = "光束蓝图类", ToolTip = "激光与地狱塔可分别指定光束蓝图；留空使用原生默认光束。"))
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

	/** 由 Character 每帧调用 —— **只给"没有某一发可广播"的持续型表现武器用**
	 *  （目前只有环绕刀片；见 NeedsCosmeticTick）。
	 *  事件型武器的开火已经全部搬进 GameplayAbility（见下面 Ability 一节），
	 *  它们的逐帧节拍由 URogueAbilityTask_WeaponFireLoop 驱动。 */
	virtual void WeaponTick(float DeltaSeconds) PURE_VIRTUAL(ARogueWeaponBase::WeaponTick, );

	/** 角色死亡时调用，子类可重写做清理 */
	virtual void OnOwnerDied();

	// ---- 能力驱动（GameplayAbility 侧要问的问题）----
	//
	// 分工：**能力决定"什么时候能开火"，武器决定"这一发是什么"**。
	// 所以能力只会问下面这几个问题，而不会碰伤害/弹速/散射那些细节。

	/** 构造函数指定的开火能力类。nullptr 表示不授予开火能力；
	 *  镰刀通过角色 TickWeapons 维护轨道与接触判定，伤害入口仍只允许权威结算。 */
	virtual TSubclassOf<URogueWeaponAbility> GetFireAbilityClass() const { return FireAbilityClass; }

	/** 是否"未解锁就不开火"。默认要求生效数量 > 0。 */
	virtual bool CanFireNow() const { return GetEffectiveCount() > 0; }

	/** 开火节拍由**目标选择**驱动（事件型武器），还是武器自己内部的持续模拟（地狱塔）。
	 *
	 *  · true （默认，子弹/火箭/迫击炮/激光）：能力每帧问一次"有目标吗"，有就开一发。
	 *  · false（地狱塔）：它维护自己的锁定列表，每个目标各有独立的伤害跳与光束刷新间隔，
	 *    没有"某一发"这种事件。能力只提供逐帧心跳，节拍在武器内部。
	 */
	virtual bool UsesTargetedFireLoop() const { return true; }

	/** 目标选择：把这一发要打的敌人填进 OutTargets。
	 *  返回空数组 = 此次不开火、不提交开火冷却，后续更新继续查询。 */
	virtual void CollectFireTargets(TArray<ARogueEnemy*>& OutTargets) const;

	/** 开火：对着选出来的目标打出这一发（池化弹体、散射、广播都在这里）。 */
	virtual void FireAtTargets(const TArray<ARogueEnemy*>& Targets);

	/** 这一发之后要冷却多久（秒）—— 生效冷却，各武器有自己的换算系数与下限。 */
	virtual float GetAbilityCooldown() const { return GetEffectiveCooldown(); }

	// ---- 联机表现：本人预测 + 服务端广播给别人 ----

	/**
	 * 服务端：把"我刚开了这一发"广播给其他客户端。
	 *
	 * 调用点应该在**权威那一发已经生成之后**（服务端自己有一份真实体，
	 * 本人客户端自己预测了一份），所以广播只用于补上"队友那一屏"。
	 * 在客户端调用它什么也不会发生（客户端本来就是接收方）。
	 */
	void BroadcastCosmeticFire(const FRogueWeaponCosmeticFire& Fire) const;

	/** 仅本地播放。事件型武器由服务器确认的开火批次触发；环绕刀片按本地转圈触发。 */
	void PlayAttackAudio(const FVector& Location);

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

	// 升级写入玩家 ASC 属性，角色将武器加成复制为快照；武器合并基础配置与快照。
	// 新武器在数据表定义基础值，并按需重写 GetEffective* 的换算。

	// ---- 查询接口 ----

	UFUNCTION(BlueprintPure)
	int32 GetCount() const { return Config.Count; }

	/** 合并基础数量与本武器数量加成，再应用 MaxCount；未解锁且无自身加成时为 0。 */
	UFUNCTION(BlueprintPure)
	int32 GetEffectiveCount() const;

	UFUNCTION(BlueprintPure)
	float GetDamage() const { return Config.Damage; }

	UFUNCTION(BlueprintPure)
	float GetRange() const { return Config.Range; }

	// 生效值合并数据表基础配置与角色的 GAS 加成快照，并按函数应用限制。
	// 各武器可以重写换算系数；伤害、冷却和范围的具体公式以对应实现为准。

	/** 单发伤害合并 Config.Damage 与 AttackPower 加成，再应用 MaxDamage。 */
	virtual float GetEffectiveDamage() const;

	/** 基类攻击间隔：Config.Cooldown − AttackSpeed，应用配置 MinCooldown（最低为 0）。 */
	virtual float GetEffectiveCooldown() const;

	/** 基类索敌范围合并 Config.Range 与 AttackRange 加成，再应用 MaxRange。 */
	virtual float GetEffectiveRange() const;

	/** 弹速 = BaseSpeed + ProjectileSpeedBonus × GetProjectileSpeedBonusScale()
	 *  @param BaseSpeed 该武器自己的基础弹速（主武器 ProjectileSpeed / 火箭 RocketSpeed / 迫击炮 MortarLaunchSpeed） */
	float GetEffectiveProjectileSpeed(float BaseSpeed) const;

	/** 弹速加成系数：默认全额（1.0），火箭 0.75、迫击炮 0.5。 */
	virtual float GetProjectileSpeedBonusScale() const { return 1.0f; }

	/** 决定读取哪一项数量加成；现有子类显式返回各自类型，基类用 Config.CountUpgradeType。 */
	virtual ERogueUpgradeType GetCountUpgradeType() const { return Config.CountUpgradeType; }

	/** 折射次数合并基础值与四舍五入后的 LaserRefraction 加成，下限 0，上限由 MaxRefractionCount 配置。 */
	int32 GetEffectiveRefractionCount() const;

	/** 获取当前武器的完整配置（可读写） */
	FRogueWeaponTableRow& GetConfig() { return Config; }
	const FRogueWeaponTableRow& GetConfig() const { return Config; }

	/** 返回拥有者 Character 的武器加成快照；服务端从 GAS 属性生成并复制。
	 *  客户端可能因网络延迟读到旧快照；无角色时返回全 0 的静态值。 */
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
	void SpawnLaserBeam(const FVector& Start, const FVector& End, bool bInfernoStyle = false, bool bImpactEffect = true, float Lifetime = 0.10f, bool bPlayAttackSound = false);

	/** 光束类武器（Laser / HellTower）共用的表现回放实现，子类 PlayCosmeticFire 直接转调它 */
	void PlayCosmeticBeam(const FRogueWeaponCosmeticFire& Fire);

	/** 武器配置数据 */
	FRogueWeaponTableRow Config;

	/** 开火能力类 —— 子类在构造里指定（见 GetFireAbilityClass）。 */
	UPROPERTY(EditDefaultsOnly, Category = "技能")
	TSubclassOf<URogueWeaponAbility> FireAbilityClass;

private:
	FRogueAttackAudioState AttackAudioState;
	UPROPERTY()
	TObjectPtr<ARogueCharacter> OwnerCharacter;
};
