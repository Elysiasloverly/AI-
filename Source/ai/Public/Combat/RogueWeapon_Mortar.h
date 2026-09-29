#pragma once

#include "CoreMinimal.h"
#include "Combat/RogueWeaponBase.h"
#include "RogueWeapon_Mortar.generated.h"

class ARogueEnemy;

UCLASS()
class AI_API ARogueWeapon_Mortar : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	virtual void WeaponTick(float DeltaSeconds) override;

	/** 队友客户端收到服务端广播后，照着补画这一发（纯表现迫击炮弹，伤害恒为 0） */
		virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::MortarCount; }
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire) override;

	// ---- 生效数值：迫击炮的换算系数与主武器/火箭都不同（沿用自研版本的行为）----

	/** 攻速：`max(0.7, 冷却 − 加成×2.0)`。 */
	virtual float GetEffectiveCooldown() const override;

	/** 弹速只吃 50% 的弹速加成。 */
	virtual float GetProjectileSpeedBonusScale() const override { return 0.5f; }

	/** 爆炸半径 = 基础半径 + 半径升级加成 + 范围加成×0.35
	 *  （旧实现里 MortarBlastRadius 加的是本地成员 ExplosionRadiusBonus，
	 *    现在它是玩家 ASC 上的 MortarBlastRadiusBonus 属性）。 */
	float GetExplosionRadius() const;

	/** 发射速度 = 迫击炮基础发射速度 + 弹速加成×0.5。 */
	float GetProjectileSpeed() const;

private:
	void FireMortarVolley(const TArray<ARogueEnemy*>& Enemies);

private:
	float FireTimer = 0.0f;
};
