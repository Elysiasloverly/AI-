// Explicit development capture of normal waves, movement, damage, deaths, XP and GAS weapons.
#include "Debug/RoguePerfStats.h"
#if WITH_DEV_AUTOMATION_TESTS && CSV_PROFILER
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "Combat/Projectiles/RogueReturningTalisman.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "World/RogueExperiencePickup.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "PipelineStateCache.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "ProfilingDebugging/MiscTrace.h"

namespace RogueRealBattle
{
int32 ChooseCard(const ARoguePlayerState* State,const ARogueCharacter* Player,bool bDefensive=false)
{
    int32 Best=0;float BestScore=-1;
    const auto& Cards=State->GetPendingUpgrades();
    for(int32 I=0;I<Cards.Num();++I)
    {
        const auto Type=Cards[I].Type;float Score=25;
        if(Type==ERogueUpgradeType::MaxHealth)Score=Player->GetMaxHealth()<300?95:45;
        else if(Type==ERogueUpgradeType::AttackPower)Score=80;
        else if(Type==ERogueUpgradeType::AttackSpeed)Score=65;
        else if(Type==ERogueUpgradeType::ArmorCapacity)Score=85;
        else if(Type==ERogueUpgradeType::PickupRadius)Score=45;
        else if(Type==ERogueUpgradeType::Recovery)Score=60;
        for(ARogueWeaponBase* W:Player->GetWeapons())if(W && W->GetCountUpgradeType()==Type)
            Score=W->GetEffectiveCount()<1?110:(W->GetEffectiveCount()<3?90:35);
        Score-=FMath::Min(State->GetUpgradePickCount(Type),5)*4;
        if(bDefensive)
        {
            if(Type==ERogueUpgradeType::MaxHealth)Score=Player->GetMaxHealth()<750?600:100;
            else if(Type==ERogueUpgradeType::ArmorCapacity)Score=550;
            else if(Type==ERogueUpgradeType::Recovery)Score=450;
        }
        if(Score>BestScore){BestScore=Score;Best=I;}
    }
    return Best;
}
struct FEnemyPose { FVector Location;FGuid Cycle; };
struct FThreat { FVector Location,Velocity;float Radius; };
struct FCapture : TSharedFromThis<FCapture>
{
    FString Label,Folder,Role,StopReason=TEXT("duration");
    int32 Wave=20,TargetLevel=21,ExpectedPlayers=1;
    float Warmup=8,Duration=35;
    double Start=FPlatformTime::Seconds(),Ready=0,CaptureStart=0,LastSample=0,StopTime=0;
    double ChoiceSeen=0,LastDash=0,PrepareStart=0,QuietSince=0,RenderQuietSince=0,LastSteer=0;
    bool bConfigured=false,bAdaptive=false,bDefensive=false;
    int32 WrittenReadiness=-1;
    int32 ShaderJobs=0,AssetJobs=0,PipelineJobs=0,AsyncLoads=0;
    float PreparationSeconds=0,LastObservedHealth=0,LastObservedArmor=0;
    FString Barrier;
    FVector SteerDirection=FVector::ForwardVector;
    TArray<FThreat> BulletThreats;
    FGuid LastChoice;
    bool bQuiet=false,bStopping=false,bScreenshot=false,bWasAlive=false;
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ARogueCharacter> Local;
    FVector Centre,LastPlayerPosition;
    float MovePhase=0,PlayerTravel=0,HealthLost=0,ArmorLost=0,AliveSeconds=0;
    int32 SubmittedCards=0,Deaths=0,LastKills=0,Alive=0,Moving=0,MoveTicks=0,Bullets=0,Managed=0,Pickups=0,Flyers=0,Healers=0;
    float NetInKB=0,NetOutKB=0;
    TMap<TWeakObjectPtr<ARogueEnemy>,FEnemyPose> EnemyPoses;
    TSharedFuture<FString> Completion;
    FString Setup;

    void Stop()
    {
        if(bStopping)return;bStopping=true;StopTime=FPlatformTime::Seconds();
        if(CaptureStart)Completion=FCsvProfiler::Get()->EndCapture();
        else {const FString Failure=FString::Printf(TEXT("Reason=%s Travel=%.1f HealthLost=%.1f ArmorLost=%.1f"),*StopReason,PlayerTravel,HealthLost,ArmorLost);FFileHelper::SaveStringToFile(Failure,*(Folder/(Label+TEXT(".failed-before-capture.txt"))));FPlatformMisc::RequestExitWithStatus(false,2);}
    }
    bool Prepare(UWorld* W,ARogueCharacter* P)
    {
        auto* Mode=W->GetAuthGameMode<ARogueGameMode>();
        if(Mode)
        {
            Mode->SetActorTickEnabled(false);W->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);
            if(!bQuiet){bQuiet=true;for(TActorIterator<ARogueEnemy> It(W);It;++It)It->DeactivateToPool();}
            TArray<ARogueCharacter*> Players;
            for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
            {
                auto* C=It->Get()?Cast<ARogueCharacter>(It->Get()->GetPawn()):nullptr;
                auto* S=C?C->GetPlayerState<ARoguePlayerState>():nullptr;
                if(C&&S&&S->IsBaseAttributesInitialized())Players.Add(C);
            }
            if(Players.Num()<ExpectedPlayers)return false;
            FMath::RandInit(71403);
            auto* Property=FindFProperty<FStructProperty>(ARogueGameMode::StaticClass(),TEXT("RunState"));
            if(!Property){StopReason=TEXT("missing_run_state");Stop();return false;}
            auto* Run=Property->ContainerPtrToValuePtr<FRogueRunState>(Mode);
            *Run=FRogueRunState();Run->CurrentWave=Wave;Run->BattleStageIndex=W->GetMapName().Contains(TEXT("Night"))?1:0;
            for(auto* C:Players)
            {
                auto* S=C->GetPlayerState<ARoguePlayerState>();int32 Picks=0;
                // Build through normal XP/level/offer/RPC paths; no direct health/damage/cooldown overrides.
                for(int32 Guard=0;C->GetPlayerLevel()<TargetLevel&&Guard<TargetLevel*4;++Guard)
                {
                    C->AddExperience(FMath::Max(1,C->GetExperienceToNextLevel()));
                    for(int32 ChoiceGuard=0;S->IsAwaitingUpgradeChoice()&&ChoiceGuard<100;++ChoiceGuard)
                    {
                        const int32 Index=ChooseCard(S,C,bDefensive && C->GetPlayerLevel()>=41);const FGuid Id=S->GetPendingUpgrades()[Index].SelectionId;
                        S->Server_RequestUpgradeSelection(Index,Id);++Picks;
                    }
                }
                Setup+=FString::Printf(TEXT("Player=%s Level=%d PresetPicks=%d ProfileNodes=%d HP=%.2f Armor=%.2f\n"),*S->GetPlayerName(),C->GetPlayerLevel(),Picks,S->GetProgressionSnapshot().NodeRanks.Num(),C->GetMaxHealth(),C->GetCurrentArmor());
                for(ARogueWeaponBase* Weapon:C->GetWeapons())if(Weapon)
                    Setup+=FString::Printf(TEXT("Weapon=%s Count=%d Damage=%.3f Cooldown=%.3f Range=%.1f\n"),*Weapon->GetClass()->GetName(),Weapon->GetEffectiveCount(),Weapon->GetEffectiveDamage(),Weapon->GetAbilityCooldown(),Weapon->GetEffectiveRange());
            }
            Mode->PushRunStateToGameState();
        }
        else
        {
            const auto* State=W->GetGameState<ARogueGameState>();
            if(!State||State->GetCurrentWave()<Wave||State->PlayerArray.Num()<ExpectedPlayers||P->GetPlayerLevel()<TargetLevel)return false;
        }
        World=W;Local=P;Role=W->GetNetMode()==NM_Client?TEXT("client"):(W->GetNetMode()==NM_ListenServer?TEXT("host"):TEXT("solo"));
        MovePhase=W->GetNetMode()==NM_Client?.9f:0.f;
        Centre=P->GetActorLocation()-FVector(FMath::Cos(MovePhase)*(bAdaptive?1800:650),FMath::Sin(MovePhase)*(bAdaptive?1300:450),0);
        LastPlayerPosition=P->GetActorLocation();bWasAlive=P->IsAlive();PrepareStart=FPlatformTime::Seconds();
        const TWeakPtr<FCapture> Weak=AsShared();
        if(auto* ASC=P->GetAbilitySystemComponent(); ASC && W->GetNetMode()!=NM_Client)
        {
            ASC->GetGameplayAttributeValueChangeDelegate(URogueAttributeSet::GetHealthAttribute()).AddLambda([Weak](const FOnAttributeChangeData& D){if(auto C=Weak.Pin())C->HealthLost+=FMath::Max(0.f,D.OldValue-D.NewValue);});
            ASC->GetGameplayAttributeValueChangeDelegate(URogueAttributeSet::GetArmorAttribute()).AddLambda([Weak](const FOnAttributeChangeData& D){if(auto C=Weak.Pin())C->ArmorLost+=FMath::Max(0.f,D.OldValue-D.NewValue);});
        }
        Setup+=FString::Printf(TEXT("DefensivePicksAfterLevel41=%d\n"),bDefensive);
        FFileHelper::SaveStringToFile(Setup,*(Folder/(Label+TEXT(".setup.txt"))));
        bConfigured=true;
        TRACE_BOOKMARK(TEXT("RealBattle %s SetupComplete"),*Label);
        return true;
    }
    bool ObserveResources()
    {
        ShaderJobs=GShaderCompilingManager?GShaderCompilingManager->GetNumRemainingJobs():0;
        AssetJobs=FAssetCompilingManager::Get().GetNumRemainingAssets();
        PipelineJobs=PipelineStateCache::GetNumActivePipelinePrecompileTasks();
        AsyncLoads=IsAsyncLoading()?1:0;
        return ShaderJobs==0 && AssetJobs==0 && PipelineJobs==0 && AsyncLoads==0;
    }
    bool AwaitPreparation(double Now,float Delta)
    {
        const bool bIdle=ObserveResources();
        if(!bIdle)QuietSince=0;
        else if(!QuietSince)QuietSince=Now;
        const bool bLocalReady=QuietSince && Now-QuietSince>=2 && Now-PrepareStart>=5;
        if(!Barrier.IsEmpty())
        {
            // Local-process QA barrier; it does not add gameplay RPCs or change online readiness.
            if(WrittenReadiness!=static_cast<int32>(bLocalReady))
            {
                FFileHelper::SaveStringToFile(bLocalReady?TEXT("ready"):TEXT("waiting"),*(Folder/(Barrier+TEXT("_")+Role+TEXT(".ready"))));
                WrittenReadiness=bLocalReady;
            }
        }
        if(Now-LastSample>2)
        {
            LastSample=Now;
            UE_LOG(LogTemp,Display,TEXT("[RealBattlePrepare] %s Shaders=%d Assets=%d Pipelines=%d Async=%d Quiet=%.2f"),*Label,ShaderJobs,AssetJobs,PipelineJobs,AsyncLoads,QuietSince?Now-QuietSince:0);
        }
        if(!bLocalReady)return false;
        if(!Barrier.IsEmpty())
        {
            FString Host,Client;
            FFileHelper::LoadFileToString(Host,*(Folder/(Barrier+TEXT("_host.ready"))));
            FFileHelper::LoadFileToString(Client,*(Folder/(Barrier+TEXT("_client.ready"))));
            if(Host!=TEXT("ready")||Client!=TEXT("ready"))return false;
            if(Role==TEXT("host"))FFileHelper::SaveStringToFile(TEXT("start"),*(Folder/(Barrier+TEXT(".start"))));
            else if(!IFileManager::Get().FileExists(*(Folder/(Barrier+TEXT(".start")))))return false;
        }
        Ready=Now;LastDash=Now;PreparationSeconds=Now-PrepareStart;
        LastPlayerPosition=Local->GetActorLocation();bWasAlive=Local->IsAlive();
        LastObservedHealth=Local->GetCurrentHealth();LastObservedArmor=Local->GetCurrentArmor();
        HealthLost=ArmorLost=AliveSeconds=0;
        if(auto* Mode=World->GetAuthGameMode<ARogueGameMode>())
        {
            Mode->SetActorTickEnabled(true);World->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(false);
        }
        const FString Preparation=FString::Printf(TEXT("PreparationSeconds=%.3f\nShaders=%d\nAssets=%d\nPipelines=%d\nAsync=%d\nStableSeconds=%.3f\nPSOPrecaching=%d\nAdaptiveSteering=%d\n"),PreparationSeconds,ShaderJobs,AssetJobs,PipelineJobs,AsyncLoads,Now-QuietSince,PipelineStateCache::IsPSOPrecachingEnabled(),bAdaptive);
        FFileHelper::SaveStringToFile(Preparation,*(Folder/(Label+TEXT(".prepared.txt"))));
        UE_LOG(LogTemp,Display,TEXT("[RealBattle] READY %s Role=%s Wave=%d Level=%d Collision=%d MovementTick=%d Players=%d Prepared=%.2fs"),*Label,*Role,Wave,Local->GetPlayerLevel(),Local->GetActorEnableCollision(),Local->GetCharacterMovement()->IsComponentTickEnabled(),ExpectedPlayers,PreparationSeconds);
        TRACE_BOOKMARK(TEXT("RealBattle %s BattleStart"),*Label);
        return true;
    }
    void Drive(float Delta,double Now)
    {
        auto* P=Local.Get();auto* S=P->GetPlayerState<ARoguePlayerState>();
        const bool Living=P->IsAlive();if(bWasAlive&&!Living)++Deaths;
        if(Living&&bWasAlive)PlayerTravel+=FVector::Dist2D(P->GetActorLocation(),LastPlayerPosition);
        LastPlayerPosition=P->GetActorLocation();bWasAlive=Living;if(Living)AliveSeconds+=Delta;
        if(Role==TEXT("client"))
        {
            // Client values are observed replicated mirrors, not authoritative damage totals.
            HealthLost+=FMath::Max(0.f,LastObservedHealth-P->GetCurrentHealth());
            ArmorLost+=FMath::Max(0.f,LastObservedArmor-P->GetCurrentArmor());
            LastObservedHealth=P->GetCurrentHealth();LastObservedArmor=P->GetCurrentArmor();
        }
        if(!Living)return;
        if(S&&S->IsAwaitingUpgradeChoice()&&!S->GetPendingUpgrades().IsEmpty())
        {
            const FGuid Id=S->GetPendingUpgrades()[0].SelectionId;
            if(LastChoice!=Id){LastChoice=Id;ChoiceSeen=Now;}
            if(Now-ChoiceSeen>.65&&!S->IsUpgradeSubmissionPending(Id)){S->SubmitUpgradeSelection(ChooseCard(S,P),Id);++SubmittedCards;}
            return;
        }
        const float Angle=(Now-Ready+.25)*(bAdaptive?.2f:.65f)+MovePhase;
        const FVector Goal=Centre+FVector(FMath::Cos(Angle)*(bAdaptive?1800:650),FMath::Sin(Angle)*(bAdaptive?1300:450),0);
        FVector Direction=(Goal-P->GetActorLocation()).GetSafeNormal2D();
        if(bAdaptive)
        {
            if(Now-LastSteer>.15)
            {
                LastSteer=Now;float Best=-FLT_MAX;
                const FVector Origin=P->GetActorLocation();
                const float Speed=P->GetCharacterMovement()->GetMaxSpeed();
                const auto* Cap=P->GetCapsuleComponent();
                FCollisionQueryParams Query(SCENE_QUERY_STAT(RealBattleAdaptiveSteering),false,P);
                for(int32 I=0;I<12;++I)
                {
                    const FVector Candidate=FVector(FMath::Cos(I*PI/6),FMath::Sin(I*PI/6),0);
                    const FVector Future=Origin+Candidate*Speed*.7f;
                    float Score=FVector::DotProduct(Candidate,Direction)*.6f+FVector::DotProduct(Candidate,SteerDirection)*.2f;
                    for(const auto& Entry:EnemyPoses)
                    {
                        if(!Entry.Key.IsValid())continue;
                        const float Dist=FVector::Dist2D(Future,Entry.Value.Location);
                        Score-=FMath::Square(FMath::Max(0.f,1-Dist/650.f))*4;
                    }
                    for(const FThreat& Threat:BulletThreats)
                    {
                        const FVector Offset=Threat.Location+Threat.Velocity*(Now-LastSample)-Origin;
                        const FVector RelativeVelocity=Threat.Velocity-Candidate*Speed;
                        const float Time=FMath::Clamp(-FVector::DotProduct(Offset,RelativeVelocity)/FMath::Max(1.f,RelativeVelocity.SizeSquared()),0.f,.7f);
                        const float Dist=(Offset+RelativeVelocity*Time).Size();
                        Score-=FMath::Square(FMath::Max(0.f,1-Dist/(Threat.Radius+Cap->GetScaledCapsuleRadius()+100)))*8;
                    }
                    FHitResult Hit;
                    if(World->SweepSingleByObjectType(Hit,Origin,Future,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius()*.9f,FMath::Max(Cap->GetScaledCapsuleRadius(),Cap->GetScaledCapsuleHalfHeight()-14)),Query)&&Hit.ImpactNormal.Z<.6f)Score-=100;
                    if(Score>Best){Best=Score;SteerDirection=Candidate;}
                }
            }
            Direction=SteerDirection;
        }
        FHitResult Wall;FCollisionQueryParams Params(SCENE_QUERY_STAT(RealBattleSteering),false,P);
        const auto* Capsule=P->GetCapsuleComponent();
        if(World->SweepSingleByObjectType(Wall,P->GetActorLocation(),P->GetActorLocation()+Direction*100,FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius()*.9f,FMath::Max(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()-14)),Params)&&Wall.ImpactNormal.Z<.6f)
        {
            Direction=FVector::VectorPlaneProject(Direction,Wall.ImpactNormal).GetSafeNormal2D();
            if(Direction.IsNearlyZero())Direction=FVector::CrossProduct(Wall.ImpactNormal,FVector::UpVector).GetSafeNormal2D();
        }
        P->AddMovementInput(Direction,FMath::Clamp(FVector::Dist2D(Goal,P->GetActorLocation())/150.f,.2f,1.f),true);
        if(Now-LastDash>4.5)
        {
            LastDash=Now;
            if(auto* ASC=P->GetAbilitySystemComponent()){FGameplayTagContainer Tags;Tags.AddTag(RogueGameplayTags::Ability_Dash);ASC->TryActivateAbilitiesByTag(Tags);}
        }
    }
    void Sample()
    {
        Alive=Moving=MoveTicks=Bullets=Managed=Pickups=Flyers=Healers=0;
        TMap<TWeakObjectPtr<ARogueEnemy>,FEnemyPose> NewPoses;
        for(TActorIterator<ARogueEnemy> It(World.Get());It;++It)
        {
            auto* E=*It;if(E->IsAvailableInPool()||E->IsDead())continue;++Alive;
            const FEnemyPose Pose{E->GetActorLocation(),E->GetSpawnCycleId()};
            if(const auto* Previous=EnemyPoses.Find(E);Previous&&Previous->Cycle==Pose.Cycle&&FVector::DistSquared(Previous->Location,Pose.Location)>25)++Moving;
            NewPoses.Add(E,Pose);if(E->GetCharacterMovement()->IsComponentTickEnabled())++MoveTicks;
            if(RogueEnemyArchetypes::IsFlyingType(E->GetEnemyType()))++Flyers;
            if(RogueEnemyArchetypes::IsHealerType(E->GetEnemyType()))++Healers;
        }
        EnemyPoses=MoveTemp(NewPoses);BulletThreats.Reset();
        for(TActorIterator<ARogueEnemyProjectile> It(World.Get());It;++It)if(!It->IsAvailableInPool())
        {
            ++Bullets;if(It->IsUsingBatchedMovement())++Managed;
            if(bAdaptive && FVector::DistSquared(It->GetActorLocation(),Local->GetActorLocation())<FMath::Square(1800.f))
            {
                const auto* Sphere=Cast<USphereComponent>(It->GetRootComponent());
                BulletThreats.Add({It->GetActorLocation(),It->GetVelocity(),Sphere?Sphere->GetScaledSphereRadius():30.f});
            }
        }
        for(TActorIterator<ARogueProjectile> It(World.Get());It;++It)if(!It->IsAvailableInPool())++Bullets;
        for(TActorIterator<ARogueRocketProjectile> It(World.Get());It;++It)if(!It->IsAvailableInPool())++Bullets;
        for(TActorIterator<ARogueExperiencePickup> It(World.Get());It;++It)if(!It->IsAvailableInPool())++Pickups;
        NetInKB=NetOutKB=0;
        if(auto* Driver=World->GetNetDriver())
        {
            auto Add=[this](const UNetConnection* C){if(C){NetInKB+=C->InBytesPerSecond/1024.f;NetOutKB+=C->OutBytesPerSecond/1024.f;}};
            Add(Driver->ServerConnection);for(UNetConnection* C:Driver->ClientConnections)Add(C);
        }
    }
    bool Tick(float Delta)
    {
        CSV_SCOPED_TIMING_STAT(RoguePerf, RealBattleHarness);
        const double Now=FPlatformTime::Seconds();
        if(Now-Start>Duration+Warmup+120){StopReason=TEXT("timeout");Stop();return false;}
        if(!Ready)
        {
            if(bConfigured)
            {
                if(!World.IsValid()||!Local.IsValid()){StopReason=TEXT("world_lost_during_preparation");Stop();return false;}
                if(!AwaitPreparation(Now,Delta))return true;
            }
            else
            {
            UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsGameWorld()){W=C.World();break;}
            if(!W)return true;auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<ARogueCharacter>(PC->GetPawn()):nullptr;
            auto* S=P?P->GetPlayerState<ARoguePlayerState>():nullptr;
            if(!P||!S||(W->GetNetMode()!=NM_Client&&!S->IsBaseAttributesInitialized())||(W->GetNetMode()==NM_Client&&(!P->GetRogueAttributeSet()||P->GetRogueAttributeSet()->GetMaxHealth()<=0)))
            {if(auto* M=W->GetAuthGameMode<ARogueGameMode>()){M->SetActorTickEnabled(false);W->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);}return true;}
            if(!Prepare(W,P))return !bStopping;
            return true;
            }
        }
        if(!World.IsValid()||!Local.IsValid()){StopReason=TEXT("world_lost");Stop();return false;}
        if(!bStopping)
        {
            Drive(Delta,Now);if(Now-LastSample>.5){LastSample=Now;Sample();UE_LOG(LogTemp,Display,TEXT("[RealBattleMotion] %s Pos=%s Speed=%.1f Travel=%.0f HP=%.1f Enemies=%d Moving=%d"),*Label,*Local->GetActorLocation().ToCompactString(),Local->GetVelocity().Size2D(),PlayerTravel,Local->GetCurrentHealth(),Alive,Moving);}
            auto* P=Local.Get();auto* State=World->GetGameState<ARogueGameState>();
            const bool bResourcesIdle=ObserveResources();
            if(!bResourcesIdle)RenderQuietSince=0;
            else if(!RenderQuietSince)RenderQuietSince=Now;
            if(!CaptureStart&&Now-Ready>=Warmup&&RenderQuietSince&&Now-RenderQuietSince>=2)
            {
                CaptureStart=Now;LastKills=State?State->GetEnemiesDefeated():0;FCsvProfiler::Get()->BeginCapture(-1,Folder,Label+TEXT(".csv"));
                UE_LOG(LogTemp,Display,TEXT("[RealBattle] CAPTURE %s Alive=%d Moving=%d HP=%.1f"),*Label,Alive,Moving,P->GetCurrentHealth());
                TRACE_BOOKMARK(TEXT("RealBattle %s CaptureStart"),*Label);
            }
            // Timed runs never take screenshots: readback and PNG compression can stall several frames.
            CSV_CUSTOM_STAT(RoguePerf, RealFrameId,static_cast<int32>(GFrameCounter),ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealElapsed,Now-Ready,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealPendingShaders,ShaderJobs,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealPendingAssets,AssetJobs,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealPendingPipelines,PipelineJobs,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealAsyncLoading,AsyncLoads,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealEnemies,Alive,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealMovingEnemies,Moving,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealEnemyMovementTicks,MoveTicks,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealFlyers,Flyers,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealHealers,Healers,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealProjectiles,Bullets,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealManagedBullets,Managed,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealPickups,Pickups,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealWave,State?State->GetCurrentWave():0,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealKills,State?State->GetEnemiesDefeated():0,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealPlayerSpeed,P->GetVelocity().Size2D(),ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealPlayerTravel,PlayerTravel,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealHealth,P->GetCurrentHealth(),ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealHealthLost,HealthLost,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealArmorLost,ArmorLost,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealPlayerAlive,P->IsAlive()?1:0,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealLevel,P->GetPlayerLevel(),ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealExperience,P->GetCurrentExperience(),ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealChoices,SubmittedCards,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealNetInKB,NetInKB,ECsvCustomStatOp::Set);
            CSV_CUSTOM_STAT(RoguePerf, RealNetOutKB,NetOutKB,ECsvCustomStatOp::Set);
            if(State&&State->IsGameOver()){StopReason=TEXT("game_over");Stop();}
            else if(CaptureStart&&Now-CaptureStart>=Duration)Stop();
        }
        if(bStopping&&CaptureStart&&Completion.IsReady())
        {
            const FString Summary=FString::Printf(TEXT("Role=%s\nStartWave=%d\nTargetLevel=%d\nCapturedSeconds=%.3f\nReason=%s\nTravelCm=%.1f\nHealthLost=%.2f\nArmorLost=%.2f\nDeaths=%d\nChoices=%d\nAliveSeconds=%.3f\nPreparationSeconds=%.3f\nAdaptiveSteering=%d\n"),*Role,Wave,TargetLevel,StopTime-CaptureStart,*StopReason,PlayerTravel,HealthLost,ArmorLost,Deaths,SubmittedCards,AliveSeconds,PreparationSeconds,bAdaptive);
            FFileHelper::SaveStringToFile(Summary,*(Folder/(Label+TEXT(".result.txt"))));
            if(Role!=TEXT("host")||Now-StopTime>2){TRACE_BOOKMARK(TEXT("RealBattle %s Complete"),*Label);UE_LOG(LogTemp,Display,TEXT("[RealBattle] COMPLETE %s %s %.2fs Kills=%d Travel=%.0f Damage=%.1f"),*Label,*StopReason,StopTime-CaptureStart,World->GetGameState<ARogueGameState>()->GetEnemiesDefeated(),PlayerTravel,HealthLost);FPlatformMisc::RequestExit(false);return false;}
        }
        return true;
    }
};
}
static FAutoConsoleCommand RealBattleCapture(TEXT("Rogue.Perf.RealBattle"),TEXT("Normal combat capture: label startWave targetLevel players warmupSeconds captureSeconds outputFolder. Sets a declared legal starting build, then uses normal gameplay and local movement inputs."),
FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& A)
{
    if(A.Num()<7){UE_LOG(LogTemp,Error,TEXT("Rogue.Perf.RealBattle needs 7 arguments"));return;}
    auto C=MakeShared<RogueRealBattle::FCapture>();C->Label=A[0];C->Wave=FMath::Clamp(FCString::Atoi(*A[1]),1,99);C->TargetLevel=FMath::Clamp(FCString::Atoi(*A[2]),1,80);C->ExpectedPlayers=FMath::Clamp(FCString::Atoi(*A[3]),1,4);C->Warmup=FMath::Max(2.f,FCString::Atof(*A[4]));C->Duration=FMath::Max(5.f,FCString::Atof(*A[5]));C->Folder=A[6];
    C->bAdaptive=FParse::Param(FCommandLine::Get(),TEXT("RoguePerfAdaptiveSteering"));
    C->bDefensive=FParse::Param(FCommandLine::Get(),TEXT("RoguePerfDefensiveBuild"));
    FParse::Value(FCommandLine::Get(),TEXT("RoguePerfBarrier="),C->Barrier);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([C](float D){return C->Tick(D);}));
}));
#endif
