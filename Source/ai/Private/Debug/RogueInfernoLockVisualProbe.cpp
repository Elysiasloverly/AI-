#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Visuals/RogueInfernoLockRing.h"
#include "Combat/Visuals/RogueInfernoLockSubsystem.h"
#include "Combat/Visuals/RogueLaserBeam.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"

namespace RogueLockVisualProbe
{
struct FProbe
{
    FString Folder;
    double Started=FPlatformTime::Seconds(),Ready=0,LastReport=0;
    int32 Shots=0;
    bool bSawTwo=false;
    TWeakObjectPtr<ARogueCharacter> Player;
    bool Tick()
    {
        const double Now=FPlatformTime::Seconds();
        if(Now-Started>50){UE_LOG(LogTemp,Error,TEXT("[LockVisual] FAIL timeout"));FPlatformMisc::RequestExit(false);return false;}
        UWorld* W=nullptr;
        for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsGameWorld()&&C.World()->GetAuthGameMode<ARogueGameMode>()){W=C.World();break;}
        if(!W)return true;
        if(!Ready)
        {
            auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<ARogueCharacter>(PC->GetPawn()):nullptr;
            auto* PS=P?P->GetPlayerState<ARoguePlayerState>():nullptr;
            if(!PS||!PS->IsBaseAttributesInitialized())return true;
            auto* Tower=P->FindWeapon<ARogueWeapon_HellTower>();if(!Tower)return true;
            W->GetAuthGameMode()->SetActorTickEnabled(false);
            W->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);
            for(TActorIterator<ARogueEnemy> It(W);It;++It)It->DeactivateToPool();
            for(ARogueWeaponBase* Weapon:P->GetWeapons())if(Weapon){Weapon->OnOwnerDied();Weapon->GetConfig().Count=0;Weapon->GetConfig().MaxCount=0;}
            Tower->GetConfig().Count=2;Tower->GetConfig().MaxCount=2;Tower->GetConfig().Range=2500;
            Tower->GetConfig().HellTowerMinDamage=0;Tower->GetConfig().BaseDamage=0;Tower->GetConfig().BaseDamageBonusScale=0;
            Tower->GetConfig().DamageRampPerTick=0;Tower->GetConfig().DamageRampBonusScale=0;
            const auto* Beam=Tower->GetConfig().BeamClass.GetDefaultObject();
            const auto* Ring=Beam&&Beam->InfernoLockRingClass?Beam->InfernoLockRingClass.GetDefaultObject():nullptr;
            UE_LOG(LogTemp,Display,TEXT("[LockVisual] SETUP beam=%s ring=%s material=%s player=%s"),*GetNameSafe(Beam),*GetNameSafe(Ring),*GetNameSafe(Ring?Ring->RingMaterial.Get():nullptr),*P->GetActorLocation().ToString());
            UClass* EnemyClass=LoadClass<ARogueEnemy>(nullptr,TEXT("/Game/BP/Enemy/Assault/BP_EnemyAssault.BP_EnemyAssault_C"));
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            for(int32 I=0;I<2;++I)
            {
                const FVector Pos=P->GetActorLocation()+FVector(440,I?230:-230,0);
                auto* E=W->SpawnActor<ARogueEnemy>(EnemyClass,Pos,FRotator::ZeroRotator,Params);
                E->ActivatePooledEnemy(nullptr,Pos,FRotator::ZeroRotator);
                FRogueEnemyProfile Profile;Profile.EnemyType=ERogueEnemyType::Assault;Profile.Health=1000000;Profile.Speed=60;Profile.Damage=0;Profile.ExperienceReward=0;
                E->InitializeEnemy(Profile);W->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);
            }
            Player=P;Ready=Now;
        }
        if(Now-LastReport>1)
        {
            LastReport=Now;auto* Rings=W->GetSubsystem<URogueInfernoLockSubsystem>();
            const int32 Count=Rings->GetActiveRingCount();bSawTwo|=Count==2;
            UE_LOG(LogTemp,Display,TEXT("[LockVisual] elapsed=%.2f rings=%d"),Now-Ready,Count);
            for(TActorIterator<ARogueInfernoLockRing> It(W);It;++It)if(!It->IsAvailable())
            {
                FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(InfernoLockProbe),true);
                for(TActorIterator<ARogueEnemy> E(W);E;++E)Q.AddIgnoredActor(*E);
                Q.AddIgnoredActor(Player.Get());Q.AddIgnoredActor(*It);
                W->LineTraceSingleByObjectType(Hit,It->GetActorLocation()+FVector(0,0,200),It->GetActorLocation()-FVector(0,0,500),FCollisionObjectQueryParams(ECC_WorldStatic),Q);
                UE_LOG(LogTemp,Display,TEXT("[LockVisual] ring=%s hidden=%d visible=%d position=%s floorZ=%.2f surface=%s scale=%s"),*It->GetName(),It->IsHidden(),It->RingMesh->IsVisible(),*It->GetActorLocation().ToString(),Hit.ImpactPoint.Z,*GetNameSafe(Hit.GetActor()),*It->GetActorScale3D().ToString());
            }
        }
        if(Shots<2&&Now-Ready>(Shots?5:2))
        {
            W->GetFirstPlayerController()->ConsoleCommand(FString::Printf(TEXT("Shot SHOWUI filename=%s -nosuffix"),*FPaths::Combine(Folder,FString::Printf(TEXT("normal-%d.png"),Shots))),false);++Shots;
        }
        if(Now-Ready>7)
        {
            if(bSawTwo){UE_LOG(LogTemp,Display,TEXT("[LockVisual] PASS normal GAS heartbeat produced both target circles"));}
            else{UE_LOG(LogTemp,Error,TEXT("[LockVisual] FAIL normal GAS heartbeat never produced both circles"));}
            FPlatformMisc::RequestExit(false);return false;
        }
        return true;
    }
};
}
static FAutoConsoleCommand LockVisualProbe(TEXT("Rogue.Test.InfernoLockVisual"),TEXT("Explicit normal GAS/grounded ring capture; output folder; exits when done."),FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
    if(Args.Num()!=1)return;auto P=MakeShared<RogueLockVisualProbe::FProbe>();P->Folder=Args[0];
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([P](float){return P->Tick();}),.02f);
}));
#endif
