// Explicit QA command: reads live map collision and moves existing enemy Blueprints across occluded pairs.
#include "Enemies/RogueEnemy.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystemComponent.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Enemies/Components/RogueEnemyObstacleMovementComponent.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "Enemies/Spawning/RogueEnemyNavigationBudget.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Engine/Engine.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "World/RogueEnemySpawnRegion.h"
#include "World/RogueRangedSpawnPoint.h"
#include "Enemies/Components/RogueBossSkillComponent.h"

namespace RogueRoutingMapProbe
{
struct FRoute
{
    TWeakObjectPtr<ARogueEnemy> Enemy;
    FVector Start,Goal,Previous;
    float Travel=0,Arrival=-1,MaxZChange=0;
};
struct FProbe
{
    FString Folder;double Started=FPlatformTime::Seconds(),ReadyWorld=-1;
    TWeakObjectPtr<UWorld> World;TArray<FRoute> Routes;
    int32 PeakQueries=0;int64 Queries=0;bool Shot=false,bFixedSites=false;

    bool Ground(UWorld* W,ARogueEnemySpawnRegion* Region,FVector Candidate,FVector& Out)
    {
        FVector A,B;if(!Region->ClipGroundTrace(Candidate,3000,5000,A,B))return false;
        FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(RouteProbeGround),false);
        for(TActorIterator<APawn> It(W);It;++It)Q.AddIgnoredActor(*It);
        if(!W->LineTraceSingleByChannel(H,A,B,ECC_Pawn,Q)||H.ImpactNormal.Z<.7||!Region->IsGroundHeightAllowed(H.ImpactPoint))return false;
        Out=H.ImpactPoint+FVector(0,0,74);
        // This is an outdoor movement probe. Sealed spaces underneath a roof are
        // covered by the separate unreachable-goal test, not asserted reachable.
        FHitResult Roof;
        if(W->LineTraceSingleByChannel(Roof,Out+FVector(0,0,70),Out+FVector(0,0,1500),ECC_Pawn,Q))return false;
        return Region->ContainsCapsule(Out,40,72)&&!W->OverlapBlockingTestByChannel(Out,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(40,64),Q);
    }



    bool Setup(UWorld* W,ARogueCharacter* P)
    {
        W->GetAuthGameMode()->SetActorTickEnabled(false);W->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);
        for(TActorIterator<ARogueEnemy> E(W);E;++E)E->DeactivateToPool();
        for(ARogueWeaponBase* Weapon:P->GetWeapons())if(Weapon){Weapon->OnOwnerDied();Weapon->GetConfig().Count=0;Weapon->GetConfig().MaxCount=0;}
        P->GetAbilitySystemComponent()->CancelAllAbilities();
        P->GetPlayerState<ARoguePlayerState>()->Server_SetPauseMenuOpen(true);
        P->SetActorEnableCollision(false);
        P->GetCharacterMovement()->StopMovementImmediately();
        P->GetCharacterMovement()->SetComponentTickEnabled(false);
        ARogueEnemySpawnRegion* Region=nullptr;for(TActorIterator<ARogueEnemySpawnRegion> R(W);R;++R){Region=*R;break;}
        if(!Region)return false;
        FMath::RandInit(11042026);
        TArray<TPair<FVector,FVector>> Pairs;
        if(bFixedSites)
        {
            FVector Goal;
            if(!Ground(W,Region,P->GetActorLocation(),Goal))return false;
            for(TActorIterator<ARogueRangedSpawnPoint> It(W);It;++It)
            {
                if(!It->bEnabled || It->SpawnRegion!=Region)continue;
                FVector Start;
                if(!Ground(W,Region,It->GetActorLocation(),Start))return false;
                Pairs.Emplace(Start,Goal);
            }
            if(Pairs.Num()!=12)return false;
        }
        for(int32 Attempt=0;!bFixedSites&&Attempt<20000&&Pairs.Num()<3;++Attempt)
        {
            FVector A,B;
            if(!Ground(W,Region,Region->GetRandomCandidate(),A))continue;
            const float Angle=FMath::FRandRange(0.f,360.f),Distance=FMath::FRandRange(900.f,1800.f);
            if(!Ground(W,Region,A+FVector::ForwardVector.RotateAngleAxis(Angle,FVector::UpVector)*Distance,B))continue;
            if(FMath::Abs(A.Z-B.Z)>30)continue;
            FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(RouteProbePair),false);
            const bool Blocked=W->SweepSingleByChannel(H,A+FVector(0,0,4),B+FVector(0,0,4),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,64),Q);
            if(!Blocked||H.ImpactNormal.Z>=.7||H.bStartPenetrating)continue;
            bool Distinct=true;for(const auto& Pair:Pairs)if(FVector::Dist2D(A,Pair.Key)<400)Distinct=false;
            if(Distinct)Pairs.Emplace(A,B);
        }
        if(Pairs.Num()<3){UE_LOG(LogTemp,Error,TEXT("[EnemyRouteMap] not enough blocked pairs: %d"),Pairs.Num());return false;}

        const TCHAR* Names[]={TEXT("Juggernaut"),TEXT("LinkHealer"),TEXT("SkyMoth"),TEXT("Juggernaut"),TEXT("Assault"),TEXT("AuraHealer")};
        const ERogueEnemyType Types[]={ERogueEnemyType::Juggernaut,ERogueEnemyType::LinkHealer,ERogueEnemyType::SkyMoth,ERogueEnemyType::Juggernaut,ERogueEnemyType::Assault,ERogueEnemyType::AuraHealer};
        for(int32 I=0;I<(bFixedSites?Pairs.Num():6);++I)
        {
            const auto& Pair=Pairs[bFixedSites?I:I%3];const FVector A=bFixedSites||I<3?Pair.Key:Pair.Value,B=bFixedSites||I<3?Pair.Value:Pair.Key;
            auto* Class=LoadClass<ARogueEnemy>(nullptr,*RogueEnemyAssetPaths::EnemyClassPath(Names[I%6]));if(!Class)return false;
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* E=W->SpawnActor<ARogueEnemy>(Class,A,FRotator::ZeroRotator,Params);E->ActivatePooledEnemy(nullptr,A,FRotator::ZeroRotator);
            E->InitializeEnemy({Types[I%6],100000,bFixedSites?600.f:300.f,0,0,I==3});E->SetActorTickEnabled(false);E->GetAbilitySystemComponent()->CancelAllAbilities();
            if(E->BossSkills){E->BossSkills->ResetForPool();E->BossSkills->SetComponentTickEnabled(false);}
            FVector Goal=B;if(RogueEnemyArchetypes::IsFlyingType(Types[I%6]))Goal.Z=E->GetActorLocation().Z;
            else Goal.Z+=E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-72.f;
            Routes.Add({E,E->GetActorLocation(),Goal,E->GetActorLocation()});
            UE_LOG(LogTemp,Display,TEXT("[EnemyRouteMap] start %d %s boss=%d %s -> %s"),I,Names[I%6],I==3,*A.ToCompactString(),*Goal.ToCompactString());
        }
        if(!IsRunningDedicatedServer())
        {
            auto* Camera=W->SpawnActor<ACameraActor>();Camera->SetActorLocation((Pairs[1].Key+Pairs[1].Value)*.5+FVector(0,0,5000));Camera->SetActorRotation(FRotator(-90,0,0));
            Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);Camera->GetCameraComponent()->OrthoWidth=4500;
            W->GetFirstPlayerController()->SetViewTarget(Camera);
        }
        ReadyWorld=W->GetTimeSeconds();World=W;return true;
    }

    void Finish(bool Passed)
    {
        FString Result=FString::Printf(TEXT("map=%s\npassed=%d\npeakQueries=%d\nqueries=%lld\n"),World.IsValid()?*World->GetMapName():TEXT("none"),Passed,PeakQueries,Queries);
        for(int32 I=0;I<Routes.Num();++I)
        {
            const auto& R=Routes[I];auto* E=R.Enemy.Get();
            Result+=FString::Printf(TEXT("%d arrived=%.3f travel=%.1f distance=%.1f zChange=%.1f final=%s %s\n"),I,R.Arrival,R.Travel,R.Arrival>=0?FVector::Dist2D(R.Previous,R.Goal):(E?FVector::Dist2D(E->GetActorLocation(),R.Goal):-1),R.MaxZChange,R.Arrival>=0?*R.Previous.ToCompactString():(E?*E->GetActorLocation().ToCompactString():TEXT("missing")),E?*E->ObstacleMovement->DescribeRoute():TEXT("missing"));
        }
        FFileHelper::SaveStringToFile(Result,*FPaths::Combine(Folder,TEXT("routes.txt")));
        UE_LOG(LogTemp,Display,TEXT("[EnemyRouteMap] %s %s"),Passed?TEXT("PASS"):TEXT("FAIL"),*Result);
        FPlatformMisc::RequestExitWithStatus(false,Passed?0:2);
    }

    bool Tick()
    {
        if(FPlatformTime::Seconds()-Started>115){Finish(false);return false;}
        if(ReadyWorld<0)
        {
            for(const auto& Context:GEngine->GetWorldContexts())
            {
                auto* W=Context.World();if(!W||!W->IsGameWorld()||!W->GetAuthGameMode<ARogueGameMode>())continue;
                auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<ARogueCharacter>(PC->GetPawn()):nullptr;
                auto* PS=P?P->GetPlayerState<ARoguePlayerState>():nullptr;if(!PS||!PS->IsBaseAttributesInitialized())continue;
                World=W;if(!Setup(W,P)){Finish(false);return false;}break;
            }
            return true;
        }
        auto* W=World.Get();if(!W){Finish(false);return false;}
        const float Age=W->GetTimeSeconds()-ReadyWorld;int32 Reached=0;
        if(Age>8&&!Shot){Shot=true;W->GetFirstPlayerController()->ConsoleCommand(FString::Printf(TEXT("Shot filename=%s -nosuffix"),*FPaths::Combine(Folder,TEXT("obstacle.png"))),false);}
        for(auto& R:Routes)
        {
            if(R.Arrival>=0){++Reached;continue;}
            auto* E=R.Enemy.Get();if(!E)continue;
            const FVector Current=E->GetActorLocation();R.Travel+=FVector::Dist2D(R.Previous,Current);R.Previous=Current;R.MaxZChange=FMath::Max(R.MaxZChange,float(FMath::Abs(Current.Z-R.Start.Z)));
            if(FMath::Abs(Current.Z-R.Start.Z)>1600){Finish(false);return false;}
            if(R.Arrival<0&&FVector::Dist2D(Current,R.Goal)<85&&FMath::Abs(Current.Z-R.Goal.Z)<120){R.Arrival=Age;E->GetCharacterMovement()->StopMovementImmediately();E->DeactivateToPool();}
            if(R.Arrival>=0){++Reached;continue;}
            E->MoveTowardsLocation(R.Goal,1,FMath::Clamp(W->GetDeltaSeconds(),.001f,.1f));
        }
        auto* B=W->GetSubsystem<URogueEnemyNavigationBudget>();PeakQueries=FMath::Max(PeakQueries,B->GetCollisionQueriesUsed());Queries+=B->GetCollisionQueriesUsed();
        if(Reached==Routes.Num()){Finish(true);return false;}
        if(Age>(bFixedSites?150:70)){Finish(false);return false;}return true;
    }
};
}
static FAutoConsoleCommand RogueEnemyRoutingProbe(TEXT("Rogue.Test.EnemyRouting"),TEXT("Live map obstacle routing QA; output folder required."),FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
    if(Args.Num()<1||Args.Num()>2)return;auto Probe=MakeShared<RogueRoutingMapProbe::FProbe>();Probe->Folder=Args[0];Probe->bFixedSites=Args.Num()>1&&Args[1]==TEXT("fixed");
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float){return Probe->Tick();}));
}));
#endif
