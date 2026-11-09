#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Widgets/RogueHUDViewData.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "UI/Widgets/RogueDashRingWidget.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/Abilities/RogueGameplayAbility_Dash.h"
#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace RogueDashHUDTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDashHUDGASTest, "Rogue.UI.DashHUD.RealGASCooldownAndReset", RogueDashHUDTests::Flags)
bool FRogueDashHUDGASTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper TestWorld;
    if (!TestWorld.CreateTestWorld(EWorldType::Game)) return false;
    UWorld* World = TestWorld.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
    if (!TestWorld.BeginPlayInTestWorld()) return false;
    World->GetAuthGameMode()->SetActorTickEnabled(false);
    World->GetSubsystem<URogueSpawnSubsystem>()->SetSpawningSuspended(true);
    ARoguePlayerController* PC = World->SpawnActor<ARoguePlayerController>();
    const FTransform Transform(FVector(0,0,300));
    ARogueCharacter* Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
    Player->FinishSpawning(Transform); PC->Possess(Player);
    Player->SetActorTickEnabled(false); Player->GetCharacterMovement()->SetComponentTickEnabled(false);
    Player->SetActorEnableCollision(false);
    URogueAbilitySystemComponent* ASC = Player->GetRogueAbilitySystemComponent();
    if (!TestNotNull(TEXT("Possessed player's ASC"), ASC)) return false;
    ASC->CancelAllAbilities();
    if (!ASC->FindAbilitySpecFromClass(URogueGameplayAbility_Dash::StaticClass()))
        ASC->GiveAbility(FGameplayAbilitySpec(URogueGameplayAbility_Dash::StaticClass()));
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetMaxHealthAttribute(),100);
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(),100);
    FRoguePlayerVitalsView View;
    RogueVitalsView::UpdateDashState(Player,View);
    TestTrue(TEXT("Initial dash is ready"), View.bDashReady);
    TestEqual(TEXT("Ready is full"), View.DashCooldownPercent,1.f);
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetDashCooldownDurationAttribute(),4.f);
    const FActiveGameplayEffectHandle First = ASC->ApplyGameplayEffectToSelf(GetDefault<URogueDashCooldownEffect>(),1,ASC->MakeEffectContext());
    RogueVitalsView::UpdateDashState(Player,View);
    TestFalse(TEXT("An actual cooldown blocks readiness"),View.bDashReady);
    TestTrue(TEXT("Actual GE duration is exposed"), FMath::IsNearlyEqual(View.DashCooldownDuration,4.f,.01f));
    TestTrue(TEXT("Cooldown starts empty"), View.DashCooldownPercent<.01f);
    for (int32 I=0;I<50;++I) TestWorld.TickTestWorld(.02f);
    RogueVitalsView::UpdateDashState(Player,View);
    TestTrue(TEXT("Cooldown recovers with GAS time"),View.DashCooldownPercent>.2f&&View.DashCooldownPercent<.3f);
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetDashCooldownDurationAttribute(),8.f);
    RogueVitalsView::UpdateDashState(Player,View);
    TestTrue(TEXT("Changing next-use duration does not resize the active bar"),FMath::IsNearlyEqual(View.DashCooldownDuration,4.f,.01f));
    const FActiveGameplayEffectHandle Second = ASC->ApplyGameplayEffectToSelf(GetDefault<URogueDashCooldownEffect>(),1,ASC->MakeEffectContext());
    RogueVitalsView::UpdateDashState(Player,View);
    TestTrue(TEXT("Overlapping cooldowns use the last expiry"),FMath::IsNearlyEqual(View.DashCooldownDuration,8.f,.01f));
    ASC->RemoveActiveGameplayEffect(Second);
    RogueVitalsView::UpdateDashState(Player,View);
    TestFalse(TEXT("Remaining earlier cooldown still blocks"),View.bDashReady);
    ASC->RemoveActiveGameplayEffect(First);
    RogueVitalsView::UpdateDashState(Player,View);
    TestTrue(TEXT("Cleared cooldown is ready immediately"),View.bDashReady);
    const FGameplayAbilitySpecHandle Dash = ASC->FindAbilitySpecFromClass(URogueGameplayAbility_Dash::StaticClass())->Handle;
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetDashSpeedAttribute(), 900.f);
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetDashDurationAttribute(), .22f);
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetMaxArmorAttribute(), 10.f);
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetArmorAttribute(), 5.f);
    if (!TestTrue(TEXT("The actual dash activates"), ASC->TryActivateAbility(Dash))) { return false; }
    RogueVitalsView::UpdateDashState(Player,View);
    TestTrue(TEXT("Active dash is identified"),View.bDashing);
    TestFalse(TEXT("Active dash is not ready"),View.bDashReady);
    auto* Attacker = World->SpawnActor<AActor>();
    TestFalse(TEXT("A dash blocks incoming damage"), RogueAbilitySystem::ApplyDamageToTarget(
        Attacker, Player, 10.f, RogueGameplayTags::Damage_Source_Projectile));
    TestEqual(TEXT("A protected dash preserves health"), Player->GetRogueAttributeSet()->GetHealth(), 100.f);
    TestEqual(TEXT("A protected dash preserves armor"), Player->GetRogueAttributeSet()->GetArmor(), 5.f);
    for (int32 Frame = 0; Frame < 18; ++Frame) { TestWorld.TickTestWorld(.02f); }
    TestFalse(TEXT("The dash state ends before the cooldown"), ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dashing));
    TestTrue(TEXT("The cooldown is still active"), ASC->HasMatchingGameplayTag(RogueGameplayTags::Cooldown_Dash));
    TestTrue(TEXT("Damage resumes during cooldown"), RogueAbilitySystem::ApplyDamageToTarget(
        Attacker, Player, 1.f, RogueGameplayTags::Damage_Source_Contact));

    FGameplayTagContainer DashCooldown(RogueGameplayTags::Cooldown_Dash);
    ASC->RemoveActiveEffectsWithGrantedTags(DashCooldown);
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetDashDurationAttribute(), 0.f);
    ASC->TryActivateAbility(Dash);
    TestFalse(TEXT("An invalid dash leaves no protection behind"), ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dashing));
    ASC->AddLooseGameplayTag(RogueGameplayTags::Cooldown_Dash);
    RogueVitalsView::UpdateDashState(Player,View);
    TestFalse(TEXT("A blocking cooldown tag without a finite timer is not ready"),View.bDashReady);
    ASC->RemoveLooseGameplayTag(RogueGameplayTags::Cooldown_Dash);
    RogueVitalsView::UpdateDashState(nullptr,View);
    TestFalse(TEXT("Losing the pawn clears stale readiness"),View.bDashReady);
    TestEqual(TEXT("Losing the pawn clears stale fill"),View.DashCooldownPercent,0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDashHUDWidgetTest, "Rogue.UI.DashHUD.ActualWBPBindingsAndStates", RogueDashHUDTests::Flags)
bool FRogueDashHUDWidgetTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<URogueVitalsWidgetBase>(nullptr,TEXT("/Game/UMG/HUD/WBP_PlayerVitals.WBP_PlayerVitals_C"));
    if (!TestNotNull(TEXT("Saved vitals WBP"),Class)) return false;
    URogueVitalsWidgetBase* Widget = NewObject<URogueVitalsWidgetBase>(GetTransientPackage(),Class);
    if (!Widget->Initialize()) return false;
    UProgressBar* Bar = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("DashCooldownBar")));
    UTextBlock* Text = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("DashCooldownText")));
    UWidget* Row = Widget->GetWidgetFromName(TEXT("DashCooldownRow"));
    auto* Ring = Cast<URogueDashRingWidget>(Widget->GetWidgetFromName(TEXT("PlayerDashRing")));
    if (!TestNotNull(TEXT("Player-side dash ring saved in designer"), Ring)) return false;
    if (!TestNotNull(TEXT("Dash bar saved in designer"),Bar) || !TestNotNull(TEXT("Dash text bound"),Text)
        || !TestNotNull(TEXT("Dash row saved"),Row)) return false;
    FRoguePlayerVitalsView View; View.bVisible=true;View.bDashUsable=true;View.bDashReady=true;View.DashCooldownPercent=1;
    View.CurrentHealth=View.MaxHealth=100;View.HealthPercent=1;
    Ring->SetVisibility(ESlateVisibility::HitTestInvisible);
    Widget->UpdateVitals(View);
    TestEqual(TEXT("A full ready ring is hidden immediately"),Ring->GetVisibility(),ESlateVisibility::Collapsed);
    TestEqual(TEXT("Ready is full"),Bar->GetPercent(),1.f);
    TestEqual(TEXT("Ready label"),Text->GetText().ToString(),FString(TEXT("冲刺就绪")));
    View.bDashReady=false;View.DashCooldownDuration=4;View.DashCooldownRemaining=2;View.DashCooldownPercent=.5f;
    Widget->UpdateVitals(View);
    TestEqual(TEXT("Halfway cooldown"),Bar->GetPercent(),.5f);
    TestEqual(TEXT("The player-side ring receives the real cooldown percentage"), Ring->GetCooldownPercent(), .5f);
    TestEqual(TEXT("Cooldown seconds"),Text->GetText().ToString(),FString(TEXT("冷却 2.0 秒")));
    View.DashCooldownRemaining=.01f;View.DashCooldownPercent=.9975f;Widget->UpdateVitals(View);
    TestEqual(TEXT("Never show zero seconds before ready"),Text->GetText().ToString(),FString(TEXT("冷却 0.1 秒")));
    View.bDowned=true;Widget->UpdateVitals(View);
    TestEqual(TEXT("Downed disables dash fill"),Bar->GetPercent(),0.f);
    TestEqual(TEXT("Downed label"),Text->GetText().ToString(),FString(TEXT("冲刺不可用")));
    TestEqual(TEXT("Downed immediately hides the ring"), Ring->GetVisibility(), ESlateVisibility::Collapsed);
    TestTrue(TEXT("Unavailable is dimmed"),Row->GetRenderOpacity()<1.f);
    View.bDowned=false;View.bDashReady=true;View.DashCooldownRemaining=0;View.DashCooldownPercent=1;Widget->UpdateVitals(View);
    TestEqual(TEXT("Revival restores ready status"),Text->GetText().ToString(),FString(TEXT("冲刺就绪")));
    TestEqual(TEXT("Revival does not show a full ready ring"),Ring->GetVisibility(),ESlateVisibility::Collapsed);
    TestEqual(TEXT("The old dash block stays hidden when a ring is bound"),Row->GetVisibility(),ESlateVisibility::Collapsed);
    TestEqual(TEXT("No armor does not stop ring cooldown updates"), Ring->GetCooldownPercent(), 1.f);
    const UProgressBar* Health = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("HealthBar")));
    if (TestNotNull(TEXT("Existing health bar retained"),Health)) TestEqual(TEXT("Dash changes do not change health"),Health->GetPercent(),1.f);
    return true;
}
#endif
