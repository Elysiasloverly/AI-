// 激光武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "RogueWeapon_Laser.generated.h"

class ARogueEnemy;

UCLASS()
class AI_API ARogueWeapon_Laser : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	ARogueWeapon_Laser();

	// ---- 能力驱动（见 ARogueWeaponBase 的"能力驱动"一节）----
	virtual void CollectFireTargets(TArray<ARogueEnemy*>& OutTargets) const override;
	virtual void FireAtTargets(const TArray<ARogueEnemy*>& Targets) override;

	virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::LaserCount; }
	/** 收到表现广播后在客户端补画光束，不结算权威伤害。 */
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire) override;

	// ---- 生效数值：激光的换算系数（攻速 ×2.0/下限 0.35、折射范围 ×0.45）由当前实现定义 ----

	/** 攻速：`max(0.35, 冷却 − 加成×2.0)`。 */
	virtual float GetEffectiveCooldown() const override;

	/** 折射范围 = Config.RefractionRange + 范围加成×0.45 */
	float GetEffectiveRefractionRange() const;

	/** 折射次数含 GAS 加成，下限 0，上限由 MaxRefractionCount 配置 */
	int32 GetRefractionCount() const { return GetEffectiveRefractionCount(); }

private:
	void FireLaserBurst(const TArray<ARogueEnemy*>& Enemies);
	void FireLaserRefractionChain(ARogueEnemy* InitialTarget, const FVector& InitialImpactLocation, float InitialDamage);
};
