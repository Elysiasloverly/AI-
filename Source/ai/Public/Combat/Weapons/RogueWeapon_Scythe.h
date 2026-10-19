// 镰刀武器子类
#pragma once

#include "CoreMinimal.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "RogueWeapon_Scythe.generated.h"

class ARogueOrbitingBlade;

UCLASS()
class AI_API ARogueWeapon_Scythe : public ARogueWeaponBase
{
	GENERATED_BODY()

public:
	virtual void WeaponTick(float DeltaSeconds) override;
	virtual void OnOwnerDied() override;

	/** 选择镰刀自己的数量加成；轨道更新是否执行由角色 TickWeapons 决定 */
	virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::ScytheCount; }
	virtual bool NeedsCosmeticTick() const override;

	/** 镰刀不配置开火能力，角色 TickWeapons 在各相关副本上维护持续轨道。
	 *  刀片有接触伤害请求；最终伤害仍由权威目标 ASC 结算。 */
	virtual bool UsesTargetedFireLoop() const override { return false; }

	// ---- 生效数值：镰刀的"攻速"表现为转速（+加成×280），范围表现为环绕半径（+加成×0.12）----
	float GetEffectiveRotationSpeed() const;
	float GetEffectiveOrbitRadius() const;

	/** 尺寸只影响刀片外观与伤害碰撞，不改变环绕距离。0.2 加成为 1.2 倍。 */
	UFUNCTION(BlueprintPure, Category = "Weapon|Scythe")
	float GetEffectiveSizeMultiplier() const;

private:
	void SyncOrbitingBlades();

	UPROPERTY()
	TArray<TObjectPtr<ARogueOrbitingBlade>> OrbitingBlades;

	float SharedOrbitAngle = 0.0f;

	/** 上一次推进相位时的**服务器时间**（详见 WeaponTick 里的说明）。
	 *  纯本地账本、各端各自维护，所以不是 UPROPERTY。
	 *  负值表示"还没建立时间基准"（第一次 tick 时只记录、不推进）。 */
	double LastOrbitServerTimeSeconds = -1.0;
};