#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "Combat/Visuals/RogueDefeatFeedbackSubsystem.h"
#include "Combat/Visuals/RogueEnemyDefeatEffect.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Framework/RogueGameMode.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "UI/Widgets/RogueKillFeedbackWidgetBase.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
namespace RogueDefeatNet
{
int32 Streak(UWorld* W)
{
    int32 N=0;for(TObjectIterator<URogueKillFeedbackWidgetBase> It;It;++It)
        if(It->GetWorld()==W && It->IsInViewport() && It->GetOwningPlayer() && It->GetOwningPlayer()->IsLocalController())N=FMath::Max(N,It->GetStreakCount());return N;
}
struct FProbe
{
    double Start=FPlatformTime::Seconds(),PhaseTime=0,SeenTwo=0;int Phase=0;bool Quiet=false;
    TWeakObjectPtr<ARogueCharacter> Host,Client;
    bool End(bool OK,const TCHAR* Why)
    {if(OK){UE_LOG(LogTemp,Display,TEXT("[DefeatNet] PASS %s"),Why);}else{UE_LOG(LogTemp,Error,TEXT("[DefeatNet] FAIL %s"),Why);}FPlatformMisc::RequestExitWithStatus(false,OK?0:1);return false;}
    bool Kill(UWorld* W,ARogueCharacter* P,bool Boss)
    {
        auto* C=LoadClass<ARogueEnemy>(nullptr,TEXT("/Game/BP/Enemy/Rifleman/BP_EnemyRifleman.BP_EnemyRifleman_C"));
        const FVector Pos=P->GetActorLocation()+FVector(300,0,0);FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E=W->SpawnActor<ARogueEnemy>(C,Pos,FRotator::ZeroRotator,Params);if(!E||!E->DefeatEffectClass)return false;
        E->ActivatePooledEnemy(nullptr,Pos,FRotator::ZeroRotator);FRogueEnemyProfile Profile;Profile.Health=100;Profile.Speed=0;Profile.Damage=0;Profile.ExperienceReward=0;Profile.bIsBoss=Boss;E->InitializeEnemy(Profile);
        E->SetActorTickEnabled(false);E->GetCharacterMovement()->DisableMovement();E->GetCharacterMovement()->SetComponentTickEnabled(false);
        return RogueAbilitySystem::ApplyDamageToTarget(P,E,10000,FGameplayTag());
    }
    bool Tick()
    {
        const double Now=FPlatformTime::Seconds();if(Now-Start>60)return End(false,TEXT("timeout"));
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsGameWorld()){W=C.World();break;}if(!W)return true;
        auto* Pool=W->GetSubsystem<URogueDefeatFeedbackSubsystem>();if(!Pool)return true;
        if(W->GetNetMode()==NM_Client)
        {
            if(Pool->GetPlayedCount()>=2)
            {
                if(Streak(W)!=1)return End(false,TEXT("client received another player's kill toast or duplicate credit"));
                if(!SeenTwo)SeenTwo=Now;if(Now-SeenTwo>.6)return End(true,TEXT("CLIENT saw both death VFX, only its own one-kill toast"));
            }
            return true;
        }
        auto* Mode=W->GetAuthGameMode<ARogueGameMode>();if(!Mode)return true;Mode->SetActorTickEnabled(false);W->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);
        if(!Quiet){Quiet=true;for(TActorIterator<ARogueEnemy> It(W);It;++It)It->DeactivateToPool();}
        if(Phase==0)
        {
            TArray<ARogueCharacter*> Players;
            for(auto It=W->GetPlayerControllerIterator();It;++It)
                if(auto* PC=It->Get())if(auto* P=Cast<ARogueCharacter>(PC->GetPawn()))if(auto* PS=P->GetPlayerState<ARoguePlayerState>();PS&&PS->IsBaseAttributesInitialized())Players.Add(P);
            if(Players.Num()<2)return true;
            for(auto* P:Players)
            {
                P->SetActorTickEnabled(false);P->GetCharacterMovement()->DisableMovement();P->GetCharacterMovement()->SetComponentTickEnabled(false);
                for(const auto& Weapon:P->GetWeapons())if(Weapon){Weapon->OnOwnerDied();Weapon->GetConfig().Count=0;Weapon->GetConfig().MaxCount=0;}
                if(P->GetController()->IsLocalController())Host=P;else Client=P;
            }
            if(!Host.IsValid()||!Client.IsValid())return End(false,TEXT("requires one listen host and one client"));Phase=1;PhaseTime=Now;
        }
        if(Phase==1&&Now-PhaseTime>1.5)
        {if(!Kill(W,Client.Get(),false))return End(false,TEXT("first real kill failed"));Phase=2;PhaseTime=Now;}
        if(Phase==2&&Now-PhaseTime>1.4)
        {
            if(Pool->GetPlayedCount()!=1||Streak(W)!=0)return End(false,TEXT("host should see remote kill VFX without remote toast"));
            if(!Kill(W,Host.Get(),true))return End(false,TEXT("second real boss kill failed"));Phase=3;PhaseTime=Now;
        }
        if(Phase==3&&Now-PhaseTime>2)
        {
            if(Pool->GetPlayedCount()!=2||Streak(W)!=1)return End(false,TEXT("host missing own boss confirmation or VFX duplicated"));
            return End(true,TEXT("HOST saw two VFX, one credited boss toast, no cross-player UI credit"));
        }
        return true;
    }
};
}
static FAutoConsoleCommand DefeatNetProbe(TEXT("Rogue.Test.DefeatNet"),TEXT("Explicit isolated listen-host/client defeat presentation check; no asset writes; exits processes."),FConsoleCommandDelegate::CreateLambda([]
{auto P=MakeShared<RogueDefeatNet::FProbe>();FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([P](float){return P->Tick();}),.02f);}));
#endif
