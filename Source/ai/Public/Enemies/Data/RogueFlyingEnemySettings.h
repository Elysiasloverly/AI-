#pragma once
#include "CoreMinimal.h"
#include "RogueFlyingEnemySettings.generated.h"

/** 飞行类型共用的可编辑参数；只在月蝶、风镰、星灯的表行使用。单位为厘米和秒。 */
USTRUCT(BlueprintType)
struct AI_API FRogueFlyingEnemySettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="离地附加高度", ClampMin="0", ClampMax="120"))
    float HoverHeight = 80;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="上下浮动幅度", ClampMin="0", ClampMax="20"))
    float BobAmplitude = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="攻击周期", ClampMin="1"))
    float AttackCooldown = 3.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="弹速", ClampMin="100", ClampMax="2000"))
    float ProjectileSpeed = 650;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="单弹伤害倍率", ClampMin="0"))
    float DamageMultiplier = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="风镰/星阵子弹数", ClampMin="3", ClampMax="16"))
    int32 ShotCount = 9;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="月蝶扫射组数", ClampMin="2", ClampMax="10"))
    int32 VolleyCount = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="月蝶组间隔", ClampMin="0.08"))
    float VolleyInterval = 0.18f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="扫射/风刃角度步长", ClampMin="1", ClampMax="40"))
    float AngleStep = 18;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="风刃折返前飞行时间", ClampMin="0.3", ClampMax="2"))
    float ReturnAfter = 0.9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="星阵预警时间（黑夜不缩短）", ClampMin="0.6", ClampMax="3"))
    float WarningSeconds = 0.9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="星阵半径", ClampMin="280", ClampMax="700"))
    float TrapRadius = 420;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="星阵逃生缺口角度", ClampMin="75", ClampMax="160"))
    float GapDegrees = 100;

    FRogueFlyingEnemySettings Sanitized() const
    {
        FRogueFlyingEnemySettings R=*this;
        auto Safe=[](float V,float D,float Min,float Max){return FMath::Clamp(FMath::IsFinite(V)?V:D,Min,Max);};
        R.HoverHeight=Safe(HoverHeight,80,0,120); R.BobAmplitude=Safe(BobAmplitude,10,0,20);
        R.AttackCooldown=Safe(AttackCooldown,3.6f,1,30); R.ProjectileSpeed=Safe(ProjectileSpeed,650,100,2000);
        R.DamageMultiplier=Safe(DamageMultiplier,0.5f,0,5); R.ShotCount=FMath::Clamp(ShotCount,3,16);
        R.VolleyCount=FMath::Clamp(VolleyCount,2,10); R.VolleyInterval=Safe(VolleyInterval,0.18f,0.08f,0.4f);
        R.AngleStep=Safe(AngleStep,18,1,40); R.ReturnAfter=Safe(ReturnAfter,0.9f,0.3f,2);
        R.WarningSeconds=Safe(WarningSeconds,0.9f,0.6f,3); R.TrapRadius=Safe(TrapRadius,420,280,700);
        R.GapDegrees=Safe(GapDegrees,100,75,160); return R;
    }
};

/** 纯确定性弹道相位；服务端与客户端表现各自推进，伤害仍只在服务端结算。 */
USTRUCT()
struct AI_API FRogueEnemyProjectileMotion
{
    GENERATED_BODY()
    UPROPERTY() float WarningSeconds = 0;
    UPROPERTY() float ReturnAfter = 0;
};

USTRUCT()
struct AI_API FRogueEnemyPatternShot
{
    GENERATED_BODY()
    UPROPERTY() FVector_NetQuantize Location = FVector::ZeroVector;
    UPROPERTY() FVector_NetQuantizeNormal Direction = FVector::ForwardVector;
    UPROPERTY() FRogueEnemyProjectileMotion Motion;
};
