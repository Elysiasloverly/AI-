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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "镰刀", meta = (DisplayName = "环绕半径"))
	float OrbitRadius = 220.0f;

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

	// ---- 升级接口 ----

	/** 武器专属升级（数量 +1 等），返回 true 表示本武器消费了该升级 */
	virtual bool OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude);

	/** 全局伤害加成 */
	virtual void ApplySharedDamageBonus(float Magnitude);

	/** 全局攻速加成 */
	virtual void ApplySharedSpeedBonus(float Magnitude);

	/** 全局范围加成 */
	virtual void ApplySharedRangeBonus(float Magnitude);

	// ---- 查询接口 ----

	UFUNCTION(BlueprintPure)
	int32 GetCount() const { return Config.Count; }

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveCount() const { return FMath::Max(0, Config.Count); }

	UFUNCTION(BlueprintPure)
	bool IsWeaponActive() const { return GetEffectiveCount() > 0; }

	UFUNCTION(BlueprintPure)
	float GetDamage() const { return Config.Damage; }

	UFUNCTION(BlueprintPure)
	float GetCooldown() const { return Config.Cooldown; }

	UFUNCTION(BlueprintPure)
	float GetRange() const { return Config.Range; }

	UFUNCTION(BlueprintPure)
	const FString& GetDisplayName() const { return Config.DisplayName; }

	UFUNCTION(BlueprintPure)
	ERogueUpgradeType GetCountUpgradeType() const { return Config.CountUpgradeType; }

	/** 获取当前武器的完整配置（可读写） */
	FRogueWeaponTableRow& GetConfig() { return Config; }
	const FRogueWeaponTableRow& GetConfig() const { return Config; }

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

	/** 武器配置数据 */
	FRogueWeaponTableRow Config;

private:
	UPROPERTY()
	TObjectPtr<ARogueCharacter> OwnerCharacter;
};
