#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "RogueEnemyPresentationComponent.generated.h"
class ARogueEnemyEffectVisual;
class UMeshComponent;
class UMaterialInterface;
UCLASS(ClassGroup=(Enemy),meta=(BlueprintSpawnableComponent,DisplayName="敌人辨识表现"))
class AI_API URogueEnemyPresentationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URogueEnemyPresentationComponent();
    void RefreshRuntime();
    void ResetForPool();
    /** Cosmetic only; existing attack/damage feedback transports these cues. */
    UFUNCTION(BlueprintCallable,Category="表现|动作") void PlayAttackCue();
    UFUNCTION(BlueprintCallable,Category="表现|动作") void PlayHitCue();
    virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Fn) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Boss",meta=(DisplayName="Boss模型附加倍率",ClampMin="1",ClampMax="3"))
    float BossModelScale=1.5f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Boss", meta=(DisplayName="Boss 辉纹覆盖材质"))
    TObjectPtr<UMaterialInterface> BossSurfaceMaterial;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Boss",meta=(DisplayName="Boss光环与头顶标记蓝图"))
    TSubclassOf<ARogueEnemyEffectVisual> BossVisualClass;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="飞行",meta=(DisplayName="飞行敌人地面标记蓝图"))
    TSubclassOf<ARogueEnemyEffectVisual> FlyingVisualClass;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="动作",meta=(DisplayName="启用模型动作"))
    bool bEnableVisualMotion=true;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="动作",meta=(DisplayName="移动前倾角度",ClampMin="0",ClampMax="15"))
    float MovementLeanDegrees=6.f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="动作",meta=(DisplayName="转弯侧倾角度",ClampMin="0",ClampMax="12"))
    float TurnBankDegrees=4.f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="动作",meta=(DisplayName="移动起伏高度",ClampMin="0",ClampMax="6",Units="cm"))
    float MovementBobHeight=1.8f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="动作",meta=(DisplayName="攻击后坐距离",ClampMin="0",ClampMax="10",Units="cm"))
    float AttackRecoilDistance=4.f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="动作",meta=(DisplayName="受击摆动角度",ClampMin="0",ClampMax="10"))
    float HitSwayDegrees=3.5f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="动作",meta=(DisplayName="动作显示距离",ClampMin="1000",ClampMax="10000",Units="cm"))
    float MotionDrawDistance=5000.f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Boss",meta=(DisplayName="Boss施法碎光")) FRogueCombatAccentSettings BossCastAccent;
private:
    float LanternLift = 0.f;
    struct FScale { TWeakObjectPtr<UMeshComponent> Mesh;FVector Value; };
    TArray<FScale> Scales;
    struct FBossSurfaceSnapshot
    {
        TWeakObjectPtr<UMeshComponent> Mesh;
        TStrongObjectPtr<UMaterialInterface> PreviousOverlay;
        bool bForcedRaster = false;
    };
    TArray<FBossSurfaceSnapshot> BossSurfaces;
    void ApplyBossSurfaces();
    void RestoreBossSurfaces();
    UPROPERTY(Transient) TObjectPtr<ARogueEnemyEffectVisual> BossVisual;
    UPROPERTY(Transient) TObjectPtr<ARogueEnemyEffectVisual> FlyingVisual;
    uint32 Generation=MAX_uint32;
    bool bScaled=false;
    double LastBossCastTime=-1.e30;
    struct FVisualPose { TWeakObjectPtr<UMeshComponent> Mesh;FVector Location;FQuat Rotation; };
    TArray<FVisualPose> Poses;
    FVector SmoothedVelocity=FVector::ZeroVector;
    float PreviousYaw=0,SmoothedTurn=0,MotionPhase=0;
    float AttackAge=10.f,HitAge=10.f,HitSign=1.f;
    bool bPoseApplied=false;
    void CacheVisualRoots();
    void RestoreVisualPose();
    void UpdateVisualMotion(float Delta);
    void SetCrowdShade(bool bEnable);
    bool bCrowdShaded = false;
    bool bSkyfallPose = false;
    ARogueEnemyEffectVisual* EnsureVisual(TObjectPtr<ARogueEnemyEffectVisual>& Actor,TSubclassOf<ARogueEnemyEffectVisual> Class);
};
