#if WITH_DEV_AUTOMATION_TESTS
#include "Enemies/RogueEnemy.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "RogueEnemyHealingTestAccess.h"
#include "UI/RogueHUD.h"
#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Enemies/Visuals/RogueEnemyEffectVisual.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameModeRules.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
namespace RogueRolePreview
{
    UWorld* GameWorld()
    {
        if (!GEngine) return nullptr;
        for (const auto& C:GEngine->GetWorldContexts())
            if (C.World()&&C.World()->IsGameWorld()&&C.World()->GetGameInstance()) return C.World();
        return nullptr;
    }
    ARogueEnemy* Spawn(UWorld* W,const TCHAR* Name,ERogueEnemyType Type,FVector Center,int32 Wave,bool Boss=false)
    {
        const FString Path=RogueEnemyAssetPaths::EnemyClassPath(Name);
        auto* Class=LoadClass<ARogueEnemy>(nullptr,*Path);if (!Class) return nullptr;
        FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E=W->SpawnActor<ARogueEnemy>(Class,Center,FRotator::ZeroRotator,P);if (!E) return nullptr;
        auto Profile=Boss?RogueGameModeRules::BuildBossProfile(Wave,Type,URogueGameBalanceAsset::GetDefaultGameBalance())
            :RogueGameModeRules::BuildEnemyProfile(Wave,Type,URogueGameBalanceAsset::GetDefaultGameBalance());
        Profile.Speed=0;Profile.Damage=0;Profile.ExperienceReward=0;
        E->ActivatePooledEnemy(nullptr,Center,FRotator::ZeroRotator);E->InitializeEnemy(Profile);
        Center.Z+=E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-72;
        E->SetSpawnGroundReference(Center);E->SetActorTickEnabled(false);
        E->GetCharacterMovement()->StopMovementImmediately();E->GetCharacterMovement()->SetComponentTickEnabled(false);
        W->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);return E;
    }
}
static FAutoConsoleCommand RolePreview(TEXT("Rogue.Test.EnemyRoles"),TEXT("Temporary Boss, flying and healer scene. Optional required player count; never saves assets."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        const int32 Count=Args.Num()?FMath::Max(1,FCString::Atoi(*Args[0])):1;
        auto Start=FPlatformTime::Seconds();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Count,Start](float)
        {
            if (FPlatformTime::Seconds()-Start>90) { UE_LOG(LogTemp,Error,TEXT("[EnemyRolesScene] FAIL player initialization timeout"));return false; }
            UWorld* W=RogueRolePreview::GameWorld();if (!W||!W->GetAuthGameMode<ARogueGameMode>()) return true;
            W->GetAuthGameMode()->SetActorTickEnabled(false);
            // 仅显式测试命令：客户端初始化期间不让主机先被普通波次击杀。
            for (TActorIterator<ARogueEnemy> It(W);It;++It) It->DeactivateToPool();
            TArray<ARogueCharacter*> Players;
            for (FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
            {
                auto* PC=It->Get();auto* P=PC?Cast<ARogueCharacter>(PC->GetPawn()):nullptr;
                auto* S=P?P->GetPlayerState<ARoguePlayerState>():nullptr;
                if (P&&S&&S->IsBaseAttributesInitialized()) Players.Add(P);
            }
            if (Players.Num()<Count) return true;
            W->GetAuthGameMode()->SetActorTickEnabled(false);
            for (TActorIterator<ARogueEnemy> It(W);It;++It) It->DeactivateToPool();
            for (TActorIterator<ARogueWeaponBase> It(W);It;++It) It->Destroy();
            const FVector O=Players[0]->GetActorLocation();
            for (auto* P:Players) { P->SetActorTickEnabled(false);P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetComponentTickEnabled(false); }
            using namespace RogueRolePreview;
            const FVector Base=O+FVector(0,0,-24);
            auto* Boss=Spawn(W,TEXT("Rifleman"),ERogueEnemyType::Rifleman,Base+FVector(550,0,0),20,true);
            auto* Fly=Spawn(W,TEXT("SkyMoth"),ERogueEnemyType::SkyMoth,Base+FVector(850,0,0),14);
            auto* Area=Spawn(W,TEXT("AuraHealer"),ERogueEnemyType::AuraHealer,Base+FVector(250,-450,0),20);
            auto* Link=Spawn(W,TEXT("LinkHealer"),ERogueEnemyType::LinkHealer,Base+FVector(250,450,0),20);
            auto* A=Spawn(W,TEXT("Assault"),ERogueEnemyType::Assault,Base+FVector(550,-450,0),20);
            auto* B=Spawn(W,TEXT("Juggernaut"),ERogueEnemyType::Juggernaut,Base+FVector(550,450,0),20);
            if (!Boss||!Fly||!Area||!Link||!A||!B) { UE_LOG(LogTemp,Error,TEXT("[EnemyRolesScene] FAIL missing class"));return false; }
            RogueAbilitySystem::ApplyDamageToTarget(Area,A,A->GetMaxHealth()*.6f,RogueGameplayTags::Damage_Source_Projectile);
            RogueAbilitySystem::ApplyDamageToTarget(Link,B,B->GetMaxHealth()*.6f,RogueGameplayTags::Damage_Source_Projectile);
            UE_LOG(LogTemp,Display,TEXT("[EnemyRolesScene] READY Boss=%s Fly=%s Area=%s Link=%s TargetA=%.3f TargetB=%.3f Players=%d"),
                *Boss->GetName(),*Fly->GetName(),*Area->GetName(),*Link->GetName(),A->GetCurrentHealth(),B->GetCurrentHealth(),Players.Num());
            return false;
        }),.1f);
    }));
static FAutoConsoleCommand RoleObserve(TEXT("Rogue.Test.ObserveEnemyRoles"),TEXT("Client validates replicated Boss profile and local healing/flying visuals."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        const double Start=FPlatformTime::Seconds();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Start](float)
        {
            UWorld* W=RogueRolePreview::GameWorld();
            if (FPlatformTime::Seconds()-Start>90) { UE_LOG(LogTemp,Error,TEXT("[EnemyRolesNet] FAIL timeout"));return false; }
            if (!W||W->GetNetMode()!=NM_Client) return true;
            bool Boss=false,Fly=false,Link=false;int32 Visible=0;
            for (TActorIterator<ARogueEnemy> It(W);It;++It)
            {
                auto* E=*It;if (E->IsAvailableInPool()||E->IsDead()) continue;
                if (E->IsBoss()&&E->GetMaxHealth()>1000) Boss=true;
                if (E->GetEnemyType()==ERogueEnemyType::SkyMoth) Fly=E->GetCurrentArchetype().Flying.HoverHeight==110;
                if (E->GetEnemyType()==ERogueEnemyType::LinkHealer)
                    if (auto* S=E->FindComponentByClass<URogueEnemySupportComponent>()) Link=S->GetHealingTarget()!=nullptr;
            }
            for (TActorIterator<ARogueEnemyEffectVisual> It(W);It;++It) if (!It->IsHidden()) ++Visible;
            auto* PC=W->GetFirstPlayerController();
            const int32 Green=FRogueEnemyHealingTestAccess::GreenCount(PC?Cast<ARogueHUD>(PC->GetHUD()):nullptr);
            if (Boss&&Fly&&Link&&Visible>=4&&Green>0)
            { UE_LOG(LogTemp,Display,TEXT("[EnemyRolesNet] PASS Boss=%d Fly=%d Link=%d VisibleEffects=%d HealingNumbers=%d"),Boss,Fly,Link,Visible,Green);return false; }
            return true;
        }),.25f);
    }));
#endif
