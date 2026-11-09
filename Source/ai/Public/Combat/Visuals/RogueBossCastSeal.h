#pragma once

#include "CoreMinimal.h"
#include "Enemies/Data/RogueBossSkillTypes.h"
#include "GameFramework/Actor.h"
#include "RogueBossCastSeal.generated.h"

class ARogueEnemy;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** 从 Boss 已同步的施法阶段驱动，每端只维护一个本地符阵。 */
UCLASS(NotBlueprintable)
class AI_API ARogueBossCastSeal : public AActor
{
    GENERATED_BODY()

public:
    ARogueBossCastSeal();
    void ApplyState(const FRogueBossSkillState& State, UMaterialInterface* Material);
    void HideSeal();
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Seal;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Crown;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SealMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CrownMaterial;
    UPROPERTY() FRogueBossSkillState CastState;
    FVector Ground = FVector::ZeroVector;
    float Spread = 0.f;
    float CastRadius = 220.f;
    float StartServerTime = 0.f;
    int32 Style = 0;
    bool bHasGround = false;
    float ServerTime() const;
};
