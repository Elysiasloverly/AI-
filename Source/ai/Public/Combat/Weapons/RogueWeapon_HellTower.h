// 地狱塔武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "RogueWeapon_HellTower.generated.h"

class ARogueEnemy;

UCLASS()
class AI_API ARogueWeapon_HellTower : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	ARogueWeapon_HellTower();

	/** 由能力每帧驱动（见 UsesTargetedFireLoop）：地狱塔维护自己的锁定列表，
	 *  每个目标各有独立的伤害跳与光束刷新间隔，所以它的"节拍"在武器内部，
	 *  能力只提供逐帧心跳。 */
	virtual void WeaponTick(float DeltaSeconds) override;
	virtual void OnOwnerDied() override;

	/** 地狱塔不是"目标选择 → 开火"型武器（它是持续锁定 + 伤害递增），
	 *  逐帧心跳直接进 WeaponTick，不走能力的目标选择那三步。 */
	virtual bool UsesTargetedFireLoop() const override { return false; }

	virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::HellTowerCount; }
	/** 收到表现广播后在客户端补画地狱塔光束，不结算权威伤害。 */
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire) override;
    virtual void UpdateCosmeticLock(const FRogueWeaponCosmeticFire& Fire) override;

	// ---- 生效数值：地狱塔的换算系数与其它武器都不同----
	// 各倍率读取本武器DT配置；伤害跳间隔最低0.03秒。

	/** 塔基础伤害 = Config.BaseDamage + 伤害加成×Config.BaseDamageBonusScale */
	float GetEffectiveBaseDamage() const;

	/** 每跳递增 = Config.DamageRampPerTick + 伤害加成×Config.DamageRampBonusScale + 专属递增加成 */
	float GetEffectiveDamageRampPerTick() const;

	/** 伤害跳间隔 = max(0.03, Config.DamageTickInterval − 攻速加成×Config.DamageTickIntervalReductionScale) */
	float GetEffectiveDamageTickInterval() const;

	/** 索敌范围 = Config.Range + 范围加成×Config.HellTowerRangeBonusScale */
	virtual float GetEffectiveRange() const override;

private:
    void ReleaseLock(ARogueEnemy* Target);
    void ReleaseAllLocks();
	TArray<TWeakObjectPtr<ARogueEnemy>> HellTowerTargets;
	// 同一Actor可在两次武器采样间回池再生；锁定必须属于同一次生成。
	TArray<FGuid> HellTowerTargetSpawnIds;
	TArray<float> HellTowerCurrentDamages;
	TArray<float> HellTowerDamageTickTimers;
	TArray<float> HellTowerBeamTimers;
    TArray<float> HellTowerLockAges;
	float UpdateAccumulator = 0.0f;
};
