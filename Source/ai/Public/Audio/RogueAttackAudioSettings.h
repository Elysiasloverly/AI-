#pragma once

#include "CoreMinimal.h"
#include "RogueAttackAudioSettings.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
class AActor;

/** 单次攻击的音效变体。支持 SoundWave、SoundCue 和 MetaSound Source。 */
USTRUCT(BlueprintType)
struct AI_API FRogueAttackAudioSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="攻击音效", meta=(DisplayName="启用攻击音效", ToolTip="子类关闭此项可明确静音，不再向父类回退。"))
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="攻击音效", meta=(DisplayName="攻击音效变体", ToolTip="每次攻击随机选择一个有效音效，避免连续重复；空槽跳过。建议使用短促、非循环音效。可继续增加槽位。", EditCondition="bEnabled"))
    TArray<TObjectPtr<USoundBase>> Sounds = { nullptr, nullptr, nullptr };

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="攻击音效", meta=(DisplayName="音量倍率", ClampMin="0", ClampMax="4", EditCondition="bEnabled"))
    float VolumeMultiplier = .7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="攻击音效", meta=(DisplayName="最低音调", ClampMin="0.1", ClampMax="4", EditCondition="bEnabled"))
    float PitchMin = .97f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="攻击音效", meta=(DisplayName="最高音调", ClampMin="0.1", ClampMax="4", EditCondition="bEnabled"))
    float PitchMax = 1.03f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="攻击音效", meta=(DisplayName="最短播放间隔", ClampMin="0", Units="s", EditCondition="bEnabled", ToolTip="限制同一把武器/同一只敌人的播放频率，0 表示不额外节流；不改变攻击及伤害频率。"))
    float MinimumInterval = .06f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="攻击音效", meta=(DisplayName="距离衰减", EditCondition="bEnabled", ToolTip="留空使用默认球形距离衰减；远处攻击不会以满音量播放。"))
    TObjectPtr<USoundAttenuation> Attenuation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="攻击音效", meta=(DisplayName="并发规则", EditCondition="bEnabled", ToolTip="留空使用玩家/敌人各自的默认并发限制，避免大量攻击音效叠加。"))
    TObjectPtr<USoundConcurrency> Concurrency;

    bool HasSound() const;
};

/** 本地播放账本；不复制，不使用游戏战斗的随机流。 */
struct FRogueAttackAudioState
{
    double LastTime = -1.e30;
    TWeakObjectPtr<USoundBase> LastSound;
    void Reset() { LastTime = -1.e30; LastSound.Reset(); }
};

struct FRogueAttackAudioRequest
{
    USoundBase* Sound = nullptr;
    float Volume = 0;
    float Pitch = 1;
};

namespace RogueAttackAudio
{
    /** 选择/节流与音频设备解耦，空配置不消耗随机数或更新账本。 */
    AI_API bool Prepare(const FRogueAttackAudioSettings& Settings, FRogueAttackAudioState& State,
        double Now, FRandomStream& Random, FRogueAttackAudioRequest& OutRequest);

#if WITH_DEV_AUTOMATION_TESTS
    DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPrepared, const UObject*, AActor*, const FRogueAttackAudioRequest&);
    AI_API FOnPrepared& OnPrepared();
#endif
}
