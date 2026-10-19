#pragma once

#include "CoreMinimal.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "RogueWeapon_Mortar.generated.h"

class ARogueEnemy;

UCLASS()
class AI_API ARogueWeapon_Mortar : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	ARogueWeapon_Mortar();

	// ---- 能力驱动（见 ARogueWeaponBase 的"能力驱动"一节）----
	virtual void CollectFireTargets(TArray<ARogueEnemy*>& OutTargets) const override;
	virtual void FireAtTargets(const TArray<ARogueEnemy*>& Targets) override;

	virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::MortarCount; }
	/** 收到表现广播后在客户端补画迫击炮弹，不结算权威伤害。 */
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire) override;

	// ---- 生效数值：迫击炮的换算系数与主武器/火箭都不同----

	/** 攻速：`max(0.7, 冷却 − 加成×2.0)`。 */
	virtual float GetEffectiveCooldown() const override;

	/** 弹速只吃 50% 的弹速加成。 */
	virtual float GetProjectileSpeedBonusScale() const override { return 0.5f; }

	/** 基础爆炸半径 + MortarBlastRadius 加成 + AttackRange 加成×0.35。
	 *  MortarExplosionRadius 不为正时回退到 ExplosionRadius。 */
	float GetExplosionRadius() const;

	/** 发射速度 = 迫击炮基础发射速度 + 弹速加成×0.5。 */
	float GetProjectileSpeed() const;

private:
	void FireMortarVolley(const TArray<ARogueEnemy*>& Enemies);
};
