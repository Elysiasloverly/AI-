#include "Audio/RogueCombatAudioSubsystem.h"
#include "Audio/RogueMusicSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarAttackAudioDebug(TEXT("Rogue.Audio.Debug"), 0, TEXT("Log local attack audio requests and empty configurations."));

void URogueCombatAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    AudioRandom.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
    DefaultAttenuation = NewObject<USoundAttenuation>(this);
    auto& Attenuation = DefaultAttenuation->Attenuation;
    Attenuation.bAttenuate = true; Attenuation.bSpatialize = true;
    Attenuation.AttenuationShape = EAttenuationShape::Sphere;
    Attenuation.AttenuationShapeExtents = FVector(180, 0, 0);
    Attenuation.FalloffDistance = 2400;
    Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
    auto MakeConcurrency = [this](int32 Count)
    {
        auto* Rule = NewObject<USoundConcurrency>(this);
        Rule->Concurrency.MaxCount = Count;
        Rule->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
        Rule->Concurrency.VoiceStealReleaseTime = .03f;
        return Rule;
    };
    WeaponConcurrency = MakeConcurrency(16);
    EnemyConcurrency = MakeConcurrency(12);
}

bool URogueCombatAudioSubsystem::PlayAttack(AActor* Source, const FVector& Location,
    const FRogueAttackAudioSettings& Settings, FRogueAttackAudioState& State, bool bEnemy)
{
    UWorld* World = GetWorld();
    if (CVarAttackAudioDebug.GetValueOnGameThread())
        UE_LOG(LogTemp, Display, TEXT("[AttackAudio] Local Mode=%d Source=%s Enabled=%d HasSound=%d Volume=%.2f"),
            World ? int32(World->GetNetMode()) : -1, *GetNameSafe(Source), Settings.bEnabled, Settings.HasSound(), Settings.VolumeMultiplier);
    if (!World || World->GetNetMode() == NM_DedicatedServer || !IsValid(Source) || Location.ContainsNaN()) return false;
    FRogueAttackAudioRequest Request;
    if (!RogueAttackAudio::Prepare(Settings, State, World->GetTimeSeconds(), AudioRandom, Request)) return false;
#if WITH_DEV_AUTOMATION_TESTS
    RogueAttackAudio::OnPrepared().Broadcast(this, Source, Request);
#endif
    // 在 Play 之前明确路由至 SFX；不修改用户的 SoundWave / SoundCue 分类。
    auto* Component = NewObject<UAudioComponent>(World);
    Component->bAutoActivate = false; Component->bAutoDestroy = true;
    Component->bStopWhenOwnerDestroyed = false;
    if (auto* Instance = World->GetGameInstance())
        if (auto* Audio = Instance->GetSubsystem<URogueMusicSubsystem>()) Component->SoundClassOverride = Audio->GetEffectsClass();
    Component->SetSound(Request.Sound); Component->SetVolumeMultiplier(Request.Volume); Component->SetPitchMultiplier(Request.Pitch);
    Component->AttenuationSettings = Settings.Attenuation ? Settings.Attenuation.Get() : DefaultAttenuation.Get();
    Component->ConcurrencySet.Add(Settings.Concurrency ? Settings.Concurrency.Get() : (bEnemy ? EnemyConcurrency.Get() : WeaponConcurrency.Get()));
    Component->SetWorldLocation(Location); Component->RegisterComponentWithWorld(World); Component->Play();
    return true;
}

const URogueAudioConfig* URogueCombatAudioSubsystem::ResolveConfig() const
{
    const UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const auto* Audio = Instance ? Instance->GetSubsystem<URogueMusicSubsystem>() : GetDefault<URogueMusicSubsystem>();
    return Audio ? Audio->GetAudioConfig() : nullptr;
}

bool URogueCombatAudioSubsystem::PlayWeaponAttack(AActor* Source, const FVector& Location, FRogueAttackAudioState& State)
{
    const auto* Config = ResolveConfig();
    const auto* Settings = Config && Source ? Config->FindWeaponAudio(Source->GetClass()) : nullptr;
    return Settings && PlayAttack(Source, Location, *Settings, State, false);
}

const FRogueAttackAudioSettings& URogueCombatAudioSubsystem::GetEnemyAttackSettings(const UClass* Class) const
{
    const auto* Config = ResolveConfig();
    if (const auto* Settings = Config ? Config->FindEnemyAudio(Class) : nullptr) return *Settings;
    static const FRogueAttackAudioSettings Empty;
    return Empty;
}

bool URogueCombatAudioSubsystem::PlayEnemyAttack(AActor* Source, const FVector& Location, FRogueAttackAudioState& State)
{
    return Source && PlayAttack(Source, Location, GetEnemyAttackSettings(Source->GetClass()), State, true);
}
