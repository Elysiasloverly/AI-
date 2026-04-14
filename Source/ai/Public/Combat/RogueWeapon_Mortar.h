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
