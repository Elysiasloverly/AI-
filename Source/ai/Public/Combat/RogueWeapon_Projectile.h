// 弹体武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/RogueWeaponBase.h"
#include "RogueWeapon_Projectile.generated.h"

class ARogueProjectile;

UCLASS()
class AI_API ARogueWeapon_Projectile : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	virtual void WeaponTick(float DeltaSeconds) override;
	virtual bool OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude) override;
	virtual void ApplySharedDamageBonus(float Magnitude) override;
	virtual void ApplySharedSpeedBonus(float Magnitude) override;

	int32 GetEffectiveProjectileCount() const { return FMath::Max(0, GetConfig().Count); }

	float GetProjectileSpeed() const { return GetConfig().ProjectileSpeed; }
	float GetInterval() const { return GetConfig().Cooldown; }

private:
	void FireAtTarget(AActor* TargetActor);

	float AttackTimer = 0.0f;
};
