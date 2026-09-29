// 地狱塔武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/RogueWeaponBase.h"
#include "RogueWeapon_HellTower.generated.h"

class ARogueEnemy;

UCLASS()
class AI_API ARogueWeapon_HellTower : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	virtual void WeaponTick(float DeltaSeconds) override;
	virtual void OnOwnerDied() override;

	/** 队友客户端收到服务端广播后，把这束地狱塔光束再画一遍（不索敌、不结算伤害） */
		virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::HellTowerCount; }
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire) override;

	// ---- 生效数值：地狱塔的换算系数与其它武器都不同（沿用自研版本的行为）----
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