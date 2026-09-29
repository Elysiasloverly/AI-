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
	virtual bool OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude) override;
	virtual void ApplySharedDamageBonus(float Magnitude) override;
	virtual void ApplySharedSpeedBonus(float Magnitude) override;
	virtual void ApplySharedRangeBonus(float Magnitude) override;

	/** 队友客户端收到服务端广播后，照着补画这一发（纯表现迫击炮弹，伤害恒为 0） */
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire) override;
	
	float GetExplosionRadius() const
	{
		const FRogueWeaponTableRow& WeaponConfig = GetConfig();
		const float BaseRadius = WeaponConfig.MortarExplosionRadius > 0.0f ? WeaponConfig.MortarExplosionRadius : WeaponConfig.ExplosionRadius;
		return BaseRadius + ExplosionRadiusBonus;
	}

	float GetProjectileSpeed() const
	{
		const FRogueWeaponTableRow& WeaponConfig = GetConfig();
		return WeaponConfig.MortarLaunchSpeed > 0.0f ? WeaponConfig.MortarLaunchSpeed : WeaponConfig.RocketSpeed;
	}

private:
	void FireMortarVolley(const TArray<ARogueEnemy*>& Enemies);

private:
	float FireTimer = 0.0f;
	float ExplosionRadiusBonus = 0.0f;
};
