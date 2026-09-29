// 火箭武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/RogueWeaponBase.h"
#include "RogueWeapon_Rocket.generated.h"

class ARogueRocketProjectile;
class ARogueEnemy;

UCLASS()
class AI_API ARogueWeapon_Rocket : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	ARogueWeapon_Rocket();

	// ---- 能力驱动（见 ARogueWeaponBase 的"能力驱动"一节）----
	virtual void CollectFireTargets(TArray<ARogueEnemy*>& OutTargets) const override;
	virtual void FireAtTargets(const TArray<ARogueEnemy*>& Targets) override;

	/** 队友客户端收到服务端广播后，照着补画这一发（纯表现火箭，伤害恒为 0） */
	virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::RocketCount; }
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire) override;

	// ---- 生效数值：火箭的换算系数与主武器不同（沿用自研版本的行为，必须一致）----

	/** 攻速：`max(0.55, 冷却 − 加成×2.5)` —— 系数 2.5、下限 0.55 都与主武器不同。 */
	virtual float GetEffectiveCooldown() const override;

	/** 弹速只吃 75% 的弹速加成。 */
	virtual float GetProjectileSpeedBonusScale() const override { return 0.75f; }

	/** 爆炸半径：基础半径 + 范围加成×0.25（旧实现对 ExplosionRadius 也是这么加的）。 */
	float GetExplosionRadius() const;

private:
	void FireRocketVolley(const TArray<ARogueEnemy*>& Enemies);
};
