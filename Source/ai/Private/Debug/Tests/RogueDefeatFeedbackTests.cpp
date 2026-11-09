#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/Visuals/RogueDefeatFeedbackSubsystem.h"
#include "Combat/Visuals/RogueEnemyDefeatEffect.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "UI/Widgets/RogueKillFeedbackWidgetBase.h"

struct FRogueKillFeedbackTestAccess
{
    static void Step(URogueKillFeedbackWidgetBase* W,float DT){W->NativeTick(FGeometry(),DT);}
};

namespace RogueDefeatTests
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
struct FFixture
{
    FTestConsoleVariable Self{TEXT("Rogue.SelfTest.WeaponUpgradeChain")},Stress{TEXT("Rogue.SelfTest.EnemyStress")};
    FTestWorldWrapper TestWorld;UWorld* W=nullptr;URogueDefeatFeedbackSubsystem* Pool=nullptr;UClass* Class=nullptr;
    bool Init()
    {
        Self.Set(TEXT("0"));Stress.Set(TEXT("0"));if(!TestWorld.CreateTestWorld(EWorldType::Game))return false;
        W=TestWorld.GetTestWorld();W->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
        if(!TestWorld.BeginPlayInTestWorld())return false;W->GetAuthGameMode()->SetActorTickEnabled(false);
        Pool=W->GetSubsystem<URogueDefeatFeedbackSubsystem>();Class=LoadClass<ARogueEnemyDefeatEffect>(nullptr,TEXT("/Game/BP/Effects/BP_EnemyDefeat.BP_EnemyDefeat_C"));return Pool&&Class;
    }
    void Step(float Time){while(Time>0){TestWorld.TickTestWorld(.02f);Time-=.02f;}}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDefeatBudgetTest,"Rogue.Presentation.Defeat.GPUAnimationBudgetLifetimeAndReuse",RogueDefeatTests::Flags)
bool FRogueDefeatBudgetTest::RunTest(const FString&)
{
    using namespace RogueDefeatTests;FFixture F;if(!F.Init())return false;
    auto* A=F.Pool->Play(F.Class,FVector(0,0,300),false,1);if(!TestNotNull(TEXT("Saved defeat BP loads and plays"),A))return false;
    TestFalse(TEXT("No per-frame actor tick"),A->IsActorTickEnabled());TestFalse(TEXT("No replicated effect actor"),A->GetIsReplicated());
    TestFalse(TEXT("No actor collision"),A->GetActorEnableCollision());TestTrue(TEXT("Particle component has no collision"),A->Particles->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
    TestEqual(TEXT("One component contains all normal particles"),A->Particles->GetInstanceCount(),14);
    F.Step(.8f);TestTrue(TEXT("Timer returns effect to local pool"),A->IsAvailableInPool());TestEqual(TEXT("Pool clears stale instances"),A->Particles->GetInstanceCount(),0);
    auto* B=F.Pool->Play(F.Class,FVector(900,0,300),false,1);TestTrue(TEXT("Next kill reuses effect"),A==B);
    for(int I=0;I<70;++I)F.Pool->Play(F.Class,FVector(0,0,300),false,1);
    TestEqual(TEXT("Mass kills obey 24 simultaneous ordinary effects"),F.Pool->GetActiveCount(),24);
    TestNotNull(TEXT("Boss has reserved room at normal budget"),F.Pool->Play(F.Class,FVector(0,0,300),true,1));
    TestTrue(TEXT("Total pool remains bounded"),F.Pool->GetPoolSize()<=32);F.Step(1.3f);TestEqual(TEXT("All effects reclaim"),F.Pool->GetActiveCount(),0);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDefeatSnapshotTest,"Rogue.Presentation.Defeat.RealGASDeathSnapshotSurvivesEnemyReuse",RogueDefeatTests::Flags)
bool FRogueDefeatSnapshotTest::RunTest(const FString&)
{
    using namespace RogueDefeatTests;FFixture F;if(!F.Init())return false;
    auto* E=F.W->SpawnActor<ARogueEnemy>();E->DefeatEffectClass=F.Class;const FVector Old(0,0,300),New(900,0,300);
    FRogueEnemyProfile P;P.Health=100;P.Speed=0;E->ActivatePooledEnemy(nullptr,Old,FRotator::ZeroRotator);E->InitializeEnemy(P);E->SetActorTickEnabled(false);
    auto* Source=F.W->SpawnActor<AActor>();auto* Mode=F.W->GetAuthGameMode<ARogueGameMode>();const int Kills=Mode->GetEnemiesDefeated();
    TestTrue(TEXT("Lethal real GAS hit accepted"),RogueAbilitySystem::ApplyDamageToTarget(Source,E,10000,RogueGameplayTags::Damage_Source_Projectile));
    F.Step(.02f);TestTrue(TEXT("Enemy returns immediately without waiting for VFX"),E->IsAvailableInPool());
    TestEqual(TEXT("Gameplay reward executes once"),Mode->GetEnemiesDefeated(),Kills+1);
    E->ActivatePooledEnemy(nullptr,New,FRotator::ZeroRotator);E->InitializeEnemy(P);E->SetActorTickEnabled(false);F.Step(.10f);
    TestEqual(TEXT("Delayed batched visual still plays"),F.Pool->GetPlayedCount(),1);
    for(TActorIterator<ARogueEnemyDefeatEffect> It(F.W);It;++It)if(!It->IsAvailableInPool())TestTrue(TEXT("Visual remains at death snapshot, never follows reused enemy"),It->GetActorLocation().Equals(Old,1));
    TestEqual(TEXT("Reused enemy retains full new-life health"),E->GetCurrentHealth(),100.f);
    TestEqual(TEXT("Cosmetic flush adds no kill or reward"),Mode->GetEnemiesDefeated(),Kills+1);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDefeatBatchTest,"Rogue.Presentation.Defeat.BatchedMassKillsRemainBounded",RogueDefeatTests::Flags)
bool FRogueDefeatBatchTest::RunTest(const FString&)
{
    using namespace RogueDefeatTests;FFixture F;if(!F.Init())return false;
    FRogueDefeatVisualEvent V;V.EffectClass=F.Class;V.Location=FVector(0,0,300);
    for(int I=0;I<200;++I)F.W->GetGameState<ARogueGameState>()->QueueDefeatFeedback(V);
    F.Step(.5f);TestTrue(TEXT("Dense death snapshots cannot exceed effect actor budget"),F.Pool->GetPoolSize()<=24);
    F.Step(1.4f);TestEqual(TEXT("Saturated burst finishes without lingering visuals"),F.Pool->GetActiveCount(),0);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDefeatToastTest,"Rogue.Presentation.Defeat.ToastAccumulatesWithoutFlashingAndFades",RogueDefeatTests::Flags)
bool FRogueDefeatToastTest::RunTest(const FString&)
{
    auto* Class=LoadClass<URogueKillFeedbackWidgetBase>(nullptr,TEXT("/Game/UMG/HUD/WBP_KillFeedback.WBP_KillFeedback_C"));
    if(!TestNotNull(TEXT("Editable kill confirmation WBP exists"),Class))return false;
    auto* Widget=NewObject<URogueKillFeedbackWidgetBase>(GetTransientPackage(),Class);Widget->SetRenderOpacity(0);
    Widget->RegisterKills(5,false);FRogueKillFeedbackTestAccess::Step(Widget,.15f);
    TestEqual(TEXT("Batched kills become one legible count"),Widget->GetStreakCount(),5);
    const float Visible=Widget->GetRenderOpacity();TestTrue(TEXT("Toast fades in"),Visible>.9f);
    Widget->RegisterKills(1,false);FRogueKillFeedbackTestAccess::Step(Widget,.01f);
    TestTrue(TEXT("Another kill does not reset visible opacity"),Widget->GetRenderOpacity()>=Visible-.01f);
    TestEqual(TEXT("Another kill increments streak"),Widget->GetStreakCount(),6);
    Widget->RegisterKills(0,true);TestEqual(TEXT("Zero-count events are ignored"),Widget->GetStreakCount(),6);
    FRogueKillFeedbackTestAccess::Step(Widget,2.f);TestEqual(TEXT("Feedback leaves no lingering paint"),Widget->GetRenderOpacity(),0.f);
    return true;
}
#endif
