// 地狱塔武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/RogueWeaponBase.h"
#include "RogueWeapon_HellTower.generated.h"

class ARogueEnemy;

UCLASS()
class AI_API ARogueWeapon_HellTower : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	virtual void WeaponTick(float DeltaSeconds) override;
	virtual void OnOwnerDied() override;
	virtual bool OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude) override;
	virtual void ApplySharedDamageBonus(float Magnitude) override;
	virtual void ApplySharedSpeedBonus(float Magnitude) override;
	virtual void ApplySharedRangeBonus(float Magnitude) override;

private:
	TArray<TWeakObjectPtr<ARogueEnemy>> HellTowerTargets;
	TArray<float> HellTowerCurrentDamages;
	TArray<float> HellTowerDamageTickTimers;
	TArray<float> HellTowerBeamTimers;
	float UpdateAccumulator = 0.0f;
};