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

	// ---- 生效数值：地狱塔的换算系数与其它武器都不同----
	// 伤害只吃 12.5%（每跳递增吃 6.25%）、伤害跳间隔按 0.08 缩放、范围只吃 70%。

	/** 塔基础伤害 = Config.BaseDamage + 伤害加成×0.125 */
	float GetEffectiveBaseDamage() const;

	/** 每跳递增 = Config.DamageRampPerTick + 伤害加成×0.0625 */
	float GetEffectiveDamageRampPerTick() const;

	/** 伤害跳间隔 = max(0.03, Config.DamageTickInterval − 攻速加成×0.08) */
	float GetEffectiveDamageTickInterval() const;

	/** 索敌范围 = Config.Range + 范围加成×0.70 */
	virtual float GetEffectiveRange() const override;

private:
	TArray<TWeakObjectPtr<ARogueEnemy>> HellTowerTargets;
	TArray<float> HellTowerCurrentDamages;
	TArray<float> HellTowerDamageTickTimers;
	TArray<float> HellTowerBeamTimers;
	float UpdateAccumulator = 0.0f;
};