#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RogueEnemyBulletSubsystem.generated.h"

class ARogueEnemyProjectile;

/** 普通直线敌方弹幕的本 World 调度器。复用 Actor 的状态、伤害与蓝图外观，集中推进运动。 */
UCLASS()
class AI_API URogueEnemyBulletSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    void RegisterProjectile(ARogueEnemyProjectile* Projectile);
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;
    int32 GetTrackedCount() const { return Active.Num()+Pending.Num(); }
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
    struct FEntry
    {
        TWeakObjectPtr<ARogueEnemyProjectile> Projectile;
        uint64 Generation=0;
        bool IsLive() const;
    };
    TArray<FEntry> Active;
    TArray<FEntry> Pending;
    TArray<FHitResult> ScratchHits;
    bool bUpdating=false;
};
