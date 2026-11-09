#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueEnemyDefeatEffect.generated.h"
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/** Independent, local-only aftermath. Never keeps a pooled enemy or changes gameplay time. */
UCLASS(Blueprintable)
class AI_API ARogueEnemyDefeatEffect : public AActor
{
    GENERATED_BODY()
public:
    ARogueEnemyDefeatEffect();
    UFUNCTION(BlueprintCallable,Category="退治特效") void ActivateDefeat(FVector Location,bool bBoss,float Size=1.f);
    UFUNCTION(BlueprintCallable,Category="退治特效") void Deactivate();
    bool IsAvailableInPool()const{return bAvailable;}
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="退治特效") TObjectPtr<UInstancedStaticMeshComponent> Particles;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="退治特效",meta=(DisplayName="消散材质")) TObjectPtr<UMaterialInterface> DefeatMaterial;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="退治特效",meta=(ClampMin=".2",ClampMax="2",DisplayName="普通消散时长")) float Duration=.65f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="退治特效",meta=(ClampMin=".2",ClampMax="3",DisplayName="Boss消散时长")) float BossDuration=1.05f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="退治特效",meta=(ClampMin="0",ClampMax="16",DisplayName="符纸碎片数量")) int32 ShardCount=8;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="退治特效",meta=(DisplayName="普通消散颜色")) FLinearColor Tint=FLinearColor(.52f,.83f,1.f);
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="退治特效",meta=(DisplayName="Boss消散颜色")) FLinearColor BossTint=FLinearColor(1.f,.74f,.32f);
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="退治特效",meta=(ClampMin="1",ClampMax="4",DisplayName="Boss大小倍率")) float BossSize=2.1f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="退治特效|Boss",meta=(DisplayName="Boss退治镜头回弹")) bool bBossCameraKick=true;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="退治特效|Boss",meta=(DisplayName="镜头回弹强度",ClampMin="0",ClampMax="1",EditCondition="bBossCameraKick")) float BossCameraKickStrength=.55f;
private:
    void AddParticle(float Birth,float Life,int32 Kind,float Seed,FLinearColor Color,float Size,FVector2D Quad);
    FTimerHandle ReturnTimer;
    bool bAvailable=true;
};
