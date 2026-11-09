#pragma once

#include "CoreMinimal.h"
#include "RogueWeaponCombatSettings.generated.h"

/** 武器 DT 行中的战斗参数；每次激活都覆盖旧池化实例的设置。 */
USTRUCT(BlueprintType)
struct AI_API FRogueProjectileCombatSettings
{
    GENERATED_BODY()
    FRogueProjectileCombatSettings() = default;
    explicit FRogueProjectileCombatSettings(float Radius) : CollisionRadius(Radius) {}
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="弹体", meta=(DisplayName="命中碰撞半径", ClampMin="1", Units="cm"))
    float CollisionRadius = 16.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="弹体", meta=(DisplayName="弹体存活时间", ClampMin="0.01", Units="s"))
    float Lifetime = 30.f;
};

/** 陨星落地残留；伤害倍率基于这一发的实际攻击伤害，减速采用最强来源，不反复相乘。 */
USTRUCT(BlueprintType)
struct AI_API FRogueIceFlameCombatSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="蓝焰",meta=(DisplayName="留下地面蓝焰")) bool bEnabled=true;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="蓝焰",meta=(DisplayName="持续时间",ClampMin=".1",Units="s")) float Lifetime=4;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="蓝焰",meta=(DisplayName="相对爆炸范围倍率",ClampMin=".01")) float RadiusScale=.65f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="蓝焰",meta=(DisplayName="每秒伤害倍率",ClampMin="0",ToolTip="相对这一发实际攻击伤害的倍率；0.08 表示每秒造成 8% 的伤害。")) float DamagePerSecondScale=.08f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="蓝焰",meta=(DisplayName="持续伤害间隔",ClampMin=".1",Units="s")) float DamageInterval=.5f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="蓝焰",meta=(DisplayName="移动减速比例",ClampMin="0",ClampMax=".95")) float SlowFraction=.25f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="蓝焰",meta=(DisplayName="影响脚底高度范围",ClampMin="1",Units="cm")) float HeightTolerance=150;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="蓝焰",meta=(DisplayName="落地检测深度",ClampMin="1",Units="cm")) float GroundSearchDepth=1600;
};

/** 冰焰球在目标上方斜坠；高度与水平距离一起决定下落角度。 */
USTRUCT(BlueprintType)
struct AI_API FRogueRocketFallSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="天降弹道", meta=(DisplayName="相对目标的召唤高度", ClampMin="1", Units="cm"))
    float SummonHeight = 1100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="天降弹道", meta=(DisplayName="斜坠水平偏移", ClampMin="1", Units="cm", ToolTip="从玩家朝目标的方向后退该距离生成；越大，下落越倾斜。"))
    float HorizontalOffset = 650.f;
};

/** 武器 DT 行中的战斗参数；每次激活都覆盖旧池化实例的设置。 */
USTRUCT(BlueprintType)
struct AI_API FRogueMortarFlightSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="弹道", meta=(DisplayName="弹道弧度", ClampMin="0", ClampMax="1"))
    float ArcParam = .74f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="弹道", meta=(DisplayName="重力倍率", ClampMin="0.01"))
    float GravityScale = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="弹道", meta=(DisplayName="最低水平发射速度", ClampMin="1", Units="cm/s"))
    float MinimumLaunchSpeed = 680.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="弹道", meta=(DisplayName="最短飞行时间", ClampMin="0.01", Units="s"))
    float MinimumFlightTime = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="弹道", meta=(DisplayName="最长飞行时间", ClampMin="0.01", Units="s"))
    float MaximumFlightTime = 2.8f;
};

/** 武器 DT 行中的战斗参数；每次激活都覆盖旧池化实例的设置。 */
USTRUCT(BlueprintType)
struct AI_API FRogueTalismanCombatSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="飞行", meta=(DisplayName="出击速度", ClampMin="100", Units="cm/s"))
    float FlightSpeed = 1500.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="飞行", meta=(DisplayName="返回速度", ClampMin="100", Units="cm/s"))
    float ReturnSpeed = 1900.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="飞行", meta=(DisplayName="穿刺后的越过距离", ClampMin="40", Units="cm"))
    float OvershootDistance = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="飞行", meta=(DisplayName="伤害扫掠半径", ClampMin="1", Units="cm"))
    float HitRadius = 28.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="飞行", meta=(DisplayName="单次出击超时返回", ClampMin="1", Units="s"))
    float MaxSortieSeconds = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="飞行", meta=(DisplayName="返回超时归位", ClampMin="0.25", Units="s"))
    float MaxReturnSeconds = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="飞行", meta=(DisplayName="基础穿刺敌人数", ClampMin="1"))
    int32 BaseTargetCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="飞行", meta=(DisplayName="基础每敌穿刺次数", ClampMin="1"))
    int32 BaseHitsPerTarget = 1;
};









/** 冰刀所有战斗尺寸、节奏和剑气数值由武器表统一配置。 */
USTRUCT(BlueprintType)
struct AI_API FRogueFrostBladeCombatSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="横斩",meta=(DisplayName="基础刀刃伤害长度",ClampMin="1",Units="cm")) float BladeLength=190;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="横斩",meta=(DisplayName="横斩碰撞宽度半径",ClampMin="1",Units="cm")) float HitRadius=20;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="横斩",meta=(DisplayName="基础离身距离",ClampMin="50",Units="cm",ToolTip="冰刀初始出击姿态的基础距离。随通用攻击范围与武器基础范围增长，不作为成长上限。")) float MaxLeashDistance=330;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="横斩",meta=(DisplayName="横斩角度",ClampMin="10",ClampMax="180",Units="deg")) float SweepDegrees=140;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="横斩",meta=(DisplayName="靠近目标时间",ClampMin="0.03",Units="s")) float ApproachSeconds=.16f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="横斩",meta=(DisplayName="挥刀时间",ClampMin="0.03",Units="s")) float SlashSeconds=.26f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="横斩",meta=(DisplayName="归位时间",ClampMin="0.03",Units="s")) float ReturnSeconds=.22f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="横斩",meta=(DisplayName="相对目标中心横斩高度",Units="cm")) float SlashHeightOffset=0;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="伤害",meta=(DisplayName="攻击力加成倍率",ClampMin="0")) float AttackPowerScale=.8f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑气",meta=(DisplayName="剑气相对横斩伤害倍率",ClampMin="0")) float WaveDamageMultiplier=.4f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑气",meta=(DisplayName="剑气飞行速度",ClampMin="1",Units="cm/s")) float WaveSpeed=1200;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑气",meta=(DisplayName="剑气寿命",ClampMin="0.01",Units="s")) float WaveLifetime=30;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑气",meta=(DisplayName="基础剑气半宽",ClampMin="1",Units="cm")) float WaveHalfWidth=240;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑气",meta=(DisplayName="剑气碰撞前后半厚",ClampMin="1",Units="cm")) float WaveHalfThickness=55;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑气",meta=(DisplayName="剑气碰撞高度半径",ClampMin="1",Units="cm")) float WaveHalfHeight=30;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑气",meta=(DisplayName="剑气出射前移",ClampMin="0",Units="cm")) float WaveSpawnOffset=25;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑气",meta=(DisplayName="多道剑气间隔角度",ClampMin="0",Units="deg")) float WaveSpreadStepDegrees=10;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑气",meta=(DisplayName="多道剑气最大扇形角",ClampMin="0",ClampMax="150",Units="deg")) float MaxWaveSpreadDegrees=70;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑气",meta=(DisplayName="剑气数量上限(-1=不限)")) int32 MaxWaveCount=-1;
};
