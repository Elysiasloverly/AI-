#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/SoundBase.h"
#include "Audio/RogueAttackAudioSettings.h"
#include "RogueAudioConfig.generated.h"

class ARogueWeaponBase;
class ARogueEnemy;
class USoundClass;
class USoundMix;

UENUM(BlueprintType)
enum class ERogueMusicScene : uint8
{
    None UMETA(DisplayName="停止"),
    Menu UMETA(DisplayName="主菜单与房间"),
    Day UMETA(DisplayName="白天战斗"),
    Night UMETA(DisplayName="黑夜战斗"),
    Boss UMETA(DisplayName="Boss 战斗"),
    Results UMETA(DisplayName="游戏结束")
};

USTRUCT(BlueprintType)
struct AI_API FRogueMusicTrack
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="音乐", ToolTip="支持 SoundWave、SoundCue、MetaSound Source；留空则此场景安静，不借用其他场景音乐。"))
    TSoftObjectPtr<USoundBase> Sound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="曲目音量", ClampMin="0", ClampMax="2"))
    float Volume = .7f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="循环播放", ToolTip="单次素材结束后自动续播；需要无缝循环时，建议在 SoundCue 或 MetaSound 中制作循环。"))
    bool bLoop = true;
};

USTRUCT(BlueprintType)
struct AI_API FRogueWeaponAudioEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="武器名称")) FString WeaponName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="武器类", ToolTip="按类匹配，子类优先；不依赖数据表中的中文名称。")) TSubclassOf<ARogueWeaponBase> WeaponClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="攻击音效")) FRogueAttackAudioSettings AttackAudio;
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyAudioEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="敌人名称")) FString EnemyName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="敌人类")) TSubclassOf<ARogueEnemy> EnemyClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="攻击音效", ToolTip="全部留空时沿用父类配置；关闭启用攻击音效可让该类静音。")) FRogueAttackAudioSettings AttackAudio;
};

/** 背景音乐、武器音效、敌人音效与混音路由的唯一编辑入口。 */
UCLASS(BlueprintType)
class AI_API URogueAudioConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="曲目", meta=(DisplayName="主菜单与房间")) FRogueMusicTrack Menu;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="曲目", meta=(DisplayName="白天战斗")) FRogueMusicTrack Day;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="曲目", meta=(DisplayName="黑夜战斗")) FRogueMusicTrack Night;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="曲目", meta=(DisplayName="Boss 战斗")) FRogueMusicTrack Boss;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="曲目", meta=(DisplayName="游戏结束")) FRogueMusicTrack Results;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="切换", meta=(DisplayName="交叉渐变时间", ClampMin="0", ClampMax="10", Units="s")) float CrossfadeSeconds = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="切换", meta=(DisplayName="主菜单地图"))
    TSoftObjectPtr<UWorld> MenuMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/MainMenu.MainMenu")));
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="混音路由", meta=(DisplayName="背景音乐分类")) TObjectPtr<USoundClass> MusicClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="混音路由", meta=(DisplayName="音效分类")) TObjectPtr<USoundClass> EffectsClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="混音路由", meta=(DisplayName="界面音效分类")) TObjectPtr<USoundClass> UIClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="混音路由", meta=(DisplayName="玩家音量混音")) TObjectPtr<USoundMix> UserMix;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="武器音效", meta=(DisplayName="各武器攻击音效", TitleProperty="WeaponName"))
    TArray<FRogueWeaponAudioEntry> Weapons;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="敌人音效", meta=(DisplayName="各敌人攻击音效", TitleProperty="EnemyName"))
    TArray<FRogueEnemyAudioEntry> Enemies;
    const FRogueAttackAudioSettings* FindEnemyAudio(const UClass* Class) const;
    const FRogueAttackAudioSettings* FindWeaponAudio(const UClass* Class) const;
    const FRogueMusicTrack* FindTrack(ERogueMusicScene Scene) const;
};
