#if WITH_DEV_AUTOMATION_TESTS
// 仅显式执行 Rogue.AudioProbe 才会修改本局临时配置，不保存任何音频或蓝图资产。
#include "Audio/RogueAttackAudioSettings.h"
#include "Audio/RogueMusicSubsystem.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemComponent.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Player/RogueCharacter.h"
#include "Sound/SoundWave.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace RogueAttackAudioProbe
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogueEnemy> Enemy;
    TStrongObjectPtr<USoundWave> Sound;
    TMap<int32, int32> PlayerCounts;
    int32 WeaponCount = 0, EnemyCount = 0;
    FDelegateHandle Observer, Cleanup;

    void Configure(FRogueAttackAudioSettings& Settings)
    {
        Settings.bEnabled = true; Settings.Sounds = {Sound.Get(), nullptr, nullptr};
        Settings.MinimumInterval = 0; Settings.VolumeMultiplier = .1f;
    }
    void Reset()
    {
        RogueAttackAudio::OnPrepared().Remove(Observer); Observer.Reset();
        FWorldDelegates::OnWorldCleanup.Remove(Cleanup); Cleanup.Reset();
        Sound.Reset(); World.Reset(); Enemy.Reset(); PlayerCounts.Reset(); WeaponCount = EnemyCount = 0;
    }
    void Execute(const TArray<FString>& Args, UWorld* InWorld)
    {
        if (!InWorld || Args.IsEmpty()) return;
        if (Args[0] == TEXT("setup"))
        {
            Reset(); World = InWorld; Sound.Reset(NewObject<USoundWave>(InWorld));
            if (auto* Spawn = InWorld->GetSubsystem<URogueSpawnSubsystem>()) Spawn->SetSpawningSuspended(true);
            for (TActorIterator<ARogueEnemy> It(InWorld); It; ++It) if (It->HasAuthority()) It->DeactivateToPool();
            if (auto* Music = InWorld->GetGameInstance()->GetSubsystem<URogueMusicSubsystem>())
            {
                auto* Config = DuplicateObject<URogueAudioConfig>(Music->GetAudioConfig(), Music);
                for (auto& Entry : Config->Weapons) Configure(Entry.AttackAudio);
                for (auto& Entry : Config->Enemies) Configure(Entry.AttackAudio);
                Music->SetMusicConfig(Config);
            }
            for (TActorIterator<ARogueCharacter> It(InWorld); It; ++It)
            {
                It->SetActorTickEnabled(true); It->GetCharacterMovement()->SetComponentTickEnabled(false);
                if (auto* ASC = It->GetAbilitySystemComponent()) ASC->CancelAllAbilities();
                for (ARogueWeaponBase* Weapon : It->GetWeapons())
                {
                    Weapon->OnOwnerDied();
                    Weapon->GetConfig().Count = Cast<ARogueWeapon_Projectile>(Weapon) ? 3 : 0;
                }
                UE_LOG(LogTemp, Display, TEXT("[AudioProbe] Setup %s Alive=%d Weapons=%d"), *It->GetName(), It->IsAlive(), It->GetWeapons().Num());
            }
            Observer = RogueAttackAudio::OnPrepared().AddLambda([](const UObject* Context, AActor* Source, const FRogueAttackAudioRequest&)
            {
                if (Context->GetWorld() != World.Get()) return;
                if (Cast<ARogueWeaponBase>(Source))
                {
                    ++WeaponCount;
                    if (auto* Player = Cast<ARogueCharacter>(Source->GetOwner()))
                        if (auto* State = Player->GetPlayerState()) ++PlayerCounts.FindOrAdd(State->GetPlayerId());
                }
                if (Cast<ARogueEnemy>(Source)) ++EnemyCount;
            });
            Cleanup = FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* Cleaning, bool, bool) { if (Cleaning == World.Get()) Reset(); });
        }
        else if (Args[0] == TEXT("spawn") && InWorld == World.Get() && InWorld->GetNetMode() != NM_Client)
        {
            ARogueCharacter* First = nullptr;
            if (auto* Music = InWorld->GetGameInstance()->GetSubsystem<URogueMusicSubsystem>())
            {
                auto* Config = DuplicateObject<URogueAudioConfig>(Music->GetAudioConfig(), Music);
                for (auto& Entry : Config->Weapons) Configure(Entry.AttackAudio);
                for (auto& Entry : Config->Enemies) Configure(Entry.AttackAudio);
                Music->SetMusicConfig(Config);
            }
            for (TActorIterator<ARogueCharacter> It(InWorld); It; ++It) { First = *It; break; }
            if (!First) return;
            const FVector Location = First->GetActorLocation() + FVector(700, 0, 0);
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Enemy = InWorld->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(), Location, FRotator::ZeroRotator, Params);
            Enemy->ActivatePooledEnemy(nullptr, Location, FRotator::ZeroRotator);
            FRogueEnemyProfile Profile; Profile.Health = 1000000; Enemy->InitializeEnemy(Profile);
            Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
            Enemy->SetNetDormancy(DORM_Awake); Enemy->ForceNetUpdate();
        }
        else if (Args[0] == TEXT("fire") && Enemy.IsValid() && InWorld->GetNetMode() != NM_Client)
        {
            FTimerHandle Delay;
            InWorld->GetTimerManager().SetTimer(Delay, [InWorld]() { Execute({TEXT("fire_now")}, InWorld); }, .1f, false);
        }
        else if (Args[0] == TEXT("fire_now") && Enemy.IsValid() && InWorld->GetNetMode() != NM_Client)
        {
            if (auto* Music = InWorld->GetGameInstance()->GetSubsystem<URogueMusicSubsystem>())
            {
                auto* Config = DuplicateObject<URogueAudioConfig>(Music->GetAudioConfig(), Music);
                for (auto& Entry : Config->Weapons) Configure(Entry.AttackAudio);
                for (auto& Entry : Config->Enemies) Configure(Entry.AttackAudio);
                Music->SetMusicConfig(Config);
            }
            for (TActorIterator<ARogueCharacter> It(InWorld); It; ++It)
            {
                It->ApplyWeaponBonusSnapshot(FRogueWeaponBonusValues());
                if (auto* Weapon = It->FindWeapon<ARogueWeapon_Projectile>())
                {
                    UE_LOG(LogTemp, Display, TEXT("[AudioProbe] Fire %s Alive=%d Count=%d Sound=%d CanFire=%d"), *It->GetName(), It->IsAlive(),
                        Weapon->GetEffectiveCount(), true, Weapon->CanFireNow());
                    UE_LOG(LogTemp, Display, TEXT("[AudioProbe] Network %s Replicated=%d Role=%d Remote=%d Dormancy=%d"), *It->GetName(), It->GetIsReplicated(), int32(It->GetLocalRole()), int32(It->GetRemoteRole()), int32(It->NetDormancy));
                    It->SetNetDormancy(DORM_Awake); It->ForceNetUpdate();
                    if (auto* Driver = InWorld->GetNetDriver())
                        for (UNetConnection* Connection : Driver->ClientConnections)
                            UE_LOG(LogTemp, Display, TEXT("[AudioProbe] Connection View=%s Driver=%s ActorDriver=%s FunctionFlags=%u Relevant=%d"),
                                *GetNameSafe(Connection->ViewTarget), *Driver->NetDriverName.ToString(), *It->GetNetDriverName().ToString(),
                                uint32(It->FindFunction(TEXT("Multicast_WeaponCosmetics"))->FunctionFlags),
                                It->IsNetRelevantFor(Connection->PlayerController, Connection->ViewTarget, It->GetActorLocation()));
                    Weapon->FireAtTargets({Enemy.Get()}); It->FlushWeaponCosmetics();
                }
            }
            Enemy->Tick(.01f);
            Enemy->ExecuteRangedAttack();
            UE_LOG(LogTemp, Display, TEXT("[AudioProbe] Confirmed requests weapons=%d enemy=%d"), WeaponCount, EnemyCount);
        }
        else if (Args[0] == TEXT("status") && Args.Num() > 1)
        {
            FString Counts;
            for (const auto& Pair : PlayerCounts) { if (!Counts.IsEmpty()) Counts += TEXT(","); Counts += FString::Printf(TEXT("\"%d\":%d"), Pair.Key, Pair.Value); }
            const FString Json = FString::Printf(TEXT("{\"mode\":%d,\"weapon_requests\":%d,\"enemy_requests\":%d,\"players\":{%s}}"),
                int32(InWorld->GetNetMode()), WeaponCount, EnemyCount, *Counts);
            FFileHelper::SaveStringToFile(Json, *Args[1], FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        }
    }
    FAutoConsoleCommandWithWorldAndArgs Command(TEXT("Rogue.AudioProbe"), TEXT("Isolated local attack-audio regression: setup/spawn/fire/status <file>"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Execute));
}
#endif
