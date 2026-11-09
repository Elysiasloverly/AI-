#if WITH_DEV_AUTOMATION_TESTS
#include "RogueProbeSupport.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Framework/RogueGameMode.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"

namespace RogueEnemyBulletNetProbe
{
struct FProbe
{
    double Start=FPlatformTime::Seconds(),PhaseTime=0,HitTime=0;
    int Phase=0;bool Quiet=false,EverBatched=false;
    TWeakObjectPtr<ARogueCharacter> Target;
    TWeakObjectPtr<ARogueEnemy> Source;

    bool Finish(bool bSuccess, const TCHAR* Detail)
    {
        return RogueProbeSupport::Finish(TEXT("EnemyBulletNet"), bSuccess, Detail);
    }
    bool Tick()
    {
        const double Now=FPlatformTime::Seconds();if(Now-Start>75)return Finish(false,TEXT("timeout"));
        UWorld* W = RogueProbeSupport::FindGameWorld();
        if(!W)return true;
        if(W->GetNetMode()==NM_Client)
        {
            auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<ARogueCharacter>(PC->GetPawn()):nullptr;
            for(TActorIterator<ARogueEnemyProjectile> It(W);It;++It)if(!It->IsAvailableInPool())EverBatched|=It->IsUsingBatchedMovement();
            if(P && P->GetRogueAttributeSet() && FMath::IsNearlyEqual(P->GetRogueAttributeSet()->GetHealth(),993.f))
            {
                if(!EverBatched)return Finish(false,TEXT("client did not observe a batched cosmetic bullet"));
                if(!HitTime)HitTime=Now;
                if(Now-HitTime>1)return Finish(true,TEXT("CLIENT batched cosmetic observed; replicated health=993, no duplicate local damage"));
            }
            return true;
        }
        auto* Mode=W->GetAuthGameMode<ARogueGameMode>();if(!Mode)return true;
        Mode->SetActorTickEnabled(false);W->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);
        if(!Quiet){Quiet=true;for(TActorIterator<ARogueEnemy> It(W);It;++It)It->DeactivateToPool();}
        if(Phase==0)
        {
            TArray<ARogueCharacter*> Players;
            for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
            {
                auto* P=It->Get()?Cast<ARogueCharacter>(It->Get()->GetPawn()):nullptr;
                auto* PS=P?P->GetPlayerState<ARoguePlayerState>():nullptr;
                if(P && PS && PS->IsBaseAttributesInitialized())Players.Add(P);
            }
            if(Players.Num()<2)return true;
            int I=0;for(auto* P:Players)
            {
                P->GetAbilitySystemComponent()->CancelAllAbilities();
                for(ARogueWeaponBase* Weapon:P->GetWeapons())if(Weapon){Weapon->OnOwnerDied();Weapon->GetConfig().MaxCount=0;Weapon->GetConfig().Count=0;}
                P->GetCharacterMovement()->DisableMovement();P->GetCharacterMovement()->SetComponentTickEnabled(false);
                auto* A=P->GetRogueAttributeSet();A->SetMaxHealth(1000);A->SetHealth(1000);A->SetArmor(0);A->SetMaxArmor(0);A->SetDamageReductionPercent(0);A->SetHealthRegenPerSecond(0);
                P->SetActorLocation(FVector(100000,I++*1000,1000),false,nullptr,ETeleportType::TeleportPhysics);P->ForceNetUpdate();
                if(auto* PC=Cast<APlayerController>(P->GetController()))PC->ClientSetLocation(P->GetActorLocation(),P->GetActorRotation());
                if(!P->GetController()->IsLocalController())Target=P;
            }
            if(!Target.IsValid())return Finish(false,TEXT("missing remote pawn"));
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Class=LoadClass<ARogueEnemy>(nullptr,*RogueEnemyAssetPaths::EnemyClassPath(TEXT("Rifleman")));
            const FVector Pos=Target->GetActorLocation()-FVector(250,0,0);
            Source=W->SpawnActor<ARogueEnemy>(Class,Pos,FRotator::ZeroRotator,Params);
            if(!Source.IsValid())return Finish(false,TEXT("source spawn"));
            Source->ActivatePooledEnemy(nullptr,Pos,FRotator::ZeroRotator);FRogueEnemyProfile Profile;Profile.Health=1000;Profile.Damage=1;Source->InitializeEnemy(Profile);
            Source->SetActorTickEnabled(false);Source->GetCharacterMovement()->SetComponentTickEnabled(false);Source->ForceNetUpdate();Phase=1;PhaseTime=Now;
            return true;
        }
        if(Phase==1 && Now-PhaseTime>1.5)
        {
            if(!Target.IsValid()||!Source.IsValid())return Finish(false,TEXT("actors lost"));
            auto* Class=LoadClass<ARogueEnemyProjectile>(nullptr,*RogueEnemyAssetPaths::ProjectileClassPath(TEXT("Rifleman")));
            const FVector Pos=Source->GetActorLocation();
            FActorSpawnParameters Params;Params.Owner=Source.Get();Params.Instigator=Source.Get();Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* B=W->SpawnActor<ARogueEnemyProjectile>(Class,Pos,FRotator::ZeroRotator,Params);
            B->ActivatePooledProjectile(Source.Get(),Source.Get(),Pos,FRotator::ZeroRotator,FVector::ForwardVector,300,7,false);
            if(!B->IsUsingBatchedMovement())return Finish(false,TEXT("server bullet did not batch"));
            Source->Multicast_SpawnCosmeticProjectile(Pos,FVector::ForwardVector,300,false,Class);Phase=2;PhaseTime=Now;
        }
        if(Phase==2)
        {
            if(!HitTime && Target.IsValid() && FMath::IsNearlyEqual(Target->GetRogueAttributeSet()->GetHealth(),993.f))HitTime=Now;
            if(HitTime && Now-HitTime>4)return Finish(true,TEXT("SERVER batched sweep inflicted exactly 7 authoritative damage"));
            if(!HitTime && Now-PhaseTime>5)return Finish(false,TEXT("authoritative hit missing"));
        }
        return true;
    }
};
}
static FAutoConsoleCommand EnemyBulletNetProbe(TEXT("Rogue.Test.EnemyBulletBatchNet"),TEXT("Explicit isolated two-process bullet authority/cosmetic check. Exits both processes, never saves assets."),
FConsoleCommandDelegate::CreateLambda([]
{
    auto P=MakeShared<RogueEnemyBulletNetProbe::FProbe>();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([P](float){return P->Tick();}),.02f);
}));
#endif
