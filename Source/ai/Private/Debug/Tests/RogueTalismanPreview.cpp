#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Weapons/RogueWeapon_Talisman.h"
#include "Combat/Projectiles/RogueReturningTalisman.h"
#include "Components/StaticMeshComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"

namespace RogueTalismanPreview
{
    UWorld* World()
    {
        if(GEngine)for(const auto& Context:GEngine->GetWorldContexts())
            if(Context.World() && Context.World()->IsGameWorld())return Context.World();
        return nullptr;
    }
}

// 仅显式开发命令调用：不保存档案和地图；用于真实 BP 外观、三态与双进程复制检查。
static FAutoConsoleCommand TalismanPreview(TEXT("Rogue.Test.Talisman"),TEXT("Isolated real talisman preview. Optional initialized player count."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        const int32 Count=Args.Num()?FMath::Clamp(FCString::Atoi(*Args[0]),1,4):1;
        const double Start=FPlatformTime::Seconds();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Count,Start](float)
        {
            if(FPlatformTime::Seconds()-Start>90){UE_LOG(LogTemp,Error,TEXT("[TalismanPreview] FAIL timeout"));return false;}
            auto* W=RogueTalismanPreview::World();if(!W || !W->GetAuthGameMode<ARogueGameMode>())return true;
            W->GetAuthGameMode()->SetActorTickEnabled(false);
            if(auto* Spawn=W->GetSubsystem<URogueSpawnSubsystem>())Spawn->SetSpawningSuspended(true);
            TArray<ARogueCharacter*> Players;
            for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
            {
                auto* P=It->Get()?Cast<ARogueCharacter>(It->Get()->GetPawn()):nullptr;
                auto* State=P?P->GetPlayerState<ARoguePlayerState>():nullptr;
                if(P && State && State->IsBaseAttributesInitialized())Players.Add(P);
            }
            if(Players.Num()<Count)return true;
            for(TActorIterator<ARogueEnemy> It(W);It;++It)It->DeactivateToPool();
            TArray<TWeakObjectPtr<ARogueWeapon_Talisman>> Weapons;
            const FVector Origin=Players[0]->GetActorLocation();
            for(int32 I=0;I<Players.Num();++I)
            {
                auto* P=Players[I];P->SetActorLocation(Origin+FVector(0,I*300,0));
                P->GetAbilitySystemComponent()->CancelAllAbilities();P->SetActorTickEnabled(true);
                // 保留角色正常的表现批次 Flush；先归零基础数量，避免编辑器调试开局影响两张灵符的演示。
                for(ARogueWeaponBase* Other:P->GetWeapons())if(Other)Other->GetConfig().Count=0;
                P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetComponentTickEnabled(false);
                FRogueUpgradeOption Upgrade;Upgrade.Magnitude=1;Upgrade.Type=ERogueUpgradeType::TalismanCount;
                P->ApplyUpgrade(Upgrade);P->ApplyUpgrade(Upgrade);
                Upgrade.Type=ERogueUpgradeType::TalismanPierce;P->ApplyUpgrade(Upgrade);
                Upgrade.Type=ERogueUpgradeType::TalismanRepeat;P->ApplyUpgrade(Upgrade);
                auto* Weapon=P->FindWeapon<ARogueWeapon_Talisman>();if(!Weapon){UE_LOG(LogTemp,Error,TEXT("[TalismanPreview] FAIL missing weapon table"));return false;}
                Weapons.Add(Weapon);
            }
            UClass* Class=LoadClass<ARogueEnemy>(nullptr,TEXT("/Game/BP/Enemy/Rifleman/BP_EnemyRifleman.BP_EnemyRifleman_C"));
            for(int32 I=0;I<3;++I)
            {
                FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                const FVector Location=Origin+FVector(520+I*220,-220+I*210,0);
                auto* Enemy=W->SpawnActor<ARogueEnemy>(Class,Location,FRotator::ZeroRotator,Params);
                if(!Enemy)continue;Enemy->ActivatePooledEnemy(nullptr,Location,FRotator::ZeroRotator);
                FRogueEnemyProfile Profile;Profile.Health=100000;Profile.Damage=0;Profile.Speed=0;Enemy->InitializeEnemy(Profile);
                W->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(Enemy);
                Enemy->SetActorTickEnabled(false);Enemy->GetCharacterMovement()->StopMovementImmediately();Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
            }
            const double Ready=FPlatformTime::Seconds();TWeakObjectPtr<UWorld> WeakWorld=W;
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weapons,WeakWorld,Ready](float)
            {
                if(!WeakWorld.IsValid() || FPlatformTime::Seconds()-Ready>45)return false;
                for(auto Weapon:Weapons)if(Weapon.IsValid())Weapon->WeaponTick(.03f);
                static double LastLog=0;
                if(FPlatformTime::Seconds()-LastLog>3)
                {
                    LastLog=FPlatformTime::Seconds();int32 Orbit=0,Strike=0,Return=0;
                    for(TActorIterator<ARogueReturningTalisman> It(WeakWorld.Get());It;++It)
                    {
                        if(It->GetFlightPhase()==ERogueTalismanPhase::Orbiting)++Orbit;
                        if(It->GetFlightPhase()==ERogueTalismanPhase::Striking)++Strike;
                        if(It->GetFlightPhase()==ERogueTalismanPhase::Returning)++Return;
                        UE_LOG(LogTemp,Display,TEXT("[TalismanDiagnostic] Tick=%d Phase=%d Hits=%d Pos=%s"),It->IsActorTickEnabled(),int32(It->GetFlightPhase()),It->GetSortieHitCount(),*It->GetActorLocation().ToCompactString());
                    }
                    UE_LOG(LogTemp,Display,TEXT("[TalismanDiagnostic] Orbit=%d Strike=%d Return=%d WorldTime=%.2f Paused=%d"),Orbit,Strike,Return,WeakWorld->GetTimeSeconds(),WeakWorld->IsPaused());
                }
                return true;
            }),.03f);
            UE_LOG(LogTemp,Display,TEXT("[TalismanPreview] READY Players=%d CountPerPlayer=2"),Players.Num());return false;
        }),.1f);
    }));

static FAutoConsoleCommand TalismanObserve(TEXT("Rogue.Test.ObserveTalisman"),TEXT("Verify all replicated talismans and their three phases. Optional player count."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        const int32 Players=Args.Num()?FMath::Clamp(FCString::Atoi(*Args[0]),1,4):2;
        const double Start=FPlatformTime::Seconds();auto Phases=MakeShared<TSet<ERogueTalismanPhase>>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Players,Start,Phases](float)
        {
            if(FPlatformTime::Seconds()-Start>90){UE_LOG(LogTemp,Error,TEXT("[TalismanNet] FAIL timeout phases=%d"),Phases->Num());return false;}
            auto* W=RogueTalismanPreview::World();if(!W || W->GetNetMode()!=NM_Client)return true;
            int32 Count=0;TSet<AActor*> Owners;bool Good=true;
            for(TActorIterator<ARogueReturningTalisman> It(W);It;++It)
            {
                ++Count;Owners.Add(It->GetOwner());Phases->Add(It->GetFlightPhase());
                Good &= !It->HasAuthority() && It->Mesh && It->Mesh->GetRelativeScale3D().Equals(FVector(.5,1,1),.001);
            }
            if(Count==Players*2 && Owners.Num()==Players && Phases->Num()==3)
            {
                UE_LOG(LogTemp,Display,TEXT("[TalismanNet] %s Replicated=%d Owners=%d Phases=%d NoLocalDuplicate=%d BlueprintScale=%d"),Good?TEXT("PASS"):TEXT("FAIL"),Count,Owners.Num(),Phases->Num(),Count==Players*2,Good);
                return false;
            }
            return true;
        }),.03f);
    }));
#endif
