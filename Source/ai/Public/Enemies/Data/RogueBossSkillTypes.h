#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enemies/Data/RogueBossAttackSettings.h"
#include "RogueBossSkillTypes.generated.h"

class URogueBossSkillAbility;
class ARogueCharacter;
class UTexture2D;

UENUM(BlueprintType)
enum class ERogueBossSkillPhase : uint8
{
    Idle UMETA(DisplayName="待机"),
    Charging UMETA(DisplayName="蓄力预警"),
    Firing UMETA(DisplayName="释放/连续发射"),
    Recovery UMETA(DisplayName="收招恢复")
};

/** 通用技能节奏放在 DT；具体招式由 Ability 子类实现，模型和材质仍在 BP。 */
USTRUCT(BlueprintType)
struct AI_API FRogueBossSkillRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="01 技能资料",meta=(DisplayName="技能名",DisplayPriority="1")) FText DisplayName;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="01 技能资料",meta=(DisplayName="启用",DisplayPriority="2")) bool bEnabled=true;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="01 技能资料",meta=(DisplayName="技能图标",DisplayPriority="3",HideFromDataTableEditorColumn)) TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="01 技能资料",meta=(DisplayName="图鉴说明",DisplayPriority="4",HideFromDataTableEditorColumn)) FText Description;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="01 技能资料",meta=(DisplayName="血条图标顺序",DisplayPriority="5",HideFromDataTableEditorColumn)) int32 DisplayOrder=0;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="02 出招节奏",meta=(DisplayName="蓄力时间",ClampMin="0",Units="s",DisplayPriority="1")) float ChargeSeconds=1.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="02 出招节奏",meta=(DisplayName="冷却时间",ClampMin="0",Units="s",DisplayPriority="2",ToolTip="收招结束后，这个技能再次可用的等待时间。")) float Cooldown=8.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="02 出招节奏",meta=(DisplayName="持续时间",ClampMin="0",Units="s",DisplayPriority="3",ToolTip="蓄力完成后的施法时长。所有 Boss 技能默认持续 10 秒，在此期间按施法间隔执行。")) float DurationSeconds=10.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="02 出招节奏",meta=(DisplayName="施法间隔",ClampMin="0.02",Units="s",DisplayPriority="4",ToolTip="持续期间两次攻击或治疗的间隔。天裁使用此值作为持续伤害间隔；金刚状态持续生效。")) float PulseInterval=.5f;
    // 读取旧表时换算持续时间，迁移后写为 0；这些旧字段不再参与释放调度。
    UPROPERTY(meta=(HideFromDataTableEditorColumn)) int32 PulseCount=0;
    UPROPERTY(meta=(HideFromDataTableEditorColumn)) float ReleaseHoldSeconds=0.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="02 出招节奏",meta=(DisplayName="收招时间",ClampMin="0",Units="s",DisplayPriority="6",HideFromDataTableEditorColumn)) float RecoverySeconds=1.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="02 出招节奏",meta=(DisplayName="登场首次等待",ClampMin="0",Units="s",DisplayPriority="7",HideFromDataTableEditorColumn)) float InitialDelay=3.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="02 出招节奏",meta=(DisplayName="蓄力期间也发射",DisplayPriority="8",HideFromDataTableEditorColumn,ToolTip="开着时连发期间继续保持蓄力状态，适用于八方飞星和追足冰棘。")) bool bPulseWhileCharging=false;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="03 触发与限次",meta=(DisplayName="生命低于比例",ClampMin="0",ClampMax="1",DisplayPriority="1",ToolTip="填写 0～1。0.5 表示生命低于一半才可触发；1 表示不限制最高生命。")) float MaxHealthFraction=1.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="03 触发与限次",meta=(DisplayName="最低生命比例",ClampMin="0",ClampMax="1",DisplayPriority="2",HideFromDataTableEditorColumn,ToolTip="填写 0～1。生命达到此比例才有资格使用此技能。")) float MinHealthFraction=0.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="03 触发与限次",meta=(DisplayName="每场限次",ClampMin="0",DisplayPriority="3",ToolTip="一只 Boss 在一场战斗里最多释放几次。0 表示不限。")) int32 MaxUses=0;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="03 触发与限次",meta=(DisplayName="出招权重",ClampMin="0",DisplayPriority="4",HideFromDataTableEditorColumn,ToolTip="同一优先级中选中此技能的相对概率。0 表示不会出招。")) float Weight=1.f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="03 触发与限次",meta=(DisplayName="优先级",DisplayPriority="5",HideFromDataTableEditorColumn,ToolTip="同时满足条件时，优先执行数值较高的技能。")) int32 Priority=0;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="04 招式参数",meta=(DisplayName="该招式的专用参数",DisplayPriority="1",HideFromDataTableEditorColumn)) FRogueBossAttackSettings Attack;

    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="05 高级控制",meta=(DisplayName="能力实现类",DisplayPriority="1",HideFromDataTableEditorColumn)) TSubclassOf<URogueBossSkillAbility> AbilityClass;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="05 高级控制",meta=(DisplayName="需要存活玩家目标",ToolTip="目标濒死、死亡或离开时中止本次技能，下次重新选目标；无目标的自身环形技能可关闭。",DisplayPriority="2",HideFromDataTableEditorColumn)) bool bRequiresTarget=true;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="05 高级控制",meta=(DisplayName="释放后不因目标退场而取消",DisplayPriority="3",HideFromDataTableEditorColumn,EditCondition="bRequiresTarget",EditConditionHides,ToolTip="例如天裁激光已经射出，原目标濒死或离场时，光束仍会继续到持续时间结束。")) bool bKeepCastingAfterRelease=false;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="05 高级控制",meta=(DisplayName="暂停移动",DisplayPriority="4",HideFromDataTableEditorColumn)) bool bPauseMovement=true;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="05 高级控制",meta=(DisplayName="暂停普通攻击",ToolTip="覆盖蓄力、释放和恢复；不清除已发出的普通弹体。",DisplayPriority="5",HideFromDataTableEditorColumn)) bool bPauseNormalAttacks=true;

    FRogueBossSkillRow Sanitized() const;
};

/** 只承载表现状态；客户端据服务器时间恢复当前预警，不从该快照补发伤害或历史连发。 */
USTRUCT(BlueprintType)
struct AI_API FRogueBossSkillState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDataTableRowHandle Skill;
    UPROPERTY(BlueprintReadOnly) FText DisplayName;
    UPROPERTY(BlueprintReadOnly) ERogueBossSkillPhase Phase=ERogueBossSkillPhase::Idle;
    UPROPERTY(BlueprintReadOnly) int32 CastId=0;
    UPROPERTY() uint32 Generation=0;
    UPROPERTY(BlueprintReadOnly) int32 PulseIndex=0;
    UPROPERTY(BlueprintReadOnly) float PhaseStartServerTime=0;
    UPROPERTY(BlueprintReadOnly) float PhaseEndServerTime=0;
    UPROPERTY(BlueprintReadOnly) FVector TargetLocation=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<ARogueCharacter> Target=nullptr;
    UPROPERTY(BlueprintReadOnly) bool bCancelled=false;
};
