#pragma once
#include "CoreMinimal.h"
#include "RogueBossAttackSettings.generated.h"
class ARogueBossAttackActor;
class ARogueEnemyProjectile;

/** Boss 招式共用的弹道/区域数值；模型和材质由 EffectClass 指定的 BP 提供。 */
USTRUCT(BlueprintType)
struct AI_API FRogueBossAttackSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="表现",meta=(DisplayName="技能效果蓝图")) TSubclassOf<ARogueBossAttackActor> EffectClass;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="弹幕",meta=(DisplayName="普通子弹蓝图")) TSubclassOf<ARogueEnemyProjectile> BulletClass;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="伤害",meta=(DisplayName="基础伤害倍率",ToolTip="乘以 Boss 的基础伤害。光束按每次持续伤害结算。",ClampMin="0")) float DamageScale=.65f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="弹道",meta=(DisplayName="飞行速度",ClampMin="1",Units="cm/s")) float Speed=700;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="弹道",meta=(DisplayName="半径",ToolTip="滚动火球、天降火球和冰刺使用此项计算受击范围。回生和金刚使用它控制符阵的表现范围。",ClampMin="1",Units="cm")) float Radius=120;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="弹道",meta=(DisplayName="相邻弹道夹角",Units="deg")) float SpreadDegrees=16;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="弹幕",meta=(DisplayName="旋转角速度",Units="deg/s")) float RotationSpeed=90;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="弹幕",meta=(DisplayName="每轮环形弹数",ToolTip="0 表示沿用该敌人原型的普通环形弹幕数量。",ClampMin="0",ClampMax="128")) int32 RingCount=0;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="光束",meta=(DisplayName="无地图边界时的备用长度",ToolTip="战斗地图有闭合外边界时，天裁会打到外边界。此值只在找不到外边界时生效。",ClampMin="1",Units="cm")) float BeamRange=3000;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="光束",meta=(DisplayName="光束半宽",ToolTip="从中心线向任一侧延伸的距离。完整宽度等于这里的两倍。",ClampMin="1",Units="cm")) float BeamHalfWidth=75;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="光束",meta=(DisplayName="持续伤害间隔",ClampMin=".1",Units="s")) float DamageInterval=.25f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑波",meta=(DisplayName="剑波初始伤害半宽",ClampMin="1",Units="cm")) float WaveHalfWidth=280;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑波",meta=(DisplayName="刀波中心离地高度",ClampMin="1",Units="cm")) float WaveHeightAboveGround=130;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑波",meta=(DisplayName="刀波初始伤害半高",ToolTip="完整高度为此值的两倍，外观和碰撞随飞行距离一起增长。",ClampMin="1",Units="cm")) float WaveHalfHeight=60;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑波",meta=(DisplayName="飞多远达到最大尺寸",ToolTip="刀波从发出位置沿水平面移动到此距离时达到最大尺寸。",ClampMin="1",Units="cm")) float WaveGrowthDistance=1200;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="剑波",meta=(DisplayName="最大尺寸倍率",ToolTip="刀波外观和命中范围共用。填写 1 时尺寸固定。",ClampMin="1",ClampMax="4")) float WaveMaxScale=2;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="区域",meta=(DisplayName="预警时间",ClampMin=".4",Units="s")) float WarningSeconds=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="区域",meta=(DisplayName="每轮火球数",ToolTip="一次释放轮次会生成的火球数。总轮数在出招节奏里调整。",ClampMin="1",ClampMax="16")) int32 BombsPerPulse=3;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="区域",meta=(DisplayName="玩家周围安全半径",ClampMin="180",Units="cm")) float SafeRadius=450;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="区域",meta=(DisplayName="落点间距",ClampMin="100",Units="cm")) float ImpactSpacing=420;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="区域",meta=(DisplayName="火球起始高度",ClampMin="300",Units="cm")) float FallHeight=1600;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="区域",meta=(DisplayName="地面余火持续",ClampMin=".1",Units="s")) float FireSeconds=4;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="区域",meta=(DisplayName="每跳灼烧伤害倍率",ToolTip="每次余火伤害乘以火球的落地伤害。",ClampMin="0")) float BurnDamageScale=.12f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="区域",meta=(DisplayName="灼烧间隔",ClampMin=".1",Units="s")) float BurnInterval=.5f;
};

USTRUCT(BlueprintType)
struct AI_API FRogueBossSkillSelection
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(DisplayName="白天随机技能数",ClampMin="0")) int32 DayCount=3;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(DisplayName="黑夜随机技能数",ClampMin="0")) int32 NightCount=4;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(DisplayName="获得全部技能的波次",ClampMin="1")) int32 AllSkillsWave=100;
};
