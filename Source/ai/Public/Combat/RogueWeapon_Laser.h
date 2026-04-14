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
	virtual bool OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude) override;
	virtual void ApplySharedDamageBonus(float Magnitude) override;
	virtual void ApplySharedSpeedBonus(float Magnitude) override;
	virtual void ApplySharedRangeBonus(float Magnitude) override;

	int32 GetRefractionCount() const { return GetConfig().RefractionCount; }

private:
	void FireLaserBurst(const TArray<ARogueEnemy*>& Enemies);
	void FireLaserRefractionChain(ARogueEnemy* InitialTarget, const FVector& InitialImpactLocation, float InitialDamage);

	float LaserTimer = 0.0f;
};