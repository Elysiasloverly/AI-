// 激光武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/RogueWeaponBase.h"
#include "RogueWeapon_Laser.generated.h"

class ARogueEnemy;

UCLASS()
class AI_API ARogueWeapon_Laser : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	virtual void WeaponTick(float DeltaSeconds) override;

	/** 队友客户端收到服务端广播后，把这条光束再画一遍（不索敌、不结算伤害） */
		virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::LaserCount; }
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire) override;

	// ---- 生效数值：激光的换算系数（攻速 ×2.0/下限 0.35、折射范围 ×0.45）沿用自研版本 ----

	/** 攻速：`max(0.35, 冷却 − 加成×2.0)`。 */
	virtual float GetEffectiveCooldown() const override;

	/** 折射范围 = Config.RefractionRange + 范围加成×0.45 */
	float GetEffectiveRefractionRange() const;

	/** 折射次数（含 GAS 加成，夹在 0..8；旧实现也是这个范围） */
	int32 GetRefractionCount() const { return GetEffectiveRefractionCount(); }

private:
	void FireLaserBurst(const TArray<ARogueEnemy*>& Enemies);
	void FireLaserRefractionChain(ARogueEnemy* InitialTarget, const FVector& InitialImpactLocation, float InitialDamage);

	float LaserTimer = 0.0f;
};