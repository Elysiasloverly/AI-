#pragma once
#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/RogueBossSkillAbility.h"
#include "Enemies/Data/RogueBossAttackSettings.h"
#include "RogueBossAttackAbilities.generated.h"
class ARogueBossAttackActor;
class ARogueCharacter;
enum class ERogueBossAttackShape : uint8;

/** 共用发射与地面工具，不依赖任何怪物类型枚举；各招式独立实现。 */
UCLASS(Abstract)
class AI_API URogueBossAttackAbility:public URogueBossSkillAbility
{
    GENERATED_BODY()
protected:
    FRogueBossAttackSettings Tuning() const;
    FVector AimDirection() const;
    ARogueBossAttackActor* SpawnEffect(ERogueBossAttackShape Shape,const FVector& Origin,const FVector& Direction,bool bReleased=true,float Seconds=5);
    void EmitRing(int32 Count,float StartYaw);
    int32 GetOrdinaryRingCount() const;
    void HealOwner(float Amount);
};

UCLASS() class AI_API URogueBossOctagramAbility:public URogueBossAttackAbility
{
    GENERATED_BODY()
    FTimerHandle SpinTimer;
    float InitialYaw=0, SpinStartedAt=0, SpinSpeed=0;
    void Spin();
    void StopSpin();
protected:
    virtual void OnChargeStarted_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnSkillPulse_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnRecoveryStarted_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnSkillStopped_Implementation(bool bCancelled) override;
};
UCLASS() class AI_API URogueBossRollingSunsAbility:public URogueBossAttackAbility
{
    GENERATED_BODY()
    TArray<TWeakObjectPtr<ARogueBossAttackActor>> Balls;
    bool bLaunched=false;
    void PrepareBalls();
protected:
    virtual void OnChargeStarted_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnSkillPulse_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnSkillStopped_Implementation(bool bCancelled) override;
};
UCLASS() class AI_API URogueBossJudgmentRayAbility:public URogueBossAttackAbility
{
    GENERATED_BODY()
    TWeakObjectPtr<ARogueBossAttackActor> Beam;
protected:
    virtual bool IsSustainedEffect() const override { return true; }
    virtual void OnChargeStarted_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnSkillPulse_Implementation(const FRogueBossSkillState& Context) override;
};
UCLASS() class AI_API URogueBossLastBloomAbility:public URogueBossAttackAbility
{
    GENERATED_BODY()
    float StartingHealth=0;
protected:
    virtual void OnChargeStarted_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnRecoveryStarted_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnSkillPulse_Implementation(const FRogueBossSkillState& Context) override;
};
UCLASS() class AI_API URogueBossScarletRainAbility:public URogueBossAttackAbility
{
    GENERATED_BODY()
    TArray<FVector> SafeCenters,RecentImpacts;
protected:
    virtual void OnChargeStarted_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnSkillPulse_Implementation(const FRogueBossSkillState& Context) override;
};
UCLASS() class AI_API URogueBossFlameCrescentsAbility:public URogueBossAttackAbility
{ GENERATED_BODY() protected: virtual void OnSkillPulse_Implementation(const FRogueBossSkillState& Context) override; };
UCLASS() class AI_API URogueBossEndlessRingAbility:public URogueBossAttackAbility
{ GENERATED_BODY() protected: virtual void OnSkillPulse_Implementation(const FRogueBossSkillState& Context) override; };
UCLASS() class AI_API URogueBossDiamondReversalAbility:public URogueBossAttackAbility
{
    GENERATED_BODY()
    bool bGranted=false;
protected:
    virtual bool IsSustainedEffect() const override { return true; }
    virtual void OnSkillPulse_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnRecoveryStarted_Implementation(const FRogueBossSkillState& Context) override;
    virtual void OnSkillStopped_Implementation(bool bCancelled) override;
};
UCLASS() class AI_API URogueBossFrostPursuitAbility:public URogueBossAttackAbility
{ GENERATED_BODY() protected: virtual void OnSkillPulse_Implementation(const FRogueBossSkillState& Context) override; };
