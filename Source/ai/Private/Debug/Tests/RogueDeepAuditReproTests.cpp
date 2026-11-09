// 2026-10-31 深度审计复现：断言期望的正确行为；覆盖旧请求、结算收尾、配置排序与慢速归位。
#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueReturningTalisman.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Combat/Weapons/RogueWeapon_Talisman.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Progression/Shop/RogueShopSystem.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Progression/Upgrades/RogueUpgradeSystem.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
namespace RogueDeepAudit
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
struct FFixture
{
    FTestConsoleVariable Self{TEXT("Rogue.SelfTest.WeaponUpgradeChain")},Stress{TEXT("Rogue.SelfTest.EnemyStress")};
    FTestWorldWrapper TestWorld;UWorld* W=nullptr;ARogueCharacter* P=nullptr;ARoguePlayerState* PS=nullptr;ARogueGameMode* Mode=nullptr;
    bool Init()
    {
        Self.Set(TEXT("0"));Stress.Set(TEXT("0"));if(!TestWorld.CreateTestWorld(EWorldType::Game))return false;
        W=TestWorld.GetTestWorld();W->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();if(!TestWorld.BeginPlayInTestWorld())return false;
        Mode=W->GetAuthGameMode<ARogueGameMode>();Mode->SetActorTickEnabled(false);
        auto* PC=W->SpawnActor<ARoguePlayerController>();PS=PC->GetPlayerState<ARoguePlayerState>();PS->SetProgressionSnapshotForRun({});
        const FTransform T(FVector(0,0,200));P=W->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        auto* Table=NewObject<UDataTable>(W);Table->RowStruct=FRogueWeaponTableRow::StaticStruct();FRogueWeaponTableRow R;R.WeaponClass=ARogueWeapon_Projectile::StaticClass();R.Count=1;R.CountUpgradeType=ERogueUpgradeType::ProjectileCount;Table->AddRow(TEXT("Projectile"),R);
        R.WeaponClass=ARogueWeapon_Talisman::StaticClass();R.Count=0;R.CountUpgradeType=ERogueUpgradeType::TalismanCount;Table->AddRow(TEXT("Talisman"),R);
        FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(),TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(P,Table);
        P->FinishSpawning(T);PC->Possess(P);P->SetActorTickEnabled(false);P->GetCharacterMovement()->SetComponentTickEnabled(false);P->GetAbilitySystemComponent()->CancelAllAbilities();return P->IsAlive();
    }
    ARogueEnemy* Enemy(FVector Location,bool Boss=false)
    {
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E=W->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(),Location,FRotator::ZeroRotator,Params);E->ActivatePooledEnemy(nullptr,Location,FRotator::ZeroRotator);
        FRogueEnemyProfile Profile;Profile.Health=100;Profile.Damage=0;Profile.bIsBoss=Boss;E->InitializeEnemy(Profile);E->SetActorTickEnabled(false);E->GetCharacterMovement()->SetComponentTickEnabled(false);W->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);return E;
    }
    void Step(float Seconds){while(Seconds>0){const float D=FMath::Min(.01f,Seconds);TestWorld.TickTestWorld(D);Seconds-=D;}}
    FRogueUpgradeSystem Only(ERogueUpgradeType Type)
    {
        auto* Rule=NewObject<URogueUpgradeRuleAsset>(W);Rule->BaseUpgradeTypes={Type};Rule->DefaultWeaponUpgradeTypes.Reset();Rule->WeaponUpgradeRules.Reset();Rule->OpeningOffenseLevels=0;FRogueUpgradeSystem S;S.SetRuleAsset(Rule);return S;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAuditShopQuote,"Rogue.Audit.Shop.StaleIndexBuysDifferentRefreshedCard",RogueDeepAudit::Flags)
bool FRogueAuditShopQuote::RunTest(const FString&)
{
    RogueDeepAudit::FFixture F;if(!F.Init())return false;auto A=F.Only(ERogueUpgradeType::MaxHealth),B=F.Only(ERogueUpgradeType::MoveSpeed);
    F.P->AddMoney(100);FRogueShopSystem Shop;Shop.Configure(1,10,1,5);if(!Shop.OpenShop(F.P,A))return false;
    const auto OldQuote=Shop.GetOffers()[0]; const auto OldType=OldQuote.GetUpgrade().Type;Shop.AdvanceAutoRefresh(6,F.P,B);
    const int Before=F.P->GetMoney();FRogueUpgradeOption Bought;const bool Accepted=Shop.TryBuyOffer(0,OldQuote.QuoteId,OldQuote.GetCost(),F.P,B,Bought);
    AddInfo(FString::Printf(TEXT("Client saw type=%d; old index accepted=%d; bought type=%d; charged=%d"),int(OldType),Accepted,int(Bought.Type),Before-F.P->GetMoney()));
    TestFalse(TEXT("Purchase for a stale displayed quote must not buy its replacement"),Accepted);TestEqual(TEXT("Stale quote must not charge money"),F.P->GetMoney(),Before);
    const auto Current=Shop.GetOffers()[0];
    TestFalse(TEXT("Even a current ID must not accept an old price"),Shop.TryBuyOffer(0,Current.QuoteId,Current.GetCost()+1,F.P,B,Bought));
    TestEqual(TEXT("Wrong price is not charged"),F.P->GetMoney(),Before);
    TestTrue(TEXT("Fresh displayed quote still buys normally"),Shop.TryBuyOffer(0,Current.QuoteId,Current.GetCost(),F.P,B,Bought));
    TestEqual(TEXT("Fresh quote charges exactly its price"),F.P->GetMoney(),Before-Current.GetCost());
    TestFalse(TEXT("Replaying a purchased quote cannot charge twice"),Shop.TryBuyOffer(0,Current.QuoteId,Current.GetCost(),F.P,B,Bought));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAuditDoubleSelect,"Rogue.Audit.Upgrades.RepeatedOldClickConsumesNextQueuedChoice",RogueDeepAudit::Flags)
bool FRogueAuditDoubleSelect::RunTest(const FString&)
{
    RogueDeepAudit::FFixture F;if(!F.Init())return false;auto System=F.Only(ERogueUpgradeType::MaxHealth);
    *FindFProperty<FStructProperty>(ARogueGameMode::StaticClass(),TEXT("UpgradeSystem"))->ContainerPtrToValuePtr<FRogueUpgradeSystem>(F.Mode)=System;
    FRogueUpgradeOption Initial;Initial.Type=ERogueUpgradeType::MaxHealth;Initial.Magnitude=20;F.PS->QueueUpgradeSelections(1);F.PS->OpenUpgradeSelection({Initial});if(!TestTrue(TEXT("First choice opened"),F.PS->IsAwaitingUpgradeChoice()))return false;
    const FGuid OldChoice=F.PS->GetPendingUpgrades()[0].SelectionId;
    F.PS->Server_RequestUpgradeSelection(0,OldChoice);TestTrue(TEXT("Second choice waits after first click"),F.PS->IsAwaitingUpgradeChoice());
    F.PS->Server_RequestUpgradeSelection(0,OldChoice);
    AddInfo(FString::Printf(TEXT("Two identical clicks without a new client snapshot: picks=%d awaiting=%d"),F.P->GetUpgradePickCount(ERogueUpgradeType::MaxHealth),F.PS->IsAwaitingUpgradeChoice()));
    TestEqual(TEXT("Old duplicate click must apply only one card"),F.P->GetUpgradePickCount(ERogueUpgradeType::MaxHealth),1);
    TestTrue(TEXT("Next queued choice must remain for the player to review"),F.PS->IsAwaitingUpgradeChoice());
    if(!F.PS->IsAwaitingUpgradeChoice())return false;
    const FGuid Next=F.PS->GetPendingUpgrades()[0].SelectionId;
    TestTrue(TEXT("Identical card text still belongs to a distinct new round"),Next.IsValid() && Next!=OldChoice);
    F.PS->SubmitUpgradeSelection(0,OldChoice);
    TestEqual(TEXT("Client helper also rejects an old round"),F.P->GetUpgradePickCount(ERogueUpgradeType::MaxHealth),1);
    F.PS->SubmitUpgradeSelection(0,Next);
    TestEqual(TEXT("New round remains selectable"),F.P->GetUpgradePickCount(ERogueUpgradeType::MaxHealth),2);
    TestFalse(TEXT("Queue completes after the fresh choice"),F.PS->IsAwaitingUpgradeChoice());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAuditPostDefeat,"Rogue.Audit.EndState.InFlightProjectileAwardsBossAfterTeamDefeat",RogueDeepAudit::Flags)
bool FRogueAuditPostDefeat::RunTest(const FString&)
{
    RogueDeepAudit::FFixture F;if(!F.Init())return false;auto* E=F.Enemy(FVector(600,0,200),true);
    auto* Shot=F.W->SpawnActor<ARogueProjectile>();Shot->ActivatePooledProjectile(F.P,F.P,FVector(100,0,200),FRotator::ZeroRotator,FVector::ForwardVector,1000,100000);
    auto* Source=F.W->SpawnActor<AActor>();RogueAbilitySystem::ApplyDamageToTarget(Source,F.P,F.P->GetMaxHealth()*10,RogueGameplayTags::Damage_Source_Contact);F.Step(.05f);
    if(!TestTrue(TEXT("The match has genuinely ended in defeat"),F.Mode->IsGameOver()))return false;
    const int Bricks=F.PS->GetBossIceBricksEarnedThisRun(),Kills=F.Mode->GetEnemiesDefeated();F.Step(1.f);
    AddInfo(FString::Printf(TEXT("After defeat: reward %d -> %d, kills %d -> %d, boss retired=%d"),Bricks,F.PS->GetBossIceBricksEarnedThisRun(),Kills,F.Mode->GetEnemiesDefeated(),!IsValid(E)));
    TestEqual(TEXT("Ended match must not grant new boss currency"),F.PS->GetBossIceBricksEarnedThisRun(),Bricks);
    TestEqual(TEXT("Ended match must freeze kill statistics"),F.Mode->GetEnemiesDefeated(),Kills);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAuditSlowReturn,"Rogue.Audit.Talisman.SlowReturnNeverCatchesMovingOrbit",RogueDeepAudit::Flags)
bool FRogueAuditSlowReturn::RunTest(const FString&)
{
    RogueDeepAudit::FFixture F;if(!F.Init())return false;F.Enemy(FVector(650,0,200));auto* C=F.W->SpawnActor<ARogueReturningTalisman>();
    FRogueTalismanCombatSettings Combat;Combat.ReturnSpeed=100;Combat.MaxSortieSeconds=1;
    C->bEnableTrail=false;C->OrbitRadius=180;C->OrbitSpeed=75;C->Configure(F.P,0,1,10,2000,1,1,1,Combat);if(!C->TryLaunch())return false;
    F.Step(15.f);AddInfo(FString::Printf(TEXT("After 15 seconds: phase=%d, hits=%d, valid authored return speed=100, orbit tangential speed=235.6"),int(C->GetFlightPhase()),C->GetSortieHitCount()));
    TestTrue(TEXT("Valid return settings must eventually rejoin orbit or recover"),C->GetFlightPhase()==ERogueTalismanPhase::Orbiting);C->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAuditSpawnOverlap,"Rogue.Audit.Projectile.SpawnOverlapResolvesOnceAndStaysPooled",RogueDeepAudit::Flags)
bool FRogueAuditSpawnOverlap::RunTest(const FString&)
{
    RogueDeepAudit::FFixture F;if(!F.Init())return false;auto* E=F.Enemy(FVector(600,0,200));auto* C=F.W->SpawnActor<ARogueProjectile>();
    C->ActivatePooledProjectile(F.P,F.P,E->GetActorLocation(),FRotator::ZeroRotator,FVector::ForwardVector,1000,10);
    F.Step(.03f);
    auto* Movement=C->FindComponentByClass<UProjectileMovementComponent>();
    if(!TestEqual(TEXT("Projectile spawned inside an enemy resolves exactly one real hit"),E->GetCurrentHealth(),90.f))return false;
    TestTrue(TEXT("Hit returns the actor to its pool"),C->IsAvailableInPool());
    AddInfo(FString::Printf(TEXT("Pooled=%d movementActive=%d velocity=%.1f actorTick=%d"),C->IsAvailableInPool(),Movement->IsActive(),Movement->Velocity.Size(),C->IsActorTickEnabled()));
    TestFalse(TEXT("A pooled actor must not restart its movement component"),Movement->IsActive());
    TestTrue(TEXT("Pooled velocity must stay zero"),Movement->Velocity.IsNearlyZero());
    const FVector Before=C->GetActorLocation();F.Step(.1f);TestTrue(TEXT("Pooled hidden actor must stay still"),C->GetActorLocation().Equals(Before,1.f));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAuditWeaponRowOrder,"Rogue.Audit.WeaponTable.ReorderingRowsChangesWeaponIdentity",RogueDeepAudit::Flags)
bool FRogueAuditWeaponRowOrder::RunTest(const FString&)
{
    RogueDeepAudit::FFixture F;if(!F.Init())return false;
    auto* Original=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Weapons/DT_WeaponConfig.DT_WeaponConfig"));
    auto* BP=LoadClass<ARogueCharacter>(nullptr,TEXT("/Game/BP/Player/BP_RogueCharacter.BP_RogueCharacter_C"));
    if(!Original || !BP)return false;
    const auto* Cold=Original->FindRow<FRogueWeaponTableRow>(TEXT("Projectile"),TEXT("Audit"));if(!Cold)return false;
    auto* Reordered=NewObject<UDataTable>(F.W);Reordered->RowStruct=FRogueWeaponTableRow::StaticStruct();
    TArray<FName> Names=Original->GetRowNames();Names.Sort([](FName A,FName B){return A.ToString()<B.ToString();});
    for(FName N:Names)
    {
        auto Row=*Original->FindRow<FRogueWeaponTableRow>(N,TEXT("Audit"));
        // 正式表也会迁移为显式类；这里仍覆盖旧表回退，避免显式配置掩盖索引依赖。
        if(Row.CountUpgradeType!=ERogueUpgradeType::TalismanCount)
        {
            Row.WeaponClass=nullptr;
            if(N!=TEXT("Mortar"))Row.CountUpgradeType=ERogueUpgradeType::ProjectileCount;
        }
        Reordered->AddRow(N,Row);
    }
    const FTransform T(FVector(0,1000,200));auto* P=F.W->SpawnActorDeferred<ARogueCharacter>(BP,T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(),TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(P,Reordered);P->FinishSpawning(T);P->SetActorTickEnabled(false);P->GetCharacterMovement()->SetComponentTickEnabled(false);
    auto* Weapon=P->FindWeapon<ARogueWeapon_Projectile>();if(!TestNotNull(TEXT("Projectile logic still exists after row reorder"),Weapon))return false;
    AddInfo(FString::Printf(TEXT("Projectile logic after alphabetical row reorder: expected name=%s actual name=%s actual base damage=%.2f"),*Cold->DisplayName,*Weapon->GetConfig().DisplayName,Weapon->GetConfig().Damage));
    TestEqual(TEXT("Reordering data rows must not change the weapon-to-config binding"),Weapon->GetConfig().DisplayName,Cold->DisplayName);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAuditLateCallbacks,"Rogue.Audit.EndState.RejectsLateDamageAndRewardCallbacks",RogueDeepAudit::Flags)
bool FRogueAuditLateCallbacks::RunTest(const FString&)
{
    RogueDeepAudit::FFixture F;if(!F.Init())return false;
    F.P->Die();F.Step(.02f);if(!TestTrue(TEXT("Real defeat reached"),F.Mode->IsGameOver()))return false;
    auto* Late=F.Enemy(FVector(900,0,200),true);
    TestFalse(TEXT("Damage entry rejects work after match end"),RogueAbilitySystem::ApplyDamageToTarget(F.P,Late,99999,RogueGameplayTags::Damage_Source_Projectile));
    TestEqual(TEXT("Rejected damage leaves health unchanged"),Late->GetCurrentHealth(),100.f);
    const int Kills=F.Mode->GetEnemiesDefeated(),Bricks=F.PS->GetBossIceBricksEarnedThisRun();
    F.Mode->HandleEnemyKilled(Late,F.P);
    TestEqual(TEXT("Late callbacks cannot change kill totals"),F.Mode->GetEnemiesDefeated(),Kills);
    TestEqual(TEXT("Late callbacks cannot grant currency"),F.PS->GetBossIceBricksEarnedThisRun(),Bricks);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAuditQuoteSerialization,"Rogue.Audit.Network.CardTokensAndPayloadRoundTripTogether",RogueDeepAudit::Flags)
bool FRogueAuditQuoteSerialization::RunTest(const FString&)
{
    FRogueShopOffer Original;Original.QuoteId=FGuid::NewGuid();Original.Reward.Cost=17;
    Original.Reward.Upgrade.SelectionId=FGuid::NewGuid();Original.Reward.Upgrade.Type=ERogueUpgradeType::MoveSpeed;
    Original.Reward.Upgrade.Title=TEXT("speed");Original.Reward.Upgrade.Description=TEXT("quoted payload");Original.Reward.Upgrade.Magnitude=3;
    TArray<uint8> Bytes;bool Ok=false;FMemoryWriter Writer(Bytes);Original.NetSerialize(Writer,nullptr,Ok);
    TestTrue(TEXT("Offer serializes"),Ok);FRogueShopOffer Received;FMemoryReader Reader(Bytes);Received.NetSerialize(Reader,nullptr,Ok);
    TestTrue(TEXT("Offer deserializes"),Ok);TestEqual(TEXT("Quote ID travels with price"),Received.QuoteId,Original.QuoteId);
    TestEqual(TEXT("Price preserved"),Received.GetCost(),17);TestEqual(TEXT("Choice ID preserved"),Received.GetUpgrade().SelectionId,Original.GetUpgrade().SelectionId);
    TestTrue(TEXT("Type preserved"),Received.GetUpgrade().Type==Original.GetUpgrade().Type);
    TestEqual(TEXT("Description preserved"),Received.GetUpgrade().Description,Original.GetUpgrade().Description);return true;
}
// 仅通过 Rogue.Test.AuditRequests 显式启动的双进程回归，不参与正式玩法。
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace RogueRequestNetworkProbe
{
struct FProbe
{
    double Start=FPlatformTime::Seconds(),PhaseTime=Start;
    int Phase=0,InitialPicks=0,Money=0,RefreshCost=0,OfferCost=0;
    FGuid Choice,Quote;
    TWeakObjectPtr<ARoguePlayerState> Target;
    bool Failed=false;
    bool Check(bool Value,const TCHAR* What)
    {
        if(!Value){Failed=true;UE_LOG(LogTemp,Error,TEXT("[AuditRequests] FAIL %s"),What);}return Value;
    }
    bool Tick()
    {
        UWorld* W=nullptr;
        if(GEngine)for(const auto& Context:GEngine->GetWorldContexts())
            if(Context.World() && Context.World()->IsGameWorld()){W=Context.World();break;}
        if(!W)return FPlatformTime::Seconds()-Start<90;
        const double Now=FPlatformTime::Seconds();
        if(Failed || Now-Start>90)
        {
            UE_LOG(LogTemp,Error,TEXT("[AuditRequests] FAIL terminal mode=%d phase=%d"),int(W->GetNetMode()),Phase);
            GEngine->Exec(W,TEXT("QUIT"));return false;
        }
        if(auto* Mode=W->GetAuthGameMode<ARogueGameMode>())
        {
            // 成功后客户端正常退出，不能再把它的离开判成业务失败。
            if(Phase==6){if(Now-PhaseTime>2){GEngine->Exec(W,TEXT("QUIT"));return false;}return true;}
            if(Phase==0)
            {
                // 等待客户端期间先隔离战斗，不能让等待时间改变测试的对局状态。
                Mode->SetActorTickEnabled(false);
                for(TActorIterator<ARogueEnemy> It(W);It;++It)if(!It->IsAvailableInPool())It->DeactivateToPool();
                if(!Check(!Mode->IsGameOver(),TEXT("Setup reached an ended match")))return true;
                TArray<ARoguePlayerState*> Ready;
                for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
                    if(auto* PC=It->Get())if(auto* S=PC->GetPlayerState<ARoguePlayerState>();S && S->IsBaseAttributesInitialized() && S->IsProgressionReady())
                    {Ready.Add(S);if(!PC->IsLocalController())Target=S;}
                if(Ready.Num()<2 || !Target.IsValid())return true;
                Mode->SetActorTickEnabled(false);
                for(auto* S:Ready)if(auto* P=S->GetPawn<ARogueCharacter>())P->GetAbilitySystemComponent()->CancelAllAbilities();
                for(TActorIterator<ARogueEnemy> It(W);It;++It)It->DeactivateToPool();
                auto* Rules=NewObject<URogueUpgradeRuleAsset>(W);Rules->BaseUpgradeTypes={ERogueUpgradeType::MaxHealth};
                Rules->DefaultWeaponUpgradeTypes.Reset();Rules->WeaponUpgradeRules.Reset();Rules->OpeningOffenseLevels=0;
                auto* System=FindFProperty<FStructProperty>(ARogueGameMode::StaticClass(),TEXT("UpgradeSystem"))->ContainerPtrToValuePtr<FRogueUpgradeSystem>(Mode);
                System->SetRuleAsset(Rules);
                auto* S=Target.Get();InitialPicks=S->GetUpgradePickCount(ERogueUpgradeType::MaxHealth);
                FRogueUpgradeOption Option;Option.Type=ERogueUpgradeType::MaxHealth;Option.Magnitude=1;Option.Title=TEXT("Network round");
                S->QueueUpgradeSelections(1);S->OpenUpgradeSelection({Option});Phase=1;
                UE_LOG(LogTemp,Display,TEXT("[AuditRequests] Server opened two queued rounds"));
            }
            else
            {
                auto* S=Target.Get();if(!S){Check(false,TEXT("Remote player lost"));return true;}
                auto* P=S->GetPawn<ARogueCharacter>();if(!P){Check(false,TEXT("Remote pawn lost"));return true;}
                const int Picks=S->GetUpgradePickCount(ERogueUpgradeType::MaxHealth);
                if(Phase==1 && Picks==InitialPicks+1){Phase=2;PhaseTime=Now;}
                if(Phase==2 && Now-PhaseTime>1)
                {
                    if(!Check(Picks==InitialPicks+1 && S->IsAwaitingUpgradeChoice(),TEXT("Duplicate consumed unseen round")))return true;
                    UE_LOG(LogTemp,Display,TEXT("[AuditRequests] PASS duplicate old selection rejected"));Phase=3;
                }
                if(Phase==3 && Picks==InitialPicks+2)
                {
                    P->AddMoney(1000);Mode->TryOpenShop(P);
                    auto& Shop=S->GetShopRuntime();if(!Check(Shop.IsOpen() && Shop.GetOffers().Num()>0,TEXT("Shop failed to open")))return true;
                    Money=P->GetMoney();RefreshCost=Shop.GetRefreshCost();Quote=Shop.GetOffers()[0].QuoteId;Phase=4;
                }
                if(Phase==4)
                {
                    const auto& Shop=S->GetShopRuntime();
                    if(Shop.GetOffers().Num() && Shop.GetOffers()[0].QuoteId!=Quote)
                    {
                        if(!Check(P->GetMoney()==Money-RefreshCost && !Shop.GetOffers()[0].IsPurchased() && Picks==InitialPicks+2,TEXT("Stale shop request charged or applied")))return true;
                        OfferCost=Shop.GetOffers()[0].GetCost();Phase=5;
                        UE_LOG(LogTemp,Display,TEXT("[AuditRequests] PASS stale quote rejected without purchase charge"));
                    }
                }
                if(Phase==5 && S->GetShopRuntime().GetOffers()[0].IsPurchased())
                {
                    if(!Check(P->GetMoney()==Money-RefreshCost-OfferCost && Picks==InitialPicks+3,TEXT("Fresh quote did not apply exactly once")))return true;
                    UE_LOG(LogTemp,Display,TEXT("[AuditRequests] PASS SERVER fresh choices and purchase, picks=%d"),Picks-InitialPicks);
                    Phase=6;PhaseTime=Now;
                }
            }
        }
        else if(W->GetNetMode()==NM_Client)
        {
            auto* PC=W->GetFirstPlayerController();auto* S=PC?PC->GetPlayerState<ARoguePlayerState>():nullptr;
            if(!S)return true;
            const auto& Options=S->GetPendingUpgrades();const auto& Offers=S->GetShopOffers();
            if(Phase==0 && S->IsAwaitingUpgradeChoice() && Options.Num() && Options[0].SelectionId.IsValid())
            {
                Choice=Options[0].SelectionId;S->Server_RequestUpgradeSelection(0,Choice);S->Server_RequestUpgradeSelection(0,Choice);Phase=1;
            }
            if(Phase==1 && Options.Num() && Options[0].SelectionId.IsValid() && Options[0].SelectionId!=Choice){Choice=Options[0].SelectionId;PhaseTime=Now;Phase=2;}
            if(Phase==2 && Now-PhaseTime>3){S->SubmitUpgradeSelection(0,Choice);Phase=3;}
            if(Phase==3 && S->IsShopOpen() && Offers.Num() && Offers[0].QuoteId.IsValid())
            {
                Quote=Offers[0].QuoteId;const int Price=Offers[0].GetCost();
                S->Server_RequestRefreshShop();S->Server_RequestBuyShopOffer(0,Quote,Price);Phase=4;
            }
            if(Phase==4 && Offers.Num() && Offers[0].QuoteId.IsValid() && Offers[0].QuoteId!=Quote){Quote=Offers[0].QuoteId;PhaseTime=Now;Phase=5;}
            if(Phase==5 && Now-PhaseTime>3){S->Server_RequestBuyShopOffer(0,Quote,Offers[0].GetCost());Phase=6;}
            if(Phase==6 && Offers.Num() && Offers[0].IsPurchased())
            {
                UE_LOG(LogTemp,Display,TEXT("[AuditRequests] PASS CLIENT replicated new round and purchased quote"));GEngine->Exec(W,TEXT("QUIT"));return false;
            }
        }
        return true;
    }
};
}
static FAutoConsoleCommand RogueAuditRequestsCommand(TEXT("Rogue.Test.AuditRequests"),TEXT("Explicit isolated two-process request replay regression."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe=RogueRequestNetworkProbe::FProbe()](float) mutable {return Probe.Tick();}),.05f);
    }));

#endif
