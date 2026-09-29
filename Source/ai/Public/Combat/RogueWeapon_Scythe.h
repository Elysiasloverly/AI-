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

	/** 持续轨道表现：没有"某一发"可广播，任何机器都得自己 tick（详见基类说明） */
		virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::ScytheCount; }
	virtual bool NeedsCosmeticTick() const override;

	// ---- 生效数值：镰刀的"攻速"表现为转速（+加成×280），范围表现为环绕半径（+加成×0.12）----
	float GetEffectiveRotationSpeed() const;
	float GetEffectiveOrbitRadius() const;

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