#if WITH_DEV_AUTOMATION_TESTS
#include "RogueTestWorld.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Components/TextBlock.h"
#include "Framework/RogueGameMode.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

namespace RogueManualUpgradeTests
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
struct FFixture
{
    FTestWorldWrapper Test;UWorld* World=nullptr;ARogueGameMode* Mode=nullptr;
    TArray<ARogueCharacter*> Players;TArray<ARoguePlayerState*> States;
    bool Init(int32 Count=2)
    {
        if(!RogueTestWorld::Begin(Test,World))return false;
        Mode=World->GetAuthGameMode<ARogueGameMode>();
        for(int32 I=0;I<Count;++I)
        {
            auto* PC=RogueTestWorld::SpawnFreshController(World);
            auto* P=World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),FTransform(FVector(0,I*3000,300)),
                nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if(!P)return false;
            P->AutoPossessPlayer=EAutoReceiveInput::Disabled;P->FinishSpawning(P->GetActorTransform());PC->Possess(P);
            P->SetActorTickEnabled(false);P->SetActorEnableCollision(false);
            P->GetCharacterMovement()->SetComponentTickEnabled(false);P->GetAbilitySystemComponent()->CancelAllAbilities();
            Players.Add(P);States.Add(PC->GetPlayerState<ARoguePlayerState>());
        }
        return true;
    }
    void Grant(int32 Count,int32 Player=0){for(int32 I=0;I<Count;++I)Mode->StartUpgradeSelection(Players[Player]);}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualUpgradeBankTest,"Rogue.Gameplay.ManualUpgrade.LevelUpsBankWithoutMenusOrInvulnerability",RogueManualUpgradeTests::Flags)
bool FManualUpgradeBankTest::RunTest(const FString&)
{
    using namespace RogueManualUpgradeTests;FFixture F;if(!F.Init())return false;
    auto* P=F.Players[0];auto* S=F.States[0];const int32 Level=P->GetPlayerLevel();
    for(int32 I=0;I<3;++I)P->AddExperience(P->GetExperienceToNextLevel());
    TestEqual(TEXT("XP still immediately increases level"),P->GetPlayerLevel(),Level+3);
    TestEqual(TEXT("Three earned upgrades remain available"),S->GetPendingUpgradeCount(),3);
    TestFalse(TEXT("Level-up does not force an open selection"),S->IsAwaitingUpgradeChoice());
    TestTrue(TEXT("Cards are generated only after an explicit request"),S->GetPendingUpgrades().IsEmpty());
    TestEqual(TEXT("The teammate has an independent bank"),F.States[1]->GetPendingUpgradeCount(),0);
    auto* Source=F.World->SpawnActor<AActor>();
    TestTrue(TEXT("Banking rewards does not protect the player from combat damage"),RogueAbilitySystem::ApplyDamageToTarget(Source,P,1,RogueGameplayTags::Damage_Source_Contact));
    S->Server_ToggleUpgradeSelection();
    TestTrue(TEXT("Explicit request opens actual server-generated cards"),S->IsAwaitingUpgradeChoice());
    TestEqual(TEXT("Opening does not spend a reward"),S->GetPendingUpgradeCount(),3);
    TestTrue(TEXT("An actively open selection remains vulnerable"),RogueAbilitySystem::ApplyDamageToTarget(Source,P,1,RogueGameplayTags::Damage_Source_Projectile));
    for(int32 I=3;I>0;--I)
    {
        const auto Options=S->GetPendingUpgrades();if(Options.IsEmpty())return false;
        const FGuid Receipt=Options[0].SelectionId;
        S->Server_RequestUpgradeSelection(0,Receipt);
        TestEqual(TEXT("Each actual pick spends exactly one reward"),S->GetPendingUpgradeCount(),I-1);
        S->Server_RequestUpgradeSelection(0,Receipt);
        TestEqual(TEXT("Repeated receipt cannot spend an unseen round"),S->GetPendingUpgradeCount(),I-1);
    }
    TestFalse(TEXT("The final pick closes the menu"),S->IsAwaitingUpgradeChoice());
    TestTrue(TEXT("Damage still applies after the last menu closes"),RogueAbilitySystem::ApplyDamageToTarget(Source,P,1,RogueGameplayTags::Damage_Source_Projectile));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualUpgradeDeferTest,"Rogue.Gameplay.ManualUpgrade.CloseAndReopenPreservesChoicesAndRejectsOldClicks",RogueManualUpgradeTests::Flags)
bool FManualUpgradeDeferTest::RunTest(const FString&)
{
    using namespace RogueManualUpgradeTests;FFixture F;if(!F.Init())return false;
    auto* S=F.States[0];auto* P=F.Players[0];F.Grant(2);
    S->Server_ToggleUpgradeSelection();const auto Options=S->GetPendingUpgrades();if(Options.IsEmpty())return false;
    S->Server_ToggleUpgradeSelection();
    TestFalse(TEXT("The second request closes selection"),S->IsAwaitingUpgradeChoice());
    TestEqual(TEXT("Closing preserves every unspent reward"),S->GetPendingUpgradeCount(),2);
    S->Server_RequestUpgradeSelection(0,Options[0].SelectionId);
    TestEqual(TEXT("Hidden choices cannot be selected"),S->GetPendingUpgradeCount(),2);
    auto* Source=F.World->SpawnActor<AActor>();
    TestTrue(TEXT("Deferred choices remain vulnerable"),RogueAbilitySystem::ApplyDamageToTarget(Source,P,1,RogueGameplayTags::Damage_Source_Area));
    S->Server_ToggleUpgradeSelection();const auto Reopened=S->GetPendingUpgrades();
    TestEqual(TEXT("Reopening does not reroll the number of cards"),Reopened.Num(),Options.Num());
    for(int32 I=0;I<Options.Num();++I)
    {
        TestTrue(TEXT("Reopening retains the same upgrade types"),Reopened[I].Type==Options[I].Type);
        TestEqual(TEXT("Reopening retains saved magnitudes"),Reopened[I].Magnitude,Options[I].Magnitude);
    }
    TestTrue(TEXT("Reopening replaces the receipt, not the cards"),Reopened[0].SelectionId!=Options[0].SelectionId);
    S->Server_RequestUpgradeSelection(0,Options[0].SelectionId);
    TestEqual(TEXT("An old click cannot spend a reopened choice"),S->GetPendingUpgradeCount(),2);
    S->Server_RequestUpgradeSelection(0,Reopened[0].SelectionId);
    return TestEqual(TEXT("The fresh displayed receipt can be selected"),S->GetPendingUpgradeCount(),1);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualUpgradeMenuGuardTest,"Rogue.Gameplay.ManualUpgrade.OtherMenusAndDeadPlayersCannotOpenChoices",RogueManualUpgradeTests::Flags)
bool FManualUpgradeMenuGuardTest::RunTest(const FString&)
{
    using namespace RogueManualUpgradeTests;FFixture F;if(!F.Init())return false;
    auto* S=F.States[0];auto* P=F.Players[0];F.Grant(1);
    S->Server_SetPauseMenuOpen(true);S->Server_ToggleUpgradeSelection();
    TestFalse(TEXT("Pause/settings does not stack a selection"),S->IsAwaitingUpgradeChoice());
    S->Server_SetPauseMenuOpen(false);F.Mode->TryOpenShop(P);S->Server_ToggleUpgradeSelection();
    TestTrue(TEXT("Shop opens independently of banked choices"),S->IsShopOpen());
    TestFalse(TEXT("Shop prevents overlapping selection menus"),S->IsAwaitingUpgradeChoice());
    F.Mode->CloseShop(P);S->Server_ToggleUpgradeSelection();F.Mode->TryOpenShop(P);
    TestTrue(TEXT("Closing shop allows the requested selection"),S->IsAwaitingUpgradeChoice());
    TestFalse(TEXT("An active selection blocks a new shop"),S->IsShopOpen());
    auto* Source=F.World->SpawnActor<AActor>();
    TestTrue(TEXT("Lethal damage can hit during selection"),RogueAbilitySystem::ApplyDamageToTarget(Source,P,1000000,RogueGameplayTags::Damage_Source_Area));
    F.Test.TickTestWorld(.03f);S->Server_ToggleUpgradeSelection();
    TestFalse(TEXT("A downed player cannot open a selection"),S->IsAwaitingUpgradeChoice());
    TestEqual(TEXT("Going down with a living teammate preserves banked reward"),S->GetPendingUpgradeCount(),1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualUpgradeStageTest,"Rogue.Gameplay.ManualUpgrade.StageTravelPreservesBankButDoesNotForceReopen",RogueManualUpgradeTests::Flags)
bool FManualUpgradeStageTest::RunTest(const FString&)
{
    using namespace RogueManualUpgradeTests;FFixture F;if(!F.Init())return false;
    auto* S=F.States[0];F.Grant(3);S->Server_ToggleUpgradeSelection();
    if(!TestTrue(TEXT("Original menu opens"),S->IsAwaitingUpgradeChoice()))return false;
    const auto Cards=S->GetPendingUpgrades();S->PrepareStageTravel();
    auto* PC=RogueTestWorld::SpawnFreshController(F.World);auto* New=PC->GetPlayerState<ARoguePlayerState>();
    S->CopyProperties(New);
    TestEqual(TEXT("Both unopened and deferred current choices survive travel"),New->GetPendingUpgradeCount(),3);
    TestEqual(TEXT("Deferred cards survive travel"),New->GetPendingUpgrades().Num(),Cards.Num());
    TestFalse(TEXT("Arrival does not automatically reopen selection"),New->IsAwaitingUpgradeChoice());
    New->ResetUpgradeState();
    return TestEqual(TEXT("A new run clears every pending reward"),New->GetPendingUpgradeCount(),0);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualUpgradeCounterTest,"Rogue.UI.ManualUpgrade.CountOnlyChangesRefreshTheSingleHint",RogueManualUpgradeTests::Flags)
bool FManualUpgradeCounterTest::RunTest(const FString&)
{
    UClass* Class=LoadClass<URogueVitalsWidgetBase>(nullptr,TEXT("/Game/UMG/HUD/WBP_PlayerVitals.WBP_PlayerVitals_C"));
    if(!Class)return false;
    auto* Widget=NewObject<URogueVitalsWidgetBase>(GetTransientPackage(),Class);if(!Widget->Initialize())return false;
    auto* Hint=Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("PendingUpgradeHintText")));
    if(!TestNotNull(TEXT("The saved HUD retains the single upgrade reminder"),Hint))return false;
    TestNull(TEXT("The level badge no longer duplicates the upgrade count"),Widget->GetWidgetFromName(TEXT("PendingUpgradesText")));
    TestNull(TEXT("The redundant Q badge is removed"),Widget->GetWidgetFromName(TEXT("UpgradeKeyBadge")));
    FRoguePlayerVitalsView View;View.bVisible=true;View.Level=18;View.CurrentHealth=View.MaxHealth=100;View.HealthPercent=1;
    Widget->UpdateVitals(View);
    TestEqual(TEXT("No unused upgrades hides the reminder"),Hint->GetVisibility(),ESlateVisibility::Collapsed);
    View.PendingUpgradeSelections=3;Widget->UpdateVitals(View);
    TestTrue(TEXT("A count-only change updates the single reminder"),Hint->GetText().ToString().Contains(TEXT("3"))&&Hint->GetText().ToString().Contains(TEXT("Q")));
    TestEqual(TEXT("Unused upgrades show the reminder"),Hint->GetVisibility(),ESlateVisibility::HitTestInvisible);
    View.PendingUpgradeSelections=0;Widget->UpdateVitals(View);
    TestEqual(TEXT("Spending the final upgrade removes the reminder"),Hint->GetVisibility(),ESlateVisibility::Collapsed);
    return true;
}
#endif
