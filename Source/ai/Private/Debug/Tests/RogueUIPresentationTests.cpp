#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Widgets/RogueBarAnimation.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "UI/Widgets/RogueBossHealthBarWidgetBase.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Enemies/RogueEnemy.h"
#include "Components/ProgressBar.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

struct FRogueUIPresentationTestAccess
{
    static void Step(URogueVitalsWidgetBase* Widget,float Delta) { Widget->NativeTick(FGeometry(),Delta); }
    static float Health(URogueVitalsWidgetBase* Widget) { return Widget->HealthBar->GetPercent(); }
    static float Trail(URogueVitalsWidgetBase* Widget) { return Widget->HealthDamageBar->GetPercent(); }
    static float Armor(URogueVitalsWidgetBase* Widget) { return Widget->ArmorBar->GetPercent(); }
};
namespace RogueUIPresentationTests
{
    constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestWorldWrapper TestWorld;
        UWorld* World=nullptr;
        bool Initialize()
        {
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
            World=TestWorld.GetTestWorld(); World->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
            if (!TestWorld.BeginPlayInTestWorld()) { return false; }
            World->GetAuthGameMode()->SetActorTickEnabled(false); return true;
        }
        ARogueEnemy* Enemy(bool bBoss)
        {
            ARogueEnemy* E=World->SpawnActor<ARogueEnemy>();
            E->ActivatePooledEnemy(nullptr,FVector(10000,10000,100),FRotator::ZeroRotator);
            FRogueEnemyProfile Profile; Profile.Health=500;Profile.Speed=0;Profile.Damage=0;Profile.bIsBoss=bBoss;
            E->InitializeEnemy(Profile);E->SetActorTickEnabled(false);
            World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);return E;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBarDamageAnimationTest,"Rogue.UI.Presentation.DamageHealingAndFrameRate",RogueUIPresentationTests::Flags)
bool FRogueBarDamageAnimationTest::RunTest(const FString& Parameters)
{
    FRogueBarAnimation A; A.Snap(1); A.SetTarget(.3f,.2f); A.Step(.1f,14,4);
    TestTrue(TEXT("Primary bar follows damage smoothly"),A.Displayed>.3f&&A.Displayed<1);
    TestEqual(TEXT("Damage trail holds briefly"),A.Trail,1.f);
    A.SetTarget(.1f,.2f);A.Step(.1f,14,4);TestEqual(TEXT("Repeated hits restart trail hold"),A.Trail,1.f);
    for(int32 I=0;I<180;++I) { A.Step(1.f/60,14,4); }
    TestEqual(TEXT("Primary reaches the real value"),A.Displayed,.1f);TestEqual(TEXT("Trail eventually clears"),A.Trail,.1f);
    A.SetTarget(.8f,.2f);A.Step(.1f,14,4);
    TestTrue(TEXT("Healing also eases"),A.Displayed>.1f&&A.Displayed<.8f);
    TestEqual(TEXT("Healing does not create a damage ghost"),A.Trail,A.Displayed);
    FRogueBarAnimation B,C;B.Snap(1);C.Snap(1);B.SetTarget(.2f,.2f);C.SetTarget(.2f,.2f);
    for(int32 I=0;I<30;++I) B.Step(1.f/30,14,4);
    for(int32 I=0;I<144;++I) C.Step(1.f/144,14,4);
    TestTrue(TEXT("Primary animation is frame-rate independent"),FMath::IsNearlyEqual(B.Displayed,C.Displayed,.001f));
    TestTrue(TEXT("Trail animation is frame-rate independent"),FMath::IsNearlyEqual(B.Trail,C.Trail,.001f));
    A.Snap(2);TestEqual(TEXT("Overflow ratio clamps"),A.Displayed,1.f);A.Snap(-1);TestEqual(TEXT("Negative ratio clamps"),A.Displayed,0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueActualVitalsAnimationTest,"Rogue.UI.Presentation.ActualVitalsBindingsAndLifeReset",RogueUIPresentationTests::Flags)
bool FRogueActualVitalsAnimationTest::RunTest(const FString& Parameters)
{
    UClass* Class=LoadClass<URogueVitalsWidgetBase>(nullptr,TEXT("/Game/UMG/HUD/WBP_PlayerVitals.WBP_PlayerVitals_C"));
    if(!TestNotNull(TEXT("Actual WBP class loads"),Class))return false;
    URogueVitalsWidgetBase* Widget=NewObject<URogueVitalsWidgetBase>(GetTransientPackage(),Class);
    if(!TestTrue(TEXT("Actual WBP initializes"),Widget->Initialize()))return false;
    for(const TCHAR* Name:{TEXT("HealthBar"),TEXT("HealthDamageBar"),TEXT("ArmorBar"),TEXT("ArmorDamageBar")})
    {if(!TestNotNull(Name,Widget->GetWidgetFromName(FName(Name))))return false;}
    FRoguePlayerVitalsView View;View.bVisible=true;View.bHasArmor=true;View.MaxHealth=100;View.CurrentHealth=100;View.HealthPercent=1;View.MaxArmor=50;View.CurrentArmor=50;View.ArmorPercent=1;
    Widget->UpdateVitals(View);TestEqual(TEXT("Initial view snaps to real health"),FRogueUIPresentationTestAccess::Health(Widget),1.f);
    View.CurrentHealth=30;View.HealthPercent=.3f;View.CurrentArmor=0;View.ArmorPercent=0;
    Widget->UpdateVitals(View);FRogueUIPresentationTestAccess::Step(Widget,.1f);
    TestTrue(TEXT("Actual bound health bar interpolates"),FRogueUIPresentationTestAccess::Health(Widget)>.3f&&FRogueUIPresentationTestAccess::Health(Widget)<1);
    TestEqual(TEXT("Actual bound trail remains visible"),FRogueUIPresentationTestAccess::Trail(Widget),1.f);
    TestTrue(TEXT("Armor depletion interpolates"),FRogueUIPresentationTestAccess::Armor(Widget)>0&&FRogueUIPresentationTestAccess::Armor(Widget)<1);
    View.bDowned=true;View.CurrentHealth=0;View.HealthPercent=0;Widget->UpdateVitals(View);
    TestEqual(TEXT("Downed view immediately confirms zero health"),FRogueUIPresentationTestAccess::Health(Widget),0.f);
    View.bDowned=false;View.CurrentHealth=50;View.HealthPercent=.5f;Widget->UpdateVitals(View);
    TestEqual(TEXT("Revival resets display without a stale trail"),FRogueUIPresentationTestAccess::Health(Widget),.5f);
    View.MaxHealth=200;View.HealthPercent=.25f;Widget->UpdateVitals(View);
    TestEqual(TEXT("Changed health capacity resets ratio"),FRogueUIPresentationTestAccess::Health(Widget),.25f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBossHealthSnapshotTest,"Rogue.UI.Presentation.BossLifecyclePoolReuseAndGameOver",RogueUIPresentationTests::Flags)
bool FRogueBossHealthSnapshotTest::RunTest(const FString& Parameters)
{
    RogueUIPresentationTests::FFixture F;if(!F.Initialize())return false;
    ARogueGameState* State=F.World->GetGameState<ARogueGameState>();
    F.Enemy(false);State->RefreshBossHealthView();TestFalse(TEXT("Ordinary enemies do not open boss UI"),State->GetBossHealthView().bVisible);
    ARogueEnemy* Boss=F.Enemy(true);State->RefreshBossHealthView();
    TestTrue(TEXT("Boss outside camera range still has global UI"),State->GetBossHealthView().bVisible);
    TestEqual(TEXT("Initial health correct"),State->GetBossHealthView().CurrentHealth,500.f);
    const FGuid First=State->GetBossHealthView().SpawnId;
    Boss->GetAbilitySystemComponent()->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(),125);
    State->RefreshBossHealthView();TestEqual(TEXT("GAS damage updates the snapshot"),State->GetBossHealthView().GetPercent(),.25f);
    ARogueEnemy* Second=F.Enemy(true);State->RefreshBossHealthView();TestEqual(TEXT("Stable target with multiple bosses"),State->GetBossHealthView().SpawnId,First);
    Boss->DeactivateToPool();State->RefreshBossHealthView();TestEqual(TEXT("Next boss replaces a defeated target"),State->GetBossHealthView().SpawnId,Second->GetSpawnCycleId());
    Second->DeactivateToPool();State->RefreshBossHealthView();TestFalse(TEXT("Last boss leaving hides global UI"),State->GetBossHealthView().bVisible);
    Boss->ActivatePooledEnemy(nullptr,FVector(10000,0,100),FRotator::ZeroRotator);
    FRogueEnemyProfile Profile;Profile.Health=1000;Profile.bIsBoss=true;Boss->InitializeEnemy(Profile);
    F.World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(Boss);State->RefreshBossHealthView();
    TestTrue(TEXT("Pool reuse gets a new boss identity"),State->GetBossHealthView().SpawnId!=First);
    TestEqual(TEXT("Pool reuse gets fresh maximum health"),State->GetBossHealthView().MaxHealth,1000.f);
    State->SetRunStateSnapshot(1,0,0,true,true);State->RefreshBossHealthView();TestFalse(TEXT("Game over hides boss UI even if actor remains"),State->GetBossHealthView().bVisible);
    return true;
}

static FAutoConsoleCommandWithWorld BossBarRegression(TEXT("Rogue.Test.BossBar"),TEXT("Development-only two-player UI regression: boss spawn, damage and death, plus player health/shield damage."),
FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
{
    if(!World||!World->GetAuthGameMode<ARogueGameMode>())return;
    World->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);
    for(TActorIterator<ARogueEnemy> It(World);It;++It){if(!It->IsAvailableInPool())It->DeactivateToPool();}
    TWeakObjectPtr<UGameInstance> GI=World->GetGameInstance();
    TSharedRef<TWeakObjectPtr<ARogueEnemy>> Boss=MakeShared<TWeakObjectPtr<ARogueEnemy>>();
    TSharedRef<int32> Phase=MakeShared<int32>(0);TSharedRef<double> Started=MakeShared<double>(0);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([GI,Boss,Phase,Started](float)
    {
        UWorld* W=GI.IsValid()?GI->GetWorld():nullptr;if(!W)return false;
        TArray<ARogueCharacter*> Players;
        for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
        {
            ARogueCharacter* P=It->Get()?Cast<ARogueCharacter>(It->Get()->GetPawn()):nullptr;
            ARoguePlayerState* S=P?P->GetPlayerState<ARoguePlayerState>():nullptr;
            if(S&&S->IsBaseAttributesInitialized())Players.Add(P);
        }
        if(*Phase==0)
        {
            if(Players.Num()!=2)return true;
            for(ARogueCharacter* P:Players)
            {
                UAbilitySystemComponent* ASC=P->GetAbilitySystemComponent();
                ASC->SetNumericAttributeBase(URogueAttributeSet::GetMaxHealthAttribute(),100);ASC->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(),100);
                ASC->SetNumericAttributeBase(URogueAttributeSet::GetMaxArmorAttribute(),60);ASC->SetNumericAttributeBase(URogueAttributeSet::GetArmorAttribute(),60);
            }
            ARogueEnemy* E=W->SpawnActor<ARogueEnemy>();E->ActivatePooledEnemy(nullptr,FVector(20000,20000,200),FRotator::ZeroRotator);
            FRogueEnemyProfile Profile;Profile.Health=500;Profile.bIsBoss=true;Profile.Speed=0;Profile.Damage=0;E->InitializeEnemy(Profile);E->SetActorTickEnabled(false);
            W->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);*Boss=E;*Started=W->GetTimeSeconds();*Phase=1;
            UE_LOG(LogTemp,Display,TEXT("[BossBarTest] Spawned off-camera Boss 500/500 for two players."));
        }
        const double Elapsed=W->GetTimeSeconds()-*Started;
        if(*Phase==1&&Elapsed>=4&&Boss->IsValid())
        {
            RogueAbilitySystem::ApplyDamageToTarget(Players.Num()?Players[0]:nullptr,Boss->Get(),250,FGameplayTag());
            for(ARogueCharacter* P:Players){RogueAbilitySystem::ApplyDamageToTarget(Boss->Get(),P,80,FGameplayTag());}
            *Phase=2;UE_LOG(LogTemp,Display,TEXT("[BossBarTest] Boss damaged to 250/500; each player's shield depleted and health damaged."));
        }
        if(*Phase==2&&Elapsed>=10&&Boss->IsValid())
        {
            RogueAbilitySystem::ApplyDamageToTarget(Players.Num()?Players[0]:nullptr,Boss->Get(),10000,FGameplayTag());
            *Phase=3;
        }
        if(*Phase==3&&Elapsed>=12)
        {
            const bool bHidden=!W->GetGameState<ARogueGameState>()->GetBossHealthView().bVisible;
            UE_LOG(LogTemp,Display,TEXT("[BossBarTest] %s Boss death cleared global UI."),bHidden?TEXT("PASS"):TEXT("FAIL"));return false;
        }
        return true;
    }),.05f);
}));
#endif
