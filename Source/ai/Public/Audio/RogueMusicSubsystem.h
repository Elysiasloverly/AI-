#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Audio/RogueAudioConfig.h"
#include "RogueMusicSubsystem.generated.h"

class UAudioComponent;
class UUserWidget;
class USoundConcurrency;

/** 每台机器独立播放，跨地图保留；仅观察复制状态，不依赖角色或 HUD 存活。 */
UCLASS(Config=Game)
class AI_API URogueMusicSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Rogue|Audio", meta=(WorldContext="Context"))
    static URogueMusicSubsystem* GetAudioSystem(const UObject* Context);
    /** 绑定当前控件及嵌套控件中的按钮；动态增添按钮后可再次调用，重复调用不会叠加声音。返回处理的按钮数。 */
    UFUNCTION(BlueprintCallable, Category="Rogue|Audio|UI")
    static int32 BindWidgetButtonSounds(UUserWidget* Widget);
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return bInitialized && !IsTemplate(); }
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(URogueMusicSubsystem, STATGROUP_Tickables); }

    UFUNCTION(BlueprintPure, Category="Rogue|Audio") float GetSoundEffectsVolume() const { return SoundEffectsVolume; }
    UFUNCTION(BlueprintPure, Category="Rogue|Audio") float GetMusicVolume() const { return MusicVolume; }
    /** 立即生效并保存；不复制到其他玩家。 */
    UFUNCTION(BlueprintCallable, Category="Rogue|Audio") void SetSoundEffectsVolume(float Value);
    UFUNCTION(BlueprintCallable, Category="Rogue|Audio") void SetMusicVolume(float Value);
    UFUNCTION(BlueprintPure, Category="Rogue|Audio") ERogueMusicScene GetCurrentScene() const { return CurrentScene; }
    /** 只读播放诊断，不允许外部替换或销毁持久音频组件。 */
    UFUNCTION(BlueprintPure, Category="Rogue|Audio|Debug") FString GetPlaybackDebugInfo() const;
    UFUNCTION(BlueprintCallable, Category="Rogue|Audio") void RefreshMusicState();
    /** 更换整份音乐配置；不改数据资产及玩家音量。 */
    UFUNCTION(BlueprintCallable, Category="Rogue|Audio") void SetMusicConfig(URogueAudioConfig* Config);
    USoundClass* GetEffectsClass() const;
    URogueAudioConfig* GetAudioConfig() const { return LoadedConfig ? LoadedConfig.Get() : MusicConfigAsset.LoadSynchronous(); }

    static ERogueMusicScene ResolveBattleScene(bool bGameOver, bool bBossAlive, bool bNight);
private:
    friend struct FRogueMusicTestAccess;
    friend struct FRogueUIButtonAudioTestAccess;
    int32 BindButtonSounds(UUserWidget* Widget);
    UFUNCTION() void HandleUIButtonPressed();
    UFUNCTION() void HandleUIButtonHovered();
    void PlayButtonSound(bool bPressed);
    void RequestScene(ERogueMusicScene Scene);
    void ApplyVolumes();
    void LoadVolumes();
    void SaveVolumes();
    void OnTrackFinished(UAudioComponent* Component);
    bool CanPlayInWorld() const;
    UPROPERTY(Config) TSoftObjectPtr<URogueAudioConfig> MusicConfigAsset;
    UPROPERTY(Transient) TObjectPtr<URogueAudioConfig> LoadedConfig;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Channels;
    UPROPERTY(Transient) TObjectPtr<USoundConcurrency> UIButtonConcurrency;
    TWeakObjectPtr<UAudioComponent> LastUIButtonComponent;
    uint32 UIButtonPressedCount = 0;
    uint32 UIButtonHoveredCount = 0;
    UPROPERTY(Transient) TObjectPtr<USoundBase> CurrentSound;
    TWeakObjectPtr<UWorld> MixWorld;
    ERogueMusicScene CurrentScene = ERogueMusicScene::None;
    int32 ActiveChannel = INDEX_NONE;
    float SoundEffectsVolume = 1.f;
    float MusicVolume = 1.f;
    float PollAccumulator = 0;
    bool bInitialized = false;
    bool bCurrentLoop = false;
    bool bLoopPending = false;
    float CurrentTrackVolume = .7f;
};
