#include "Audio/RogueMusicSubsystem.h"
#include "AudioDevice.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Sound/SoundConcurrency.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Framework/RogueGameState.h"
#include "Framework/RogueMainMenuGameMode.h"
#include "Networking/RogueLobby.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace
{
    const TCHAR* SettingsSection = TEXT("/Script/ai.RogueRuntimeSettings");
    float SafeVolume(float Value) { return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : 1.f; }
}

URogueMusicSubsystem* URogueMusicSubsystem::GetAudioSystem(const UObject* Context)
{
    auto* Instance = UGameplayStatics::GetGameInstance(Context);
    return Instance ? Instance->GetSubsystem<URogueMusicSubsystem>() : nullptr;
}

int32 URogueMusicSubsystem::BindWidgetButtonSounds(UUserWidget* Widget)
{
    if (!IsValid(Widget)) return 0;
    if (auto* Audio = GetAudioSystem(Widget)) return Audio->BindButtonSounds(Widget);
    return 0;
}

int32 URogueMusicSubsystem::BindButtonSounds(UUserWidget* Widget)
{
    if (!Widget || !Widget->WidgetTree) return 0;
    int32 ButtonCount = 0;
    Widget->WidgetTree->ForEachWidgetAndDescendants([this, &ButtonCount](UWidget* Child)
    {
        if (auto* Button = Cast<UButton>(Child))
        {
            // 只移除 Slate 自带声音，保留美术样式和原有的按钮业务事件。
            FButtonStyle Style = Button->GetStyle();
            Style.PressedSlateSound = FSlateSound();
            Style.HoveredSlateSound = FSlateSound();
            Button->SetStyle(Style);
            Button->OnPressed.AddUniqueDynamic(this, &ThisClass::HandleUIButtonPressed);
            Button->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleUIButtonHovered);
            ++ButtonCount;
        }
    });
    return ButtonCount;
}

void URogueMusicSubsystem::HandleUIButtonPressed() { PlayButtonSound(true); }
void URogueMusicSubsystem::HandleUIButtonHovered() { PlayButtonSound(false); }

void URogueMusicSubsystem::PlayButtonSound(bool bPressed)
{
    if (!CanPlayInWorld() || !LoadedConfig || !LoadedConfig->Buttons.bEnabled || SoundEffectsVolume <= KINDA_SMALL_NUMBER) return;
    const auto& Settings = LoadedConfig->Buttons;
    USoundBase* Sound = bPressed ? Settings.ClickSound.Get() : Settings.HoverSound.Get();
    const float RawVolume = bPressed ? Settings.ClickVolume : Settings.HoverVolume;
    const float Volume = FMath::IsFinite(RawVolume) ? FMath::Clamp(RawVolume, 0.f, 2.f) : 0.f;
    if (!Sound || Volume <= KINDA_SMALL_NUMBER) return;
    // UButton 只在进入/按下的状态边沿发事件。跨按钮的时间限流会吞掉真实输入；
    // 密集声音交由有上限的并发组处理，不丢弃后续按钮的进入事件。
    if (MixWorld.Get() != GetWorld()) ApplyVolumes();
    if (!UIButtonConcurrency)
    {
        UIButtonConcurrency = NewObject<USoundConcurrency>(this);
        UIButtonConcurrency->Concurrency.MaxCount = 6;
        UIButtonConcurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
        UIButtonConcurrency->Concurrency.VoiceStealReleaseTime = .01f;
    }
    // 先配置分类再播放；按下时响，进入地图后可播完短尾音，结束后组件自行回收。
    UAudioComponent* Component = UGameplayStatics::CreateSound2D(this, Sound, Volume, 1.f, 0.f, UIButtonConcurrency, true, true);
    if (!Component) return;
    Component->SoundClassOverride = LoadedConfig->UIClass ? LoadedConfig->UIClass.Get() : LoadedConfig->EffectsClass.Get();
    Component->bIsUISound = true;
    Component->bAllowSpatialization = false;
    Component->bStopWhenOwnerDestroyed = false;
    Component->Play();
    LastUIButtonComponent = Component;
    if (bPressed) ++UIButtonPressedCount; else ++UIButtonHoveredCount;
}

void URogueMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Channels.SetNum(2);
    LoadVolumes();
    LoadedConfig = MusicConfigAsset.LoadSynchronous();
    bInitialized = true;
}

void URogueMusicSubsystem::Deinitialize()
{
    bInitialized = false;
    for (UAudioComponent* Component : Channels)
        if (IsValid(Component)) { Component->OnAudioFinishedNative.RemoveAll(this); Component->Stop(); Component->DestroyComponent(); }
    if (UAudioComponent* Component = LastUIButtonComponent.Get()) Component->Stop();
    LastUIButtonComponent.Reset(); UIButtonConcurrency = nullptr;
    Channels.Empty(); CurrentSound = nullptr; LoadedConfig = nullptr; MixWorld.Reset();
    Super::Deinitialize();
}

bool URogueMusicSubsystem::CanPlayInWorld() const
{
    const UWorld* World = GetWorld();
    return World && World->IsGameWorld() && World->GetGameInstance() == GetGameInstance()
        && World->GetNetMode() != NM_DedicatedServer;
}

void URogueMusicSubsystem::Tick(float DeltaTime)
{
    if (!CanPlayInWorld()) return;
    if (MixWorld.Get() != GetWorld()) ApplyVolumes();
    if (bLoopPending && MusicVolume > KINDA_SMALL_NUMBER)
    {
        bLoopPending = false;
        UAudioComponent* Audio = Channels.IsValidIndex(ActiveChannel) ? Channels[ActiveChannel].Get() : nullptr;
        if (bCurrentLoop && IsValid(Audio) && Audio->Sound == CurrentSound && !Audio->IsPlaying()) Audio->FadeIn(0, CurrentTrackVolume);
    }
    PollAccumulator += DeltaTime;
    if (PollAccumulator >= .2f) { PollAccumulator = 0; RefreshMusicState(); }
}

ERogueMusicScene URogueMusicSubsystem::ResolveBattleScene(bool bGameOver, bool bBossAlive, bool bNight)
{
    if (bGameOver) return ERogueMusicScene::Results;
    if (bBossAlive) return ERogueMusicScene::Boss;
    return bNight ? ERogueMusicScene::Night : ERogueMusicScene::Day;
}

void URogueMusicSubsystem::RefreshMusicState()
{
    if (!CanPlayInWorld()) return;
    UWorld* World = GetWorld();
    if (const ARogueGameState* State = World->GetGameState<ARogueGameState>())
    {
        // Countdown / Travelling 仍属于白天；只在黑夜地图的阶段状态生效后切换。
        RequestScene(ResolveBattleScene(State->IsGameOver(), State->IsBossAlive(), State->GetBattleStage().Phase == ERogueBattleStage::Night));
    }
    else if (World->GetGameState<ARogueLobbyGameState>() || World->GetAuthGameMode<ARogueMainMenuGameMode>()
        || (LoadedConfig && UGameplayStatics::GetCurrentLevelName(World, true) == FPackageName::GetShortName(LoadedConfig->MenuMap.ToSoftObjectPath().GetLongPackageName())))
    {
        RequestScene(ERogueMusicScene::Menu);
    }
    // 过渡世界尚无 GameState 时保留当前曲目，避免 seamless travel 中途切成菜单音乐。
}

void URogueMusicSubsystem::SetMusicConfig(URogueAudioConfig* Config)
{
    for (UAudioComponent* Component : Channels) if (IsValid(Component)) Component->Stop();
    LoadedConfig = Config; CurrentScene = ERogueMusicScene::None; CurrentSound = nullptr; ActiveChannel = INDEX_NONE;
    MixWorld.Reset(); ApplyVolumes(); RefreshMusicState();
}

void URogueMusicSubsystem::RequestScene(ERogueMusicScene Scene)
{
    if (Scene == CurrentScene) return;
    CurrentScene = Scene;
    const FRogueMusicTrack* Track = LoadedConfig ? LoadedConfig->FindTrack(Scene) : nullptr;
    USoundBase* Sound = Track ? Track->Sound.LoadSynchronous() : nullptr;
    const float Fade = LoadedConfig && FMath::IsFinite(LoadedConfig->CrossfadeSeconds) ? FMath::Clamp(LoadedConfig->CrossfadeSeconds, 0.f, 10.f) : 1.f;
    const float TrackVolume = Track && FMath::IsFinite(Track->Volume) ? FMath::Clamp(Track->Volume, 0.f, 2.f) : .7f;
    bCurrentLoop = Track && Track->bLoop;
    bLoopPending = false;
    CurrentTrackVolume = TrackVolume;
    UAudioComponent* Active = Channels.IsValidIndex(ActiveChannel) ? Channels[ActiveChannel].Get() : nullptr;
    // 相邻阶段配置了同一首曲子时只调整曲目音量，不重新从头播放。
    if (Sound && Sound == CurrentSound && IsValid(Active))
    {
        Active->AdjustVolume(Fade, TrackVolume);
        return;
    }
    if (IsValid(Active)) { if (Fade > 0) Active->FadeOut(Fade, 0); else Active->Stop(); }
    CurrentSound = Sound;
    if (!Sound) { ActiveChannel = INDEX_NONE; return; }
    ActiveChannel = ActiveChannel == 0 ? 1 : 0;
    UAudioComponent* Next = Channels[ActiveChannel];
    if (!IsValid(Next))
    {
        Next = UGameplayStatics::CreateSound2D(this, Sound, 1, 1, 0, nullptr, true, false);
        Channels[ActiveChannel] = Next;
        if (Next) Next->OnAudioFinishedNative.AddUObject(this, &ThisClass::OnTrackFinished);
    }
    if (!Next) return;
    Next->Stop(); Next->SetSound(Sound);
    Next->SoundClassOverride = LoadedConfig ? LoadedConfig->MusicClass : nullptr;
    Next->bIsUISound = true; Next->bIsMusic = true; Next->bAllowSpatialization = false;
    Next->bStopWhenOwnerDestroyed = false;
    Next->FadeIn(Fade, TrackVolume);
    Next->SetPaused(MusicVolume <= KINDA_SMALL_NUMBER);
    UE_LOG(LogTemp, Log, TEXT("[RogueMusic] Scene=%d Sound=%s"), int32(Scene), *GetNameSafe(Sound));
}

void URogueMusicSubsystem::OnTrackFinished(UAudioComponent* Component)
{
    if (bInitialized && bCurrentLoop && CanPlayInWorld() && Channels.IsValidIndex(ActiveChannel)
        && Component == Channels[ActiveChannel] && CurrentSound && Component->Sound == CurrentSound)
    {
        // 子系统即使在暂停时仍 tick；不使用会随世界暂停的计时器。
        bLoopPending = true;
    }
}

void URogueMusicSubsystem::LoadVolumes()
{
    float LegacyVolume = 1.f;
    if (!GConfig) return;
    GConfig->GetFloat(SettingsSection, TEXT("MasterVolume"), LegacyVolume, GGameUserSettingsIni);
    SoundEffectsVolume = MusicVolume = SafeVolume(LegacyVolume);
    GConfig->GetFloat(SettingsSection, TEXT("SoundEffectsVolume"), SoundEffectsVolume, GGameUserSettingsIni);
    GConfig->GetFloat(SettingsSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
    SoundEffectsVolume = SafeVolume(SoundEffectsVolume); MusicVolume = SafeVolume(MusicVolume);
}

void URogueMusicSubsystem::SaveVolumes()
{
    if (!GConfig) return;
    GConfig->SetFloat(SettingsSection, TEXT("SoundEffectsVolume"), SoundEffectsVolume, GGameUserSettingsIni);
    GConfig->SetFloat(SettingsSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void URogueMusicSubsystem::SetSoundEffectsVolume(float Value) { SoundEffectsVolume = SafeVolume(Value); ApplyVolumes(); SaveVolumes(); }
void URogueMusicSubsystem::SetMusicVolume(float Value) { MusicVolume = SafeVolume(Value); ApplyVolumes(); SaveVolumes(); }
USoundClass* URogueMusicSubsystem::GetEffectsClass() const { return LoadedConfig ? LoadedConfig->EffectsClass.Get() : nullptr; }

FString URogueMusicSubsystem::GetPlaybackDebugInfo() const
{
    FString Entries;
    for (int32 Index = 0; Index < Channels.Num(); ++Index)
    {
        if (Index) Entries += TEXT(",");
        const auto* Component = Channels[Index].Get();
        Entries += FString::Printf(TEXT("{\"id\":\"%s\",\"sound\":\"%s\",\"playing\":%s,\"active\":%s}"),
            IsValid(Component) ? *Component->GetPathName() : TEXT(""),
            IsValid(Component) ? *GetPathNameSafe(Component->Sound) : TEXT(""),
            IsValid(Component) && Component->IsPlaying() ? TEXT("true") : TEXT("false"), Index == ActiveChannel ? TEXT("true") : TEXT("false"));
    }
    const UAudioComponent* UI = LastUIButtonComponent.Get();
    return FString::Printf(TEXT("{\"scene\":%d,\"sound\":\"%s\",\"channels\":[%s],\"ui_click\":%u,\"ui_hover\":%u,\"ui_last\":{\"id\":\"%s\",\"sound\":\"%s\",\"playing\":%s,\"is_ui\":%s,\"class\":\"%s\"}}"),
        int32(CurrentScene), *GetPathNameSafe(CurrentSound), *Entries, UIButtonPressedCount, UIButtonHoveredCount,
        UI ? *UI->GetPathName() : TEXT(""), UI ? *GetPathNameSafe(UI->Sound) : TEXT(""),
        UI && UI->IsPlaying() ? TEXT("true") : TEXT("false"), UI && UI->bIsUISound ? TEXT("true") : TEXT("false"),
        UI ? *GetPathNameSafe(UI->SoundClassOverride) : TEXT(""));
}

void URogueMusicSubsystem::ApplyVolumes()
{
    if (!CanPlayInWorld() || !LoadedConfig || !LoadedConfig->UserMix) return;
    UWorld* World = GetWorld();
    if (MixWorld.Get() != World)
    {
        UGameplayStatics::SetBaseSoundMix(World, LoadedConfig->UserMix);
        if (FAudioDevice* Device = World->GetAudioDeviceRaw()) Device->SetTransientPrimaryVolume(1.f);
        MixWorld = World;
    }
    auto Set = [&](USoundClass* Class, float Volume)
    { if (Class) UGameplayStatics::SetSoundMixClassOverride(World, LoadedConfig->UserMix, Class, Volume, 1, .05f, true); };
    Set(LoadedConfig->MusicClass, MusicVolume);
    Set(LoadedConfig->EffectsClass, SoundEffectsVolume);
    Set(LoadedConfig->UIClass, SoundEffectsVolume);
    // 音乐静音时暂停组件，避免零音量裁剪后循环重建；恢复音量可继续播放。
    for (UAudioComponent* Component : Channels) if (IsValid(Component)) Component->SetPaused(MusicVolume <= KINDA_SMALL_NUMBER);
}
