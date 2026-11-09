#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "Enemies/Data/RogueBossSkillTypes.h"
#include "RogueBossSkillComponent.generated.h"

class ARogueEnemy;
class ARogueBossCastSeal;
class UMaterialInterface;
class URogueBossSkillAbility;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRogueBossSkillPresentation, const FRogueBossSkillState&, State);

/** 只在 Boss 的服务器实例调度。普通攻击仍走原通路；每次池化生命周期重新授予技能。 */
UCLASS(ClassGroup=(Enemy),meta=(BlueprintSpawnableComponent,DisplayName="Boss 技能插槽"))
class AI_API URogueBossSkillComponent : public UActorComponent
{
    GENERATED_BODY()
    friend struct FRogueBossSkillPreviewAccess;
public:
    URogueBossSkillComponent();
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Boss技能",meta=(DisplayName="技能插槽",RowType="/Script/ai.RogueBossSkillRow")) TArray<FDataTableRowHandle> SkillSlots;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Boss技能",meta=(DisplayName="随机技能目录")) TObjectPtr<UDataTable> SkillCatalog;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Boss技能",meta=(DisplayName="抽取规则")) FRogueBossSkillSelection Selection;
    const TArray<FDataTableRowHandle>& GetSelectedSkills() const { return SelectedSkills; }
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Boss技能",meta=(DisplayName="自动释放技能")) bool bAutoUseSkills=true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss技能|表现", meta=(DisplayName="蓄力符阵材质"))
    TObjectPtr<UMaterialInterface> CastSealMaterial;
    UPROPERTY(BlueprintAssignable,Category="Boss技能|表现") FRogueBossSkillPresentation OnPresentationChanged;
    /** 短暂发射表现：不可靠多播，可能丢失；真实弹体/伤害由技能自己的权威通路处理。 */
    UPROPERTY(BlueprintAssignable,Category="Boss技能|表现") FRogueBossSkillPresentation OnPulseCosmetic;

    void InitializeRuntime();
    void ResetForPool();
    void RefreshPresentation();
    UFUNCTION(BlueprintCallable,BlueprintAuthorityOnly,Category="Boss技能") bool TryStartSkill(int32 SlotIndex);
    UFUNCTION(BlueprintCallable,BlueprintAuthorityOnly,Category="Boss技能") void CancelCurrentSkill();
    UFUNCTION(BlueprintPure,Category="Boss技能") const FRogueBossSkillState& GetSkillState() const { return State; }
    UFUNCTION(BlueprintPure,Category="Boss技能") float GetSkillCooldownRemaining(int32 SlotIndex) const;
    UFUNCTION(BlueprintPure,Category="Boss技能") float GetPhaseRemaining() const;
    UFUNCTION(BlueprintPure,Category="Boss技能") bool IsCasting() const { return State.Phase!=ERogueBossSkillPhase::Idle; }
    bool SuppressesNormalAttacks() const;
    bool SuppressesMovement() const;
    bool IsOwnerReady() const;
    bool IsActivationAllowed(FGameplayAbilitySpecHandle Handle) const;
    bool AttachExecution(URogueBossSkillAbility* Ability,FGameplayAbilitySpecHandle Handle);
    const FRogueBossSkillRow& GetActiveSettings() const { return ActiveSettings; }
    bool IsExecutionCurrent(const URogueBossSkillAbility* Ability,int32 CastId) const;
    void PublishPhase(ERogueBossSkillPhase Phase,float Duration,int32 PulseIndex);
    void NotifyExecutionEnded(URogueBossSkillAbility* Ability,bool bCancelled);
    virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Tick) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    struct FRuntimeSlot { FDataTableRowHandle Row; FRogueBossSkillRow Settings; FGameplayAbilitySpecHandle Handle; double ReadyAt=0; int32 Uses=0; };
    TArray<FRuntimeSlot> RuntimeSlots;
    UPROPERTY() TArray<FDataTableRowHandle> SelectedSkills;
    int32 ActiveSlot=INDEX_NONE;
    int32 NextCastId=0;
    FRogueBossSkillRow ActiveSettings;
    TWeakObjectPtr<URogueBossSkillAbility> ActiveAbility;
    FRogueBossSkillState LastPresented;
    bool bHadPresentation=false;
    bool bResetting=false;
    uint32 LastPulseGeneration=0;
    int32 LastPulseCast=0,LastPulseIndex=0;
    UPROPERTY(Transient) TObjectPtr<ARogueBossCastSeal> CastSeal;
    void UpdateCastSeal(const FRogueBossSkillState& Visible);
    float ServerTime() const;
    bool IsSlotReady(int32 Index) const;
    bool MeetsSlotConditions(int32 Index) const;
    UPROPERTY(ReplicatedUsing=OnRep_State) FRogueBossSkillState State;
    UFUNCTION() void OnRep_State();
    UFUNCTION(NetMulticast,Unreliable) void Multicast_Pulse(const FRogueBossSkillState& Pulse);
};
