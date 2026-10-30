#pragma once
#include "CoreMinimal.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "RogueWeapon_Talisman.generated.h"

class ARogueReturningTalisman;

/** 持续实体由服务器维护，独立复制给所有玩家；不另生成本地预测副本。 */
UCLASS()
class AI_API ARogueWeapon_Talisman : public ARogueWeaponBase
{
    GENERATED_BODY()
public:
    virtual void WeaponTick(float DeltaSeconds) override;
    virtual void OnOwnerDied() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual bool NeedsCosmeticTick() const override { return true; }
    virtual bool UsesTargetedFireLoop() const override { return false; }
    virtual ERogueUpgradeType GetCountUpgradeType() const override { return ERogueUpgradeType::TalismanCount; }
    UFUNCTION(BlueprintPure, Category="灵符") int32 GetEffectiveTargetCount() const;
    UFUNCTION(BlueprintPure, Category="灵符") int32 GetEffectiveHitsPerTarget() const;
private:
    UPROPERTY() TArray<TObjectPtr<ARogueReturningTalisman>> Talismans;
};
