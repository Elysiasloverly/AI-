#pragma once
#include "CoreMinimal.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "RogueWeapon_FrostBlade.generated.h"
class ARogueFrostBlade;
UCLASS()
class AI_API ARogueWeapon_FrostBlade:public ARogueWeaponBase
{
    GENERATED_BODY()
public:
    virtual bool NeedsCosmeticTick()const override{return true;}
    virtual bool UsesTargetedFireLoop()const override{return false;}
    virtual ERogueUpgradeType GetCountUpgradeType()const override{return ERogueUpgradeType::FrostBladeCount;}
    virtual void WeaponTick(float Delta)override;
    virtual void OnOwnerDied()override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
    virtual float GetEffectiveRange()const override;
    virtual float GetEffectiveDamage()const override;
    UFUNCTION(BlueprintPure,Category="冰刀") float GetEffectiveBladeScale()const;
    UFUNCTION(BlueprintPure,Category="冰刀") int32 GetEffectiveWaveCount()const;
    UFUNCTION(BlueprintPure,Category="冰刀") float GetEffectiveWaveScale()const;
    UFUNCTION(BlueprintPure,Category="冰刀") ARogueFrostBlade* GetBlade()const{return Blade;}
private:
    UPROPERTY() TObjectPtr<ARogueFrostBlade> Blade;
    float NextAttack=0;
};
