// 火箭武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/Weapons/RogueWeaponBase.h"
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

	virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::RocketCount; }
	/** 收到表现广播后在客户端补画火箭，不结算权威伤害。 */
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire) override;

	// ---- 生效数值：火箭的换算系数与主武器不同----

	/** 冷却、攻速换算系数和最低间隔都读取当前武器表行。 */
	virtual float GetEffectiveCooldown() const override;

	/** 弹速只吃 75% 的弹速加成。 */
	virtual float GetProjectileSpeedBonusScale() const override { return Config.ProjectileSpeedBonusScale; }

	/** 爆炸半径 = 表内基础半径 + 通用范围加成×表内系数 + 冰焰扩散加成。 */
	float GetExplosionRadius() const;

private:
	void FireRocketVolley(const TArray<ARogueEnemy*>& Enemies);
};
