// 弹体武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "RogueWeapon_Projectile.generated.h"

class ARogueEnemy;
class ARogueProjectile;

UCLASS()
class AI_API ARogueWeapon_Projectile : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	ARogueWeapon_Projectile();

	// ---- 能力驱动（见 ARogueWeaponBase 的"能力驱动"一节）----
	virtual void CollectFireTargets(TArray<ARogueEnemy*>& OutTargets) const override;
	virtual void FireAtTargets(const TArray<ARogueEnemy*>& Targets) override;

	/** 队友客户端收到服务端广播后，照着补画这一发（纯表现弹体，伤害恒为 0） */
	virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::ProjectileCount; }
	virtual void PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire) override;

	// ---- 对外查询：全部是"数据表基础值 + GAS 加成"的生效值 ----
	// 升级不再改这里的配置，所以这些取值必须走 GetEffective*（HUD 与升级规则都读它们）。

	int32 GetEffectiveProjectileCount() const { return GetEffectiveCount(); }

	float GetProjectileSpeed() const { return GetEffectiveProjectileSpeed(GetConfig().ProjectileSpeed); }
	float GetInterval() const { return GetEffectiveCooldown(); }

private:
	void FireAtTarget(AActor* TargetActor);
};
