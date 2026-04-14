// 镰刀武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/RogueWeaponBase.h"
#include "RogueWeapon_Scythe.generated.h"

class ARogueOrbitingBlade;

UCLASS()
class AI_API ARogueWeapon_Scythe : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	virtual void WeaponTick(float DeltaSeconds) override;
	virtual void OnOwnerDied() override;
	virtual bool OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude) override;
	virtual void ApplySharedDamageBonus(float Magnitude) override;
	virtual void ApplySharedSpeedBonus(float Magnitude) override;
	virtual void ApplySharedRangeBonus(float Magnitude) override;

	float GetOrbitRadius() const { return GetConfig().OrbitRadius; }
	float GetRotationSpeed() const { return GetConfig().RotationSpeed; }

private:
	void SyncOrbitingBlades();

	UPROPERTY()
	TArray<TObjectPtr<ARogueOrbitingBlade>> OrbitingBlades;

	float SharedOrbitAngle = 0.0f;
};