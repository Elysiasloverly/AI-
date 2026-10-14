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

	/** 持续轨道表现：没有"某一发"可广播，任何机器都得自己 tick（详见基类说明） */
	virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::ScytheCount; }
	virtual bool NeedsCosmeticTick() const override;

	/** 镰刀**不参与能力驱动**：它的表现是一条连续轨道，在**所有**机器上都要画
	 *  （包括"既非服务端也非本人"的队友客户端），而技能列表只复制给本人
	 *  （ASC 的 ActivatableAbilities 是 COND_OwnerOnly）—— 那些机器上根本没有这份能力。
	 *  所以它继续留在 ARogueCharacter::TickWeapons 那条"纯表现"路径上，
	 *  GetFireAbilityClass() 返回 nullptr（基类默认）。 */
	virtual bool UsesTargetedFireLoop() const override { return false; }

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