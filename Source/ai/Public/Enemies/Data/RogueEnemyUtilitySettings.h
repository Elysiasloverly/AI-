#pragma once
#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"
#include "RogueEnemyUtilitySettings.generated.h"

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyHealingSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="治疗范围",ClampMin="100"))
    float Radius=420;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="治疗间隔（秒）",ClampMin="0.25"))
    float Interval=1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="每次恢复目标最大生命比例",ClampMin="0",UIMax="1"))
    float HealFraction=0.025f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="对Boss治疗倍率",ClampMin="0",ClampMax="1"))
    float BossFactor=0.02f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="范围治疗最多目标数",ClampMin="1",ClampMax="16"))
    int32 MaxTargets=6;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(DisplayName="治疗需要无遮挡视线"))
    bool bRequireLineOfSight=false;
    FRogueEnemyHealingSettings Sanitized() const
    {
        FRogueEnemyHealingSettings S=*this;
        auto Safe=[](float V,float D,float A,float B){return FMath::Clamp(FMath::IsFinite(V)?V:D,A,B);};
        S.Radius=Safe(Radius,420,100,2000);S.Interval=Safe(Interval,1,.25f,10);
        S.HealFraction=FMath::IsFinite(HealFraction)?FMath::Max(0.f,HealFraction):.025f;
        S.BossFactor=Safe(BossFactor,.02f,0,1);S.MaxTargets=FMath::Clamp(MaxTargets,1,16);return S;
    }
};

/** 同一池周期中的外观档案；血量本身仍由ASC复制。 */
USTRUCT()
struct FRogueEnemyRuntimeProfile
{
    GENERATED_BODY()
    UPROPERTY() FRogueEnemyProfile Profile;
    UPROPERTY() float GroundZ=0;
    UPROPERTY() uint32 Generation=0;
};
