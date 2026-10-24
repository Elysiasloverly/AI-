#if WITH_DEV_AUTOMATION_TESTS
#include "Audio/RogueMusicSubsystem.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#include "UObject/UnrealType.h"

struct FRogueMusicTestAccess
{
    static void Load(URogueMusicSubsystem* Audio) { Audio->LoadVolumes(); }
};
constexpr EAutomationTestFlags MusicTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueCentralAudioTest, "Rogue.Audio.System.CentralAssetInheritanceAndStates", MusicTestFlags)
bool FRogueCentralAudioTest::RunTest(const FString&)
{
    auto* Saved = GetDefault<URogueMusicSubsystem>()->GetAudioConfig();
    if (!TestNotNull(TEXT("Configured central audio asset loads"), Saved)) return false;
    TestEqual(TEXT("All six existing weapons have one central entry"), Saved->Weapons.Num(), 6);
    TestTrue(TEXT("Native enemy default plus existing blueprints are migrated"), Saved->Enemies.Num() >= 15);
    TestTrue(TEXT("Music and effects are separate child classes"), Saved->MusicClass && Saved->EffectsClass && Saved->MusicClass != Saved->EffectsClass);
    TestTrue(TEXT("Music and effects share the project master"), Saved->MusicClass->ParentClass == Saved->EffectsClass->ParentClass);
    TestFalse(TEXT("Legacy weapon audio is no longer an editable second source"), FindFProperty<FProperty>(FRogueWeaponTableRow::StaticStruct(), TEXT("AttackAudio"))->HasAnyPropertyFlags(CPF_Edit));
    TestFalse(TEXT("Legacy enemy audio is no longer an editable second source"), FindFProperty<FProperty>(ARogueEnemy::StaticClass(), TEXT("AttackAudio"))->HasAnyPropertyFlags(CPF_Edit));
    auto* Config = DuplicateObject<URogueAudioConfig>(Saved, GetTransientPackage());
    auto* A = NewObject<USoundWave>(); auto* B = NewObject<USoundWave>();
    Config->Enemies.Reset();
    auto& Base = Config->Enemies.AddDefaulted_GetRef(); Base.EnemyClass = ARogueEnemy::StaticClass(); Base.AttackAudio.Sounds = {A};
    auto* Child = LoadClass<ARogueEnemy>(nullptr, TEXT("/Game/BP/Enemy/BP_EnemyRifleman.BP_EnemyRifleman_C"));
    auto& Specific = Config->Enemies.AddDefaulted_GetRef(); Specific.EnemyClass = Child;
    TestTrue(TEXT("Empty specific entry inherits central base sound"), Config->FindEnemyAudio(Child)->Sounds.Contains(A));
    Specific.AttackAudio.Sounds = {B}; TestTrue(TEXT("Specific enemy overrides only itself"), Config->FindEnemyAudio(Child)->Sounds.Contains(B));
    Specific.AttackAudio.bEnabled = false; TestFalse(TEXT("Explicit mute blocks fallback"), Config->FindEnemyAudio(Child)->bEnabled);
    auto* Weapon = Config->FindWeaponAudio(ARogueWeapon_Projectile::StaticClass());
    TestNotNull(TEXT("Weapon audio resolves from its class, not a display name"), Weapon);
    TestEqual(TEXT("Boss takes precedence over night"), URogueMusicSubsystem::ResolveBattleScene(false, true, true), ERogueMusicScene::Boss);
    TestEqual(TEXT("Game over takes precedence over boss"), URogueMusicSubsystem::ResolveBattleScene(true, true, true), ERogueMusicScene::Results);
    TestEqual(TEXT("Boss ending returns to current stage"), URogueMusicSubsystem::ResolveBattleScene(false, false, true), ERogueMusicScene::Night);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAudioVolumePersistenceTest, "Rogue.Audio.System.IndependentVolumesPersistenceAndMigration", MusicTestFlags)
bool FRogueAudioVolumePersistenceTest::RunTest(const FString&)
{
    struct FSettingsGuard
    {
        FString Previous = GGameUserSettingsIni;
        FString Temporary = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/AudioVolumes-") + FGuid::NewGuid().ToString() + TEXT(".ini"));
        FSettingsGuard() { GConfig->Add(Temporary, FConfigFile()); GGameUserSettingsIni = Temporary; }
        ~FSettingsGuard() { GConfig->UnloadFile(Temporary); IFileManager::Get().Delete(*Temporary); GGameUserSettingsIni = Previous; }
    } Guard;
    const TCHAR* Section = TEXT("/Script/ai.RogueRuntimeSettings");
    GConfig->SetFloat(Section, TEXT("MasterVolume"), .4f, GGameUserSettingsIni);
    auto* Instance = NewObject<UGameInstance>(GEngine);
    auto* Audio = NewObject<URogueMusicSubsystem>(Instance);
    FRogueMusicTestAccess::Load(Audio);
    TestEqual(TEXT("Legacy master volume migrates to music"), Audio->GetMusicVolume(), .4f);
    TestEqual(TEXT("Legacy master volume migrates to effects"), Audio->GetSoundEffectsVolume(), .4f);
    Audio->SetMusicVolume(.2f); Audio->SetSoundEffectsVolume(.8f);
    TestEqual(TEXT("Changing effects leaves music unchanged"), Audio->GetMusicVolume(), .2f);
    GConfig->UnloadFile(Guard.Temporary); GConfig->LoadFile(Guard.Temporary);
    auto* Reloaded = NewObject<URogueMusicSubsystem>(Instance); FRogueMusicTestAccess::Load(Reloaded);
    TestEqual(TEXT("Music persists independently"), Reloaded->GetMusicVolume(), .2f);
    TestEqual(TEXT("Effects persists independently"), Reloaded->GetSoundEffectsVolume(), .8f);
    Reloaded->SetMusicVolume(-2); Reloaded->SetSoundEffectsVolume(8);
    TestEqual(TEXT("Music mute is supported"), Reloaded->GetMusicVolume(), 0.f);
    TestEqual(TEXT("Effects are clamped"), Reloaded->GetSoundEffectsVolume(), 1.f);
    return true;
}
#endif
