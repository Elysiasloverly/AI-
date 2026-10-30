#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Framework/RogueGameMode.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Projectiles/RogueEnemySkyfallProjectile.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace RogueSkyfallPreview
{
    UWorld* World()
    {
        if (GEngine) for (const auto& C:GEngine->GetWorldContexts())
            if (C.World()&&C.World()->IsGameWorld()) return C.World();
        return nullptr;
    }
}
// 仅显式开发测试命令启用：隔离战斗、等待人数、召唤一次真实BP攻击，不保存任何地图或进度。
static FAutoConsoleCommand SkyfallPreview(TEXT("Rogue.Test.Skyfall"),TEXT("One isolated real skyfall attack. Optional initialized player count."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        const int32 Count=Args.Num()?FMath::Clamp(FCString::Atoi(*Args[0]),1,4):1;
        const double Start=FPlatformTime::Seconds();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Count,Start](float)
        {
            auto* W=RogueSkyfallPreview::World();
            if (FPlatformTime::Seconds()-Start>90) { UE_LOG(LogTemp,Error,TEXT("[SkyfallPreview] FAIL timeout"));return false; }
            if (!W||!W->GetAuthGameMode<ARogueGameMode>()) return true;
            W->GetAuthGameMode()->SetActorTickEnabled(false);
            if (auto* Spawns=W->GetSubsystem<URogueSpawnSubsystem>()) Spawns->SetSpawningSuspended(true);
            for (TActorIterator<ARogueEnemy> It(W);It;++It) It->DeactivateToPool();
            TArray<ARogueCharacter*> Players;
            for (FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
            {
                auto* P=It->Get()?Cast<ARogueCharacter>(It->Get()->GetPawn()):nullptr;
                auto* S=P?P->GetPlayerState<ARoguePlayerState>():nullptr;
                if (P&&S&&S->IsBaseAttributesInitialized()) Players.Add(P);
            }
            if (Players.Num()<Count) return true;
            for (TActorIterator<ARogueWeaponBase> It(W);It;++It) It->Destroy();
            for (auto* P:Players)
            { P->SetActorTickEnabled(false);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetComponentTickEnabled(false); }
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* C=LoadClass<ARogueEnemy>(nullptr,TEXT("/Game/BP/Enemy/Marksman/BP_EnemyMarksman.BP_EnemyMarksman_C"));
            auto* E=C?W->SpawnActor<ARogueEnemy>(C,Players[0]->GetActorLocation()+FVector(700,0,0),FRotator::ZeroRotator,Params):nullptr;
            if (!E) { UE_LOG(LogTemp,Error,TEXT("[SkyfallPreview] FAIL BP"));return false; }
            E->ActivatePooledEnemy(nullptr,E->GetActorLocation(),FRotator::ZeroRotator);
            FRogueEnemyProfile Profile;Profile.EnemyType=ERogueEnemyType::Marksman;Profile.Health=100000;Profile.Damage=0;Profile.Speed=0;
            E->InitializeEnemy(Profile);E->SetRangedAttackCooldown(1000);E->Tick(.01f);E->SetActorTickEnabled(false);
            E->GetCharacterMovement()->StopMovementImmediately();E->GetCharacterMovement()->SetComponentTickEnabled(false);
            E->ExecuteRangedAttack();
            UE_LOG(LogTemp,Display,TEXT("[SkyfallPreview] FIRED Players=%d"),Players.Num());
            return false;
        }),.1f);
    }));

static FAutoConsoleCommand SkyfallObserve(TEXT("Rogue.Test.ObserveSkyfall"),TEXT("Observe real replicated warning, descent and burst, without applying client damage."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        const double Start=FPlatformTime::Seconds();
        auto SawWarning=MakeShared<bool>(false),SawFalling=MakeShared<bool>(false);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Start,SawWarning,SawFalling](float)
        {
            auto* W=RogueSkyfallPreview::World();
            if (FPlatformTime::Seconds()-Start>90) { UE_LOG(LogTemp,Error,TEXT("[SkyfallNet] FAIL timeout"));return false; }
            if (!W||W->GetNetMode()!=NM_Client) return true;
            for (TActorIterator<ARogueEnemySkyfallProjectile> It(W);It;++It)
            {
                auto* S=*It;
                if (S->IsWarningVisible())
                {
                    *SawWarning=true;
                    if (S->GetAttackAge()>.85f) *SawFalling=true;
                }
                if (S->HasImpacted())
                {
                    int32 N=0;for (TActorIterator<ARogueEnemyProjectile> B(W);B;++B) if (!B->IsAvailableInPool()) ++N;
                    const bool Good=*SawWarning&&*SawFalling&&!S->IsWarningVisible()&&N==24;
                    UE_LOG(LogTemp,Display,TEXT("[SkyfallNet] %s Warning=%d FallingWarning=%d Impact=%d Burst=%d"),
                        Good?TEXT("PASS"):TEXT("FAIL"),*SawWarning,*SawFalling,S->HasImpacted(),N);
                    return false;
                }
            }
            return true;
        }),.03f);
    }));
#endif
