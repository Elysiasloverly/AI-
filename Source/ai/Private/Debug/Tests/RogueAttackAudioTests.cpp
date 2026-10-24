#if WITH_DEV_AUTOMATION_TESTS
#include "Audio/RogueAttackAudioSettings.h"
#include "Audio/RogueMusicSubsystem.h"
#include "Audio/RogueCombatAudioSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Combat/Weapons/RogueWeapon_Rocket.h"
#include "Combat/Weapons/RogueWeapon_Mortar.h"
#include "Combat/Weapons/RogueWeapon_Laser.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Combat/Weapons/RogueWeapon_Scythe.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Sound/SoundWave.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

struct FRogueAttackAudioTestAccess
{
    static FRogueAttackAudioSettings& Settings(ARogueEnemy* E)
    {
        auto* Config = GetDefault<URogueMusicSubsystem>()->GetAudioConfig();
        for (auto& Entry : Config->Enemies) if (Entry.EnemyClass.Get() == E->GetClass()) return Entry.AttackAudio;
        auto& Entry = Config->Enemies.AddDefaulted_GetRef(); Entry.EnemyClass = E->GetClass(); return Entry.AttackAudio;
    }
    static const FRogueAttackAudioSettings& Resolved(ARogueEnemy* E) { return E->GetWorld()->GetSubsystem<URogueCombatAudioSubsystem>()->GetEnemyAttackSettings(E->GetClass()); }
    static void Receive(ARogueEnemy* E, uint32 Generation) { E->Multicast_AttackAudio_Implementation(E->GetActorLocation(), Generation); }
};

namespace RogueAttackAudioTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
        FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
        FTestWorldWrapper TestWorld;
        UWorld* World = nullptr;
        ARogueCharacter* Player = nullptr;
        USoundWave* Sound = nullptr;
        URogueAudioConfig* AudioConfig = nullptr;
        TArray<FRogueWeaponAudioEntry> SavedWeapons;
        TArray<FRogueEnemyAudioEntry> SavedEnemies;
        TMap<AActor*, int32> Requests;
        FDelegateHandle Observer;
        ~FFixture() { if (AudioConfig) { AudioConfig->Weapons = SavedWeapons; AudioConfig->Enemies = SavedEnemies; } RogueAttackAudio::OnPrepared().Remove(Observer); }
        bool Initialize()
        {
            SelfTest.Set(TEXT("0")); Stress.Set(TEXT("0"));
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) return false;
            World = TestWorld.GetTestWorld(); World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
            if (!TestWorld.BeginPlayInTestWorld()) return false;
            World->GetAuthGameMode()->SetActorTickEnabled(false);
            World->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);
            Sound = NewObject<USoundWave>(World);
            auto* Table = NewObject<UDataTable>(World); Table->RowStruct = FRogueWeaponTableRow::StaticStruct();
            const TSubclassOf<ARogueWeaponBase> Classes[] = {ARogueWeapon_Projectile::StaticClass(), ARogueWeapon_Rocket::StaticClass(),
                ARogueWeapon_Mortar::StaticClass(), ARogueWeapon_Laser::StaticClass(), ARogueWeapon_HellTower::StaticClass(), ARogueWeapon_Scythe::StaticClass()};
            const FName Names[] = {TEXT("Projectile"), TEXT("Rocket"), TEXT("Mortar"), TEXT("Laser"), TEXT("HellTower"), TEXT("Scythe")};
            for (int32 I = 0; I < 6; ++I)
            {
                FRogueWeaponTableRow Row; Row.WeaponClass = Classes[I]; Row.Count = 3; Row.Range = 3000; Row.Damage = 1;
                Row.RefractionCount = 2; Row.RefractionRange = 2000; Row.ProjectileSpeed = 0;
                if (!AudioConfig) { AudioConfig = GetDefault<URogueMusicSubsystem>()->GetAudioConfig(); if (!AudioConfig) return false; SavedWeapons = AudioConfig->Weapons; SavedEnemies = AudioConfig->Enemies; AudioConfig->Weapons.Reset(); AudioConfig->Enemies.Reset(); }
                FRogueWeaponAudioEntry Entry; Entry.WeaponClass = Classes[I]; Entry.AttackAudio.Sounds[0] = Sound; Entry.AttackAudio.MinimumInterval = 0; AudioConfig->Weapons.Add(Entry);
                Table->AddRow(Names[I], Row);
            }
            auto* PC = World->SpawnActor<ARoguePlayerController>();
            const FTransform Transform(FVector(0, 0, 300));
            Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(), TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(Player, Table);
            Player->AutoPossessPlayer = EAutoReceiveInput::Disabled; Player->FinishSpawning(Transform); PC->Possess(Player);
            Player->SetActorTickEnabled(false); Player->GetCharacterMovement()->SetComponentTickEnabled(false); Player->SetActorEnableCollision(false);
            Player->GetAbilitySystemComponent()->CancelAllAbilities(); Player->ApplyWeaponBonusSnapshot(FRogueWeaponBonusValues());
            for (ARogueWeaponBase* Weapon : Player->GetWeapons()) Weapon->GetConfig().Count = 3;
            Observer = RogueAttackAudio::OnPrepared().AddLambda([this](const UObject* Context, AActor* Source, const FRogueAttackAudioRequest&)
            { if (Context->GetWorld() == World) ++Requests.FindOrAdd(Source); });
            return true;
        }
        ARogueEnemy* Enemy(FVector Location, UClass* Class = ARogueEnemy::StaticClass())
        {
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* E = World->SpawnActor<ARogueEnemy>(Class, Location, FRotator::ZeroRotator, Params);
            E->ActivatePooledEnemy(nullptr, Location, FRotator::ZeroRotator);
            FRogueEnemyProfile P; P.Health = 1000000; E->InitializeEnemy(P); E->SetActorTickEnabled(false);
            E->GetCharacterMovement()->SetComponentTickEnabled(false); E->SetActorEnableCollision(false);
            World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E); return E;
        }
        void Step(float Time) { while (Time > KINDA_SMALL_NUMBER) { const float Dt = FMath::Min(Time, .02f); TestWorld.TickTestWorld(Dt); Time -= Dt; } }
        int32 Count(AActor* Source) const { return Requests.FindRef(Source); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAudioSelectionTest, "Rogue.Audio.Attack.VariantsEmptyAndThrottle", RogueAttackAudioTests::Flags)
bool FRogueAudioSelectionTest::RunTest(const FString& Parameters)
{
    FRogueAttackAudioSettings S; FRogueAttackAudioState State; FRogueAttackAudioRequest Request; FRandomStream Random(73);
    TestEqual(TEXT("New configuration provides three empty slots"), S.Sounds.Num(), 3);
    TestFalse(TEXT("Empty audio does nothing"), RogueAttackAudio::Prepare(S, State, 0, Random, Request));
    auto* A = NewObject<USoundWave>(); auto* B = NewObject<USoundWave>();
    S.Sounds = {nullptr, A, B, A}; S.PitchMin = 1.1f; S.PitchMax = .9f;
    USoundBase* Previous = nullptr;
    for (int32 I = 0; I < 20; ++I)
    {
        TestTrue(TEXT("A valid variation is selected"), RogueAttackAudio::Prepare(S, State, I, Random, Request));
        TestTrue(TEXT("Null slots and immediate repeats are excluded"), Request.Sound && Request.Sound != Previous);
        TestTrue(TEXT("Reversed pitch bounds are normalized"), Request.Pitch >= .9f && Request.Pitch <= 1.1f);
        Previous = Request.Sound;
    }
    TestFalse(TEXT("The same source is throttled without affecting damage"), RogueAttackAudio::Prepare(S, State, 19.01, Random, Request));
    State.Reset(); TestTrue(TEXT("A new lifecycle can play immediately"), RogueAttackAudio::Prepare(S, State, 19.01, Random, Request));
    S.bEnabled = false; TestFalse(TEXT("Explicit mute is respected"), RogueAttackAudio::Prepare(S, State, 20, Random, Request));
    S.bEnabled = true; S.VolumeMultiplier = 0; TestFalse(TEXT("Zero volume never starts a component"), RogueAttackAudio::Prepare(S, State, 20, Random, Request));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAudioWeaponCadenceTest, "Rogue.Audio.Attack.ActualWeaponsVolleyBurstAndContinuous", RogueAttackAudioTests::Flags)
bool FRogueAudioWeaponCadenceTest::RunTest(const FString& Parameters)
{
    using namespace RogueAttackAudioTests;
    FFixture F; if (!F.Initialize()) return false;
    auto* E = F.Enemy(FVector(700, 0, 300)); auto* E2 = F.Enemy(FVector(800, 150, 300));
    ARogueWeaponBase* Discrete[] = {F.Player->FindWeapon<ARogueWeapon_Projectile>(), F.Player->FindWeapon<ARogueWeapon_Rocket>(),
        F.Player->FindWeapon<ARogueWeapon_Mortar>(), F.Player->FindWeapon<ARogueWeapon_Laser>()};
    for (auto* W : Discrete)
    {
        if (!TestNotNull(TEXT("Real weapon initialized"), W)) return false;
        W->FireAtTargets({E, E2}); TestEqual(TEXT("Three projectiles or refracting beams produce one attack sound"), F.Count(W), 1);
        F.Player->FlushWeaponCosmetics(); TestEqual(TEXT("Listen-server multicast does not replay the host sound"), F.Count(W), 1);
    }
    auto* Projectile = F.Player->FindWeapon<ARogueWeapon_Projectile>(); F.Requests.Reset();
    FRogueWeaponBonusValues Bonus; Bonus.ProjectileBurst = 2; F.Player->ApplyWeaponBonusSnapshot(Bonus);
    Projectile->FireAtTargets({E}); F.Step(.35f);
    TestEqual(TEXT("A three-round burst emits three sounds, not nine pellets"), F.Count(Projectile), 3);
    auto* Tower = F.Player->FindWeapon<ARogueWeapon_HellTower>(); F.Requests.Reset();
    Tower->WeaponTick(.05f); TestEqual(TEXT("Multiple tower locks share one sound at refresh"), F.Count(Tower), 1);
    Tower->WeaponTick(.05f); TestEqual(TEXT("Damage-only ticks do not emit beam sounds"), F.Count(Tower), 1);
    auto* Scythe = F.Player->FindWeapon<ARogueWeapon_Scythe>(); F.Requests.Reset();
    Scythe->WeaponTick(0); for (int32 I = 0; I < 30; ++I) Scythe->WeaponTick(0);
    TestEqual(TEXT("Three orbiting blades do not emit sound every frame"), F.Count(Scythe), 1);
    for (int32 I = 0; I < 30; ++I) { F.Step(.1f); Scythe->WeaponTick(.1f); }
    TestEqual(TEXT("Orbit sound repeats once after a full revolution"), F.Count(Scythe), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAudioEnemyInheritanceTest, "Rogue.Audio.Attack.EnemyBlueprintInheritanceAttacksAndPool", RogueAttackAudioTests::Flags)
bool FRogueAudioEnemyInheritanceTest::RunTest(const FString& Parameters)
{
    using namespace RogueAttackAudioTests; using Access = FRogueAttackAudioTestAccess;
    FFixture F; if (!F.Initialize()) return false;
    UClass* Base = LoadClass<ARogueEnemy>(nullptr, TEXT("/Game/BP/Enemy/BP_EnemyBase.BP_EnemyBase_C"));
    UClass* Child = LoadClass<ARogueEnemy>(nullptr, TEXT("/Game/BP/Enemy/BP_EnemyRifleman.BP_EnemyRifleman_C"));
    if (!TestTrue(TEXT("Saved enemy child derives from saved base"), Base && Child && Child->IsChildOf(Base))) return false;
    auto* Defaults = Cast<ARogueEnemy>(Base->GetDefaultObject());
    struct FRestore { ARogueEnemy* Object; FRogueAttackAudioSettings Saved; ~FRestore() { Access::Settings(Object) = Saved; } } Restore{Defaults, Access::Settings(Defaults)};
    Access::Settings(Defaults).Sounds[0] = F.Sound;
    auto* Enemy = F.Enemy(FVector(900, 0, 300), Child); Access::Settings(Enemy).Sounds.Empty();
    TestTrue(TEXT("Empty child slots fall back to configured base"), Access::Resolved(Enemy).Sounds.Contains(F.Sound));
    auto* Different = NewObject<USoundWave>(F.World); Access::Settings(Enemy).Sounds = {Different};
    TestTrue(TEXT("A child may choose a specific sound"), Access::Resolved(Enemy).Sounds.Contains(Different));
    Access::Settings(Enemy).bEnabled = false;
    TestFalse(TEXT("Explicit mute prevents base fallback"), Access::Resolved(Enemy).bEnabled);
    Access::Settings(Enemy).bEnabled = true; Access::Settings(Enemy).MinimumInterval = 0;
    TestFalse(TEXT("Contact without a nearby player does not emit audio"), Enemy->ExecuteContactAttack());
    TestEqual(TEXT("Missed contact remains silent"), F.Count(Enemy), 0);
    Enemy->SetActorLocation(F.Player->GetActorLocation()); Enemy->ExecuteContactAttack();
    TestEqual(TEXT("One successful contact emits one sound"), F.Count(Enemy), 1);
    Enemy->SetActorLocation(F.Player->GetActorLocation() + FVector(500, 0, 0)); Enemy->Tick(.01f); Enemy->ExecuteRangedAttack();
    TestEqual(TEXT("A ranged volley emits one sound"), F.Count(Enemy), 2);
    const uint32 OldGeneration = Enemy->GetPresentationGeneration();
    Enemy->DeactivateToPool(); Enemy->ActivatePooledEnemy(nullptr, FVector(500,0,300), FRotator::ZeroRotator);
    FRogueEnemyProfile Profile; Profile.Health = 1000; Enemy->InitializeEnemy(Profile);
    Access::Settings(Enemy).MinimumInterval = 1000;
    Access::Receive(Enemy, OldGeneration); TestEqual(TEXT("Late audio from previous pool occupant is discarded"), F.Count(Enemy), 2);
    Access::Receive(Enemy, Enemy->GetPresentationGeneration()); TestEqual(TEXT("New pool generation plays immediately"), F.Count(Enemy), 3);
    Enemy->DeactivateToPool(); Access::Receive(Enemy, Enemy->GetPresentationGeneration());
    TestEqual(TEXT("Pooled hidden enemy cannot emit attacks"), F.Count(Enemy), 3);
    return true;
}
#endif
