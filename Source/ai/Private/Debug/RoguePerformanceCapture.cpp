// Explicit development-only fixed-load capture. Never changes maps, tables or local progression.
#include "Debug/RoguePerfStats.h"
CSV_DEFINE_CATEGORY(RoguePerf, true);
#if WITH_DEV_AUTOMATION_TESTS && CSV_PROFILER
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueReturningTalisman.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Weapons/RogueWeapon_Talisman.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Framework/RogueGameMode.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Player/RoguePlayerMotionComponent.h"
#include "World/RogueSceneDetail.h"
#include "UI/Widgets/RogueBattlePresentationWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "UnrealClient.h"

namespace RoguePerformance
{
// Diagnostic only: disable visual groups in this temporary capture world.
// 1=scenic instances, 2=player poses, 4=chapter UI, 8=new water material.
static TAutoConsoleVariable<int32> ArtAblation(TEXT("Rogue.Perf.ArtAblation"),0,TEXT("Fixed-load diagnostic mask; no map/config writes."));
struct FCapture
{
    FString Label, Folder;
    int32 EnemyCount=200, TalismanCount=4;
    double Start=FPlatformTime::Seconds(), Ready=0, CaptureStart=0, LastCounts=0;
    float Warmup=15, Duration=20;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogueCharacter> Player;
    TArray<TWeakObjectPtr<ARogueWeaponBase>> Weapons;
    TArray<float> FireTimers;
    TSharedFuture<FString> Completion;
    bool bStopping=false, bScreenshot=false;
    int32 Alive=0, Bullets=0, Charms=0, FlyingCharms=0, ManagedBullets=0;
    float ProcessMB=0;

    bool Tick(float Delta)
    {
        CSV_SCOPED_TIMING_STAT(RoguePerf, CaptureHarness);
        const double Now=FPlatformTime::Seconds();
        if (Now-Start>Warmup+Duration+120) { UE_LOG(LogTemp,Error,TEXT("[RoguePerf] TIMEOUT %s"),*Label);FPlatformMisc::RequestExit(false);return false; }
        if (!Ready)
        {
            UWorld* W=nullptr;
            for(const auto& C:GEngine->GetWorldContexts()) if(C.World() && C.World()->IsGameWorld() && C.World()->GetAuthGameMode<ARogueGameMode>()) { W=C.World();break; }
            if(!W)return true;
            W->GetAuthGameMode()->SetActorTickEnabled(false);
            if(auto* S=W->GetSubsystem<URogueSpawnSubsystem>())S->SetSpawningSuspended(true);
            ARogueCharacter* P=nullptr;
            for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
                if(It->Get())P=Cast<ARogueCharacter>(It->Get()->GetPawn());
            if(!P || !P->GetPlayerState<ARoguePlayerState>() || !P->GetPlayerState<ARoguePlayerState>()->IsBaseAttributesInitialized())return true;
            World=W;Player=P;
            const int32 ArtMask=ArtAblation.GetValueOnGameThread();
            if(ArtMask&1)for(TActorIterator<ARogueSceneDetail> It(W);It;++It)It->SetActorHiddenInGame(true);
            if(ArtMask&2)if(auto* Motion=P->FindComponentByClass<URoguePlayerMotionComponent>()){Motion->ResetPose();Motion->bEnableMotion=false;Motion->SetComponentTickEnabled(false);}
            if(ArtMask&4)
            {
                TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,URogueBattlePresentationWidget::StaticClass(),true);
                for(auto* Widget:Widgets)if(auto* Chapter=Cast<URogueBattlePresentationWidget>(Widget)){Chapter->bAutomatic=false;Chapter->SetVisibility(ESlateVisibility::Collapsed);}
            }
            if(ArtMask&8)
            {
                auto* Original=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GameAssets/ArtDirection/Materials/M_PaintedWater.M_PaintedWater"));
                if(Original)for(TActorIterator<AStaticMeshActor> It(W);It;++It)
                {auto* Mesh=It->GetStaticMeshComponent();for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)if(auto* Mat=Mesh->GetMaterial(Slot);Mat&&Mat->GetPathName().Contains(TEXT("/Completion/Materials/MI_Water_")))Mesh->SetMaterial(Slot,Original);}
            }
            UE_LOG(LogTemp,Display,TEXT("[RoguePerf] ArtAblation=%d"),ArtMask);
            for(TActorIterator<ARogueEnemy> It(W);It;++It)It->DeactivateToPool();
            P->GetAbilitySystemComponent()->CancelAllAbilities();
            P->SetActorEnableCollision(false);
            P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetComponentTickEnabled(false);
            // Disable normal loadout; benchmark actors use in-memory copies of the saved weapon rows.
            for(ARogueWeaponBase* Existing:P->GetWeapons())if(Existing){Existing->OnOwnerDied();Existing->GetConfig().Count=0;Existing->GetConfig().MaxCount=0;}
            P->ApplyWeaponBonusSnapshot(FRogueWeaponBonusValues());
            const FVector Origin=P->GetActorLocation();
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const TCHAR* Names[]={TEXT("Assault"),TEXT("Rifleman"),TEXT("Juggernaut")};
            UClass* Classes[3];for(int I=0;I<3;++I)Classes[I]=LoadClass<ARogueEnemy>(nullptr,*RogueEnemyAssetPaths::EnemyClassPath(Names[I]));
            for(int I=0;I<EnemyCount;++I)
            {
                // Golden-angle disk: same coordinates and enemy mix in every build/run.
                const float Angle=I*2.39996323f, Radius=380+1000*FMath::Sqrt(float(I)/FMath::Max(1,EnemyCount));
                FVector Pos=Origin+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
                auto* E=W->SpawnActor<ARogueEnemy>(Classes[I%3],Pos,FRotator::ZeroRotator,Params);
                if(!E)continue;
                E->ActivatePooledEnemy(nullptr,Pos,FRotator::ZeroRotator);
                FRogueEnemyProfile Profile;Profile.Health=100000000;Profile.Speed=0;Profile.Damage=1;Profile.ExperienceReward=0;
                const ERogueEnemyType Types[]={ERogueEnemyType::Assault,ERogueEnemyType::Rifleman,ERogueEnemyType::Juggernaut};Profile.EnemyType=Types[I%3];
                E->InitializeEnemy(Profile);
                // Static targets retain AI/attacks; remove movement/falling to keep population identical.
                E->GetCharacterMovement()->StopMovementImmediately();E->GetCharacterMovement()->SetComponentTickEnabled(false);
                W->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);
            }
            if(EnemyCount>0)
            {
                auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Weapons/DT_WeaponConfig.DT_WeaponConfig"));
                if(!Table){UE_LOG(LogTemp,Error,TEXT("[RoguePerf] Missing weapon table"));FPlatformMisc::RequestExit(false);return false;}
                for(const FName Name:Table->GetRowNames())
                {
                    const auto* Saved=Table->FindRow<FRogueWeaponTableRow>(Name,TEXT("Perf"));
                    if(!Saved || !Saved->WeaponClass)continue;
                    auto Row=*Saved;const bool IsCharm=Row.WeaponClass->IsChildOf(ARogueWeapon_Talisman::StaticClass());
                    Row.Count=IsCharm?TalismanCount:3;Row.MaxCount=Row.Count;Row.Damage=1;Row.MaxDamage=1;Row.Range=2200;Row.MaxRange=2200;Row.Cooldown=.65f;
                    auto* Weapon=W->SpawnActor<ARogueWeaponBase>(Row.WeaponClass,Origin,FRotator::ZeroRotator,Params);
                    if(Weapon){Weapon->SetOwner(P);Weapon->InitializeWeapon(P,Row);Weapons.Add(Weapon);FireTimers.Add(.1f*Weapons.Num());}
                }
            }
            FMath::RandInit(137);Ready=Now;
            UE_LOG(LogTemp,Display,TEXT("[RoguePerf] READY %s Map=%s Enemies=%d Talismans=%d Weapons=%d Warmup=%.1f Capture=%.1f Render=%d"),*Label,*W->GetMapName(),EnemyCount,TalismanCount,Weapons.Num(),Warmup,Duration,FApp::CanEverRender());
        }
        if(!World.IsValid() || !Player.IsValid())return false;
        Player->ApplyWeaponBonusSnapshot(FRogueWeaponBonusValues());
        {
            CSV_SCOPED_TIMING_STAT(RoguePerf, BenchmarkWeapons);
            for(int I=0;I<Weapons.Num();++I)if(auto* Weapon=Weapons[I].Get())
            {
                if(Weapon->NeedsCosmeticTick() || !Weapon->UsesTargetedFireLoop())Weapon->WeaponTick(Delta);
                else if((FireTimers[I]-=Delta)<=0)
                {
                    FireTimers[I]+=Weapon->GetAbilityCooldown();
                    TArray<ARogueEnemy*> Targets;Weapon->CollectFireTargets(Targets);if(!Targets.IsEmpty())Weapon->FireAtTargets(Targets);
                }
            }
        }
        if(Now-LastCounts>=1)
        {
            ManagedBullets=0;LastCounts=Now;Alive=World->GetSubsystem<URogueEnemyTrackerSubsystem>()->GetActiveEnemyCount();Bullets=0;Charms=0;FlyingCharms=0;
            for(TActorIterator<ARogueProjectile> It(World.Get());It;++It)if(!It->IsAvailableInPool())++Bullets;
            for(TActorIterator<ARogueEnemyProjectile> It(World.Get());It;++It)if(!It->IsAvailableInPool()){++Bullets;if(It->IsUsingBatchedMovement())++ManagedBullets;}
            for(TActorIterator<ARogueRocketProjectile> It(World.Get());It;++It)if(!It->IsAvailableInPool())++Bullets;
            for(TActorIterator<ARogueReturningTalisman> It(World.Get());It;++It){++Charms;if(It->GetFlightPhase()!=ERogueTalismanPhase::Orbiting)++FlyingCharms;}
            ProcessMB=float(FPlatformMemory::GetStats().UsedPhysical)/1048576.f;
        }
        CSV_CUSTOM_STAT(RoguePerf, AliveEnemies, Alive, ECsvCustomStatOp::Set);
        CSV_CUSTOM_STAT(RoguePerf, ActiveProjectiles, Bullets, ECsvCustomStatOp::Set);
        CSV_CUSTOM_STAT(RoguePerf, ManagedEnemyProjectiles, ManagedBullets, ECsvCustomStatOp::Set);
        CSV_CUSTOM_STAT(RoguePerf, Talismans, Charms, ECsvCustomStatOp::Set);
        CSV_CUSTOM_STAT(RoguePerf, FlyingTalismans, FlyingCharms, ECsvCustomStatOp::Set);
        CSV_CUSTOM_STAT(RoguePerf, ProcessPhysicalMB, ProcessMB, ECsvCustomStatOp::Set);
        if(!CaptureStart && Now-Ready>=Warmup)
        {
            CaptureStart=Now;FCsvProfiler::Get()->BeginCapture(-1,Folder,Label+TEXT(".csv"));
            UE_LOG(LogTemp,Display,TEXT("[RoguePerf] CAPTURE %s Enemies=%d Bullets=%d Charms=%d"),*Label,Alive,Bullets,Charms);
        }
        if(!CaptureStart && !bScreenshot && Now-Ready>=Warmup*.5)
        { bScreenshot=true;FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,Label+TEXT(".png")),true,false); }
        if(CaptureStart && !bStopping && Now-CaptureStart>=Duration)
        { bStopping=true;Completion=FCsvProfiler::Get()->EndCapture();World->GetSubsystem<URogueCombatPoolSubsystem>()->LogPoolDiagnostics(); }
        if(bStopping && Completion.IsReady())
        {
            UE_LOG(LogTemp,Display,TEXT("[RoguePerf] COMPLETE %s File=%s Enemies=%d Bullets=%d Charms=%d"),*Label,*Completion.Get(),Alive,Bullets,Charms);
            FPlatformMisc::RequestExit(false);return false;
        }
        return true;
    }
};
}
static FAutoConsoleCommand CaptureCommand(TEXT("Rogue.Perf.Capture"),TEXT("Fixed-load GPU/CPU capture: label enemies talismans warmupSeconds captureSeconds absoluteOutputFolder. Development only, exits when complete."),
FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
    if(Args.Num()<6){UE_LOG(LogTemp,Error,TEXT("Rogue.Perf.Capture requires 6 arguments"));return;}
    auto C=MakeShared<RoguePerformance::FCapture>();C->Label=Args[0];C->EnemyCount=FMath::Clamp(FCString::Atoi(*Args[1]),0,1000);C->TalismanCount=FMath::Clamp(FCString::Atoi(*Args[2]),0,64);
    C->Warmup=FMath::Max(2.f,FCString::Atof(*Args[3]));C->Duration=FMath::Max(3.f,FCString::Atof(*Args[4]));C->Folder=Args[5];
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([C](float Delta){return C->Tick(Delta);}));
}));
#endif
