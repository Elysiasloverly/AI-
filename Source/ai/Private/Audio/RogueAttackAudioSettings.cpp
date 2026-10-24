#include "Audio/RogueAttackAudioSettings.h"
#include "Sound/SoundBase.h"

bool FRogueAttackAudioSettings::HasSound() const
{
    for (const auto& Sound : Sounds) if (IsValid(Sound.Get())) return true;
    return false;
}

bool RogueAttackAudio::Prepare(const FRogueAttackAudioSettings& Settings, FRogueAttackAudioState& State,
    double Now, FRandomStream& Random, FRogueAttackAudioRequest& Out)
{
    Out = FRogueAttackAudioRequest();
    if (!Settings.bEnabled || !FMath::IsFinite(Now) || !FMath::IsFinite(Settings.VolumeMultiplier) || Settings.VolumeMultiplier <= 0) return false;
    const double Interval = FMath::IsFinite(Settings.MinimumInterval) ? FMath::Max(0.f, Settings.MinimumInterval) : .06f;
    if (Now >= State.LastTime && Now + 1.e-6 < State.LastTime + Interval) return false;
    TArray<USoundBase*, TInlineAllocator<8>> Choices;
    for (const auto& Sound : Settings.Sounds) if (IsValid(Sound.Get())) Choices.AddUnique(Sound.Get());
    if (Choices.IsEmpty()) return false;
    if (Choices.Num() > 1) Choices.Remove(State.LastSound.Get());
    Out.Sound = Choices[Random.RandRange(0, Choices.Num() - 1)];
    const float A = FMath::IsFinite(Settings.PitchMin) ? FMath::Clamp(Settings.PitchMin, .1f, 4.f) : 1.f;
    const float B = FMath::IsFinite(Settings.PitchMax) ? FMath::Clamp(Settings.PitchMax, .1f, 4.f) : 1.f;
    Out.Pitch = Random.FRandRange(FMath::Min(A, B), FMath::Max(A, B));
    Out.Volume = FMath::Clamp(Settings.VolumeMultiplier, 0.f, 4.f);
    State.LastTime = Now;
    State.LastSound = Out.Sound;
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS
RogueAttackAudio::FOnPrepared& RogueAttackAudio::OnPrepared()
{
    static FOnPrepared Event;
    return Event;
}
#endif
