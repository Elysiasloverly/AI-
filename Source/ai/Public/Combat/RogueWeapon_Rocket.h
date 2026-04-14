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
	virtual void WeaponTick(float DeltaSeconds) override;
	virtual bool OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude) override;
	virtual void ApplySharedDamageBonus(float Magnitude) override;
	virtual void ApplySharedSpeedBonus(float Magnitude) override;
	virtual void ApplySharedRangeBonus(float Magnitude) override;

	float GetExplosionRadius() const { return GetConfig().ExplosionRadius; }
	float GetRocketSpeed() const { return GetConfig().RocketSpeed; }

private:
	void FireRocketVolley(const TArray<ARogueEnemy*>& Enemies);

	float RocketTimer = 0.0f;
};