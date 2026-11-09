#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanel.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

struct FRogueVitalsFeedbackTestAccess
{
    static void UseReadableTextPalette(URogueVitalsWidgetBase* W)
    {
        W->DashReadyTextColor=FLinearColor(.02f,.22f,.28f,1);
        W->DashStatusTextColor=FLinearColor(.09f,.14f,.19f,1);
        W->RestoreFeedbackTextColor=FLinearColor(.02f,.24f,.10f,1);
    }

    static void Step(URogueVitalsWidgetBase* W, float Delta) { W->NativeTick(FGeometry(), Delta); }
    static const FRogueVitalsFeedback& Health(URogueVitalsWidgetBase* W) { return W->HealthChange; }
    static const FRogueVitalsFeedback& Armor(URogueVitalsWidgetBase* W) { return W->ArmorChange; }
    static const FRogueVitalsFeedback& Experience(URogueVitalsWidgetBase* W) { return W->ExperienceChange; }
    static float WrapDuration(URogueVitalsWidgetBase* W) { return W->ExperienceWrapDuration; }
    static bool WrapPending(URogueVitalsWidgetBase* W) { return W->bExperienceWrapPending; }
    static bool BindingsValid(URogueVitalsWidgetBase* W)
    {
        return W->HealthRow && W->ArmorRow && W->ExperienceRow && W->HealthFeedback && W->ArmorFeedback
            && W->ExperienceFeedback && W->HealthChangeText && W->ArmorChangeText && W->ExperienceChangeText
            && W->HealthBar && W->HealthDamageBar && W->ArmorBar && W->ArmorDamageBar && W->ExperienceBar;
    }
};

namespace RogueVitalsFeedbackTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
    using FAccess = FRogueVitalsFeedbackTestAccess;

    template <typename T>
    T* Find(URogueVitalsWidgetBase* Widget, const TCHAR* Name)
    {
        return Cast<T>(Widget->GetWidgetFromName(FName(Name)));
    }

    URogueVitalsWidgetBase* Create(FAutomationTestBase& Test)
    {
        UClass* Class = LoadClass<URogueVitalsWidgetBase>(nullptr, TEXT("/Game/UMG/HUD/WBP_PlayerVitals.WBP_PlayerVitals_C"));
        if (!Test.TestNotNull(TEXT("Actual player vitals WBP loads"), Class)) return nullptr;
        auto* Widget = NewObject<URogueVitalsWidgetBase>(GetTransientPackage(), Class);
        if (!Test.TestTrue(TEXT("Actual WBP initializes and binds"), Widget->Initialize())) return nullptr;
        if (!Test.TestTrue(TEXT("All presentation controls bind to the actual WBP"), FAccess::BindingsValid(Widget))) return nullptr;
        return Widget;
    }

    FRoguePlayerVitalsView Baseline()
    {
        FRoguePlayerVitalsView View;
        View.bVisible = true; View.bHasArmor = true;
        View.CurrentHealth = View.MaxHealth = 100; View.HealthPercent = 1;
        View.CurrentArmor = View.MaxArmor = 50; View.ArmorPercent = 1;
        View.Level = 1; View.Experience = 10; View.ExperienceToNextLevel = 100; View.ExperiencePercent = .1f;
        return View;
    }

    void Settle(URogueVitalsWidgetBase* W, float Seconds = 2.f)
    {
        const int32 Steps = FMath::CeilToInt(Seconds * 60.f);
        for (int32 Index = 0; Index < Steps; ++Index) FAccess::Step(W, 1.f / 60.f);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueVitalsActualFeedbackTest,
    "Rogue.UI.VitalsFeedback.ActualWBPChangesAndStyleRecovery", RogueVitalsFeedbackTests::Flags)
bool FRogueVitalsActualFeedbackTest::RunTest(const FString& Parameters)
{
    using namespace RogueVitalsFeedbackTests;
    auto* W = Create(*this); if (!W) return false;
    FAccess::UseReadableTextPalette(W);
    auto* HealthRow = Find<USizeBox>(W, TEXT("HealthRow"));
    auto* ArmorRow = Find<USizeBox>(W, TEXT("ArmorRow"));
    auto* HealthFlash = Find<UBorder>(W, TEXT("HealthFeedback"));
    auto* HealthNumber = Find<UTextBlock>(W, TEXT("HealthChangeText"));
    auto* ArmorNumber = Find<UTextBlock>(W, TEXT("ArmorChangeText"));
    auto* HealthBar = Find<UProgressBar>(W, TEXT("HealthBar"));
    auto* HealthTrail = Find<UProgressBar>(W, TEXT("HealthDamageBar"));
    auto* HealthLabel = Find<UTextBlock>(W, TEXT("HealthText"));
    auto* Status = Find<UWidget>(W, TEXT("StatusPanel"));
    if (!TestNotNull(TEXT("Health row is an actual flow-layout SizeBox"), HealthRow)
        || !TestNotNull(TEXT("Armor row is an actual flow-layout SizeBox"), ArmorRow)
        || !TestNotNull(TEXT("Status panel remains in WBP"), Status)
        || !TestNotNull(TEXT("Health label remains bound"), HealthLabel)) return false;
    TestTrue(TEXT("Armor and health share the centered layout container"), ArmorRow->GetParent()==HealthRow->GetParent() && Cast<UCanvasPanel>(ArmorRow->GetParent())!=nullptr);
    TestTrue(TEXT("Health uses scale fill to preserve its moving rounded end"), HealthBar->GetBarFillStyle() == EProgressBarFillStyle::Scale);

    const FWidgetTransform AuthoredTransform(FVector2D(2, -1), FVector2D(.975f, 1.025f), FVector2D::ZeroVector, 1.f);
    {
        auto* ColorWidget=Create(*this);if(!ColorWidget)return false;FAccess::UseReadableTextPalette(ColorWidget);
        FRoguePlayerVitalsView ColorView;ColorView.bVisible=true;ColorView.bDashUsable=true;ColorView.bDashReady=true;
        ColorView.CurrentHealth=ColorView.MaxHealth=100;ColorView.HealthPercent=1;
        auto* DashText=Find<UTextBlock>(ColorWidget,TEXT("DashCooldownText"));
        if(!TestNotNull(TEXT("Dash status text retained"),DashText))return false;
        ColorWidget->UpdateVitals(ColorView);
        TestTrue(TEXT("Ready text uses its authored palette"),DashText->GetColorAndOpacity().GetSpecifiedColor()==FLinearColor(.02f,.22f,.28f,1));
        ColorView.bDashReady=false;ColorView.DashCooldownRemaining=2;ColorWidget->UpdateVitals(ColorView);
        TestTrue(TEXT("Cooldown text uses its authored palette"),DashText->GetColorAndOpacity().GetSpecifiedColor()==FLinearColor(.09f,.14f,.19f,1));
    }
    const FLinearColor AuthoredFlash(.48f, .4f, .32f, .7f);
    const FSlateColor AuthoredText(FLinearColor(.9f, .8f, .7f, 1.f));
    const FVector2D AuthoredNumberPosition(3, 2);
    HealthRow->SetRenderTransform(AuthoredTransform);
    HealthFlash->SetBrushColor(AuthoredFlash);
    HealthNumber->SetColorAndOpacity(AuthoredText);
    HealthNumber->SetRenderTranslation(AuthoredNumberPosition);
    const FWidgetTransform StatusTransform = Status->GetRenderTransform();
    FRoguePlayerVitalsView V = Baseline(); W->UpdateVitals(V);
    TestEqual(TEXT("First observation does not flash"), HealthFlash->GetRenderOpacity(), 0.f);
    TestEqual(TEXT("First observation does not show a delta"), HealthNumber->GetRenderOpacity(), 0.f);

    V.CurrentHealth = 80; V.HealthPercent = .8f; W->UpdateVitals(V); FAccess::Step(W, .08f);
    TestTrue(TEXT("A real health decrease triggers damage emphasis"), FAccess::Health(W).Kind == ERogueVitalsChange::Damage);
    TestEqual(TEXT("Damage number uses the real amount"), FAccess::Health(W).Amount, -20.f);
    TestTrue(TEXT("Primary health eases toward the real value"), HealthBar->GetPercent() > .8f && HealthBar->GetPercent() < 1.f);
    TestEqual(TEXT("Damage trail is still holding the old value"), HealthTrail->GetPercent(), 1.f);
    TestTrue(TEXT("Damage edge is visible briefly"), HealthFlash->GetRenderOpacity() > 0.f);
    TestTrue(TEXT("Change text rises without changing its layout slot"), HealthNumber->GetRenderTransform().Translation.Y < AuthoredNumberPosition.Y);
    TestTrue(TEXT("The row receives only a temporary transform"), HealthRow->GetRenderTransform() != AuthoredTransform);
    TestTrue(TEXT("Wave and wallet panel never receive the row emphasis"), Status->GetRenderTransform() == StatusTransform);
    TestEqual(TEXT("The separated health caption stays stable"), HealthLabel->GetText().ToString(), FString(TEXT("生命")));
    auto* HealthValue = Find<UTextBlock>(W, TEXT("HealthValueText"));
    if (TestNotNull(TEXT("The actual WBP has a separate health value"), HealthValue))
        TestEqual(TEXT("The health value updates before the animated bar settles"), HealthValue->GetText().ToString(), FString(TEXT("80 / 100")));
    V.CurrentHealth = 70; V.HealthPercent = .7f; W->UpdateVitals(V);
    TestEqual(TEXT("Continuous damage merges into one brief delta"), FAccess::Health(W).Amount, -30.f);
    TestEqual(TEXT("Presentation does not alter the supplied health"), W->GetCachedVitals().CurrentHealth, 70.f);
    Settle(W);
    TestEqual(TEXT("Settled health reaches the true ratio"), HealthBar->GetPercent(), .7f);
    TestEqual(TEXT("Damage edge finishes"), HealthFlash->GetRenderOpacity(), 0.f);
    TestEqual(TEXT("Damage number finishes"), HealthNumber->GetRenderOpacity(), 0.f);
    TestTrue(TEXT("Authored row transform is restored exactly"), HealthRow->GetRenderTransform() == AuthoredTransform);
    TestTrue(TEXT("Authored number translation is restored"), HealthNumber->GetRenderTransform().Translation == AuthoredNumberPosition);

    V.CurrentHealth = 90; V.HealthPercent = .9f; W->UpdateVitals(V); FAccess::Step(W, .08f);
    TestTrue(TEXT("Healing has its own response"), FAccess::Health(W).Kind == ERogueVitalsChange::Restore);
    TestEqual(TEXT("Healing delta is positive and accurate"), FAccess::Health(W).Amount, 20.f);
    TestTrue(TEXT("Healing emphasizes the row without instantly jumping the bar"), HealthBar->GetPercent() > .7f && HealthBar->GetPercent() < .9f);
    TestTrue(TEXT("Healing color differs temporarily from the WBP text color"), HealthNumber->GetColorAndOpacity().GetSpecifiedColor() != AuthoredText.GetSpecifiedColor());
    TestTrue(TEXT("Healing uses the configured readable text palette"),HealthNumber->GetColorAndOpacity().GetSpecifiedColor()==FLinearColor(.02f,.24f,.10f,1));
    V.CurrentHealth = 90.5f; V.HealthPercent = .905f; W->UpdateVitals(V);
    TestEqual(TEXT("Sub-threshold regeneration does not spam new numbers"), FAccess::Health(W).Amount, 20.f);
    Settle(W);
    TestTrue(TEXT("Healing restores the authored accent tint"), HealthFlash->GetBrushColor() == AuthoredFlash);
    TestTrue(TEXT("Healing restores the authored text color"), HealthNumber->GetColorAndOpacity().GetSpecifiedColor() == AuthoredText.GetSpecifiedColor());

    V.CurrentArmor = 0; V.ArmorPercent = 0; W->UpdateVitals(V);
    TestTrue(TEXT("Positive armor dropping to zero triggers armor break"), FAccess::Armor(W).Kind == ERogueVitalsChange::ArmorBreak);
    TestEqual(TEXT("Armor break has the intended label"), ArmorNumber->GetText().ToString(), FString(TEXT("护甲破裂")));
    TestTrue(TEXT("Depleted but unlocked armor keeps its row"), ArmorRow->GetVisibility() != ESlateVisibility::Collapsed);
    FAccess::Step(W, .1f); const float BreakAge = FAccess::Armor(W).Elapsed;
    ++V.Money; W->UpdateVitals(V);
    TestEqual(TEXT("Unrelated HUD data does not restart armor break"), FAccess::Armor(W).Elapsed, BreakAge);
    Settle(W);
    TestEqual(TEXT("Armor break number finishes"), ArmorNumber->GetRenderOpacity(), 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueVitalsFeedbackResetTest,
    "Rogue.UI.VitalsFeedback.IdentityLifeAndArmorLayout", RogueVitalsFeedbackTests::Flags)
bool FRogueVitalsFeedbackResetTest::RunTest(const FString& Parameters)
{
    using namespace RogueVitalsFeedbackTests;
    auto* W = Create(*this); if (!W) return false;
    auto* HealthBar = Find<UProgressBar>(W, TEXT("HealthBar"));
    auto* HealthTrail = Find<UProgressBar>(W, TEXT("HealthDamageBar"));
    auto* HealthFlash = Find<UBorder>(W, TEXT("HealthFeedback"));
    auto* ArmorRow = Find<USizeBox>(W, TEXT("ArmorRow"));
    if (!TestNotNull(TEXT("Collapsible armor row is present"), ArmorRow)) return false;
    FTestWorldWrapper Sources;
    if (!TestTrue(TEXT("Create isolated source-identity world"), Sources.CreateTestWorld(EWorldType::Game))) return false;
    AActor* First = Sources.GetTestWorld()->SpawnActor<AActor>();
    AActor* Second = Sources.GetTestWorld()->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("First display source"), First) || !TestNotNull(TEXT("Replacement display source"), Second)) return false;
    FRoguePlayerVitalsView V = Baseline(); V.SourceActor = First; W->UpdateVitals(V);
    V.CurrentHealth = 50; V.HealthPercent = .5f; W->UpdateVitals(V); FAccess::Step(W, .05f);
    TestTrue(TEXT("Damage starts before identity replacement"), FAccess::Health(W).Active());
    V.SourceActor = Second; V.CurrentHealth = 40; V.HealthPercent = .4f; W->UpdateVitals(V);
    TestFalse(TEXT("Same-frame Pawn replacement only establishes a baseline"), FAccess::Health(W).Active());
    TestEqual(TEXT("Replacement health snaps to its real ratio"), HealthBar->GetPercent(), .4f);
    TestEqual(TEXT("Replacement never inherits the old damage ghost"), HealthTrail->GetPercent(), .4f);
    TestEqual(TEXT("Replacement clears old emphasis"), HealthFlash->GetRenderOpacity(), 0.f);
    V.CurrentHealth = 30; V.HealthPercent = .3f; W->UpdateVitals(V);
    V.MaxHealth = 200; V.HealthPercent = .15f; W->UpdateVitals(V);
    TestFalse(TEXT("Capacity changes clear the old damage notification"), FAccess::Health(W).Active());
    TestEqual(TEXT("Capacity changes do not animate a fake hit"), HealthBar->GetPercent(), .15f);
    V.bDowned = true; V.CurrentHealth = 0; V.HealthPercent = 0; W->UpdateVitals(V);
    TestEqual(TEXT("Downed health confirms zero immediately"), HealthBar->GetPercent(), 0.f);
    TestFalse(TEXT("Downed state clears cosmetic deltas"), FAccess::Health(W).Active());
    V.bDowned = false; V.CurrentHealth = 100; V.HealthPercent = .5f; W->UpdateVitals(V);
    TestFalse(TEXT("Revival does not masquerade as healing"), FAccess::Health(W).Active());
    TestEqual(TEXT("Revival has a fresh bar baseline"), HealthBar->GetPercent(), .5f);
    V.bVisible = false; W->UpdateVitals(V);
    TestTrue(TEXT("Missing local gameplay view collapses HUD"), W->GetVisibility() == ESlateVisibility::Collapsed);
    V.bVisible = true; V.CurrentHealth = 80; V.HealthPercent = .4f; W->UpdateVitals(V);
    TestFalse(TEXT("Showing a new observation does not invent damage"), FAccess::Health(W).Active());
    V.bHasArmor = false; V.MaxArmor = 0; V.CurrentArmor = 0; V.ArmorPercent = 0; W->UpdateVitals(V);
    TestTrue(TEXT("Locked armor removes the entire flow row and its gap"), ArmorRow->GetVisibility() == ESlateVisibility::Collapsed);
    TestFalse(TEXT("Removing armor capacity is not armor break"), FAccess::Armor(W).Active());
    auto* ArmorUnavailable = Find<UTextBlock>(W, TEXT("ArmorUnavailableText"));
    if (TestNotNull(TEXT("The saved HUD includes its no-armor caption"), ArmorUnavailable))
        TestEqual(TEXT("No armor shows the muted caption instead of a numeric armor row"), ArmorUnavailable->GetVisibility(), ESlateVisibility::HitTestInvisible);
    V.bHasArmor = true; V.MaxArmor = 50; W->UpdateVitals(V);
    TestTrue(TEXT("Unlocked armor at zero still has a readable empty row"), ArmorRow->GetVisibility() != ESlateVisibility::Collapsed);
    TestFalse(TEXT("Adding armor capacity at zero does not trigger a break"), FAccess::Armor(W).Active());
    if (ArmorUnavailable) TestEqual(TEXT("Unlocking armor removes the empty-state caption"), ArmorUnavailable->GetVisibility(), ESlateVisibility::Collapsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueVitalsExperienceWrapTest,
    "Rogue.UI.VitalsFeedback.LevelWrapExactFullAndMultiLevel", RogueVitalsFeedbackTests::Flags)
bool FRogueVitalsExperienceWrapTest::RunTest(const FString& Parameters)
{
    using namespace RogueVitalsFeedbackTests;
    auto* W = Create(*this); if (!W) return false;
    auto* XP = Find<UProgressBar>(W, TEXT("ExperienceBar"));
    auto* Number = Find<UTextBlock>(W, TEXT("ExperienceChangeText"));
    FRoguePlayerVitalsView V = Baseline(); W->UpdateVitals(V);
    V.Level = 2; V.Experience = 40; V.ExperiencePercent = .4f; W->UpdateVitals(V);
    TestTrue(TEXT("Level increase is detected even when the new ratio is larger"), FAccess::Experience(W).Kind == ERogueVitalsChange::LevelUp);
    TestEqual(TEXT("Level notification reports final level"), Number->GetText().ToString(), FString(TEXT("灵力提升 Lv.2")));
    FAccess::Step(W, FAccess::WrapDuration(W) + .1f);
    TestEqual(TEXT("A hitch still renders an exact full old-level bar"), XP->GetPercent(), 1.f);
    TestTrue(TEXT("Full bar is held rather than reset in the same frame"), FAccess::WrapPending(W));
    FAccess::Step(W, .01f);
    TestEqual(TEXT("Full bar remains visible briefly"), XP->GetPercent(), 1.f);
    FAccess::Step(W, .06f);
    TestEqual(TEXT("New level begins at zero after full-bar hold"), XP->GetPercent(), 0.f);
    Settle(W);
    TestEqual(TEXT("New level settles at the actual remaining experience"), XP->GetPercent(), .4f);
    TestFalse(TEXT("Completed transition is not left pending"), FAccess::WrapPending(W));

    V.Level = 5; V.Experience = 70; V.ExperiencePercent = .7f; W->UpdateVitals(V);
    TestEqual(TEXT("A multi-level grant reports the final level once"), FAccess::Experience(W).Amount, 5.f);
    TestEqual(TEXT("Multi-level message does not sum level identifiers"), Number->GetText().ToString(), FString(TEXT("灵力提升 Lv.5")));
    V.Experience = 80; V.ExperiencePercent = .8f; ++V.Money; W->UpdateVitals(V);
    TestTrue(TEXT("XP gained during the wrap keeps the level notification"), FAccess::Experience(W).Kind == ERogueVitalsChange::LevelUp);
    TestEqual(TEXT("XP earned during the wrap does not accumulate level numbers"), FAccess::Experience(W).Amount, 5.f);
    FAccess::Step(W, 1.f);
    TestEqual(TEXT("Multi-level hitch also displays exact full"), XP->GetPercent(), 1.f);
    FAccess::Step(W, .06f); Settle(W);
    TestEqual(TEXT("Wrap uses the latest XP ratio received during transition"), XP->GetPercent(), .8f);
    TestEqual(TEXT("Final level in the view remains authoritative"), W->GetCachedVitals().Level, 5);
    TestEqual(TEXT("Level notification eventually disappears"), Number->GetRenderOpacity(), 0.f);

    V.Experience = 81; V.ExperiencePercent = .81f; W->UpdateVitals(V);
    TestTrue(TEXT("Later ordinary XP uses the small gain response"), FAccess::Experience(W).Kind == ERogueVitalsChange::Experience);
    TestEqual(TEXT("Ordinary XP delta uses the latest real value"), FAccess::Experience(W).Amount, 1.f);
    Settle(W); TestEqual(TEXT("Ordinary gain settles precisely"), XP->GetPercent(), .81f);
    V.Level = 6; V.Experience = 0; V.ExperiencePercent = 0; W->UpdateVitals(V);
    FAccess::Step(W, 1.f); TestEqual(TEXT("Zero-overflow level still completes old bar"), XP->GetPercent(), 1.f);
    FAccess::Step(W, .06f); Settle(W);
    TestEqual(TEXT("Zero overflow produces an empty new level bar"), XP->GetPercent(), 0.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueVitalsPermanentLevelLabel,
    "Rogue.UI.VitalsFeedback.CurrentLevelPersistsAcrossUpgradeDownedAndNewRun", RogueVitalsFeedbackTests::Flags)
bool FRogueVitalsPermanentLevelLabel::RunTest(const FString&)
{
    using namespace RogueVitalsFeedbackTests;
    auto* W=Create(*this);auto* Other=Create(*this);if(!W||!Other)return false;
    auto* Label=Find<UTextBlock>(W,TEXT("LevelText"));if(!TestNotNull(TEXT("Saved HUD includes the level binding"),Label))return false;
    auto V=Baseline();Other->UpdateVitals(V);
    for(int32 Level:{1,2,37}){V.Level=Level;W->UpdateVitals(V);Settle(W);TestEqual(TEXT("Persistent label follows the final authoritative level"),Label->GetText().ToString(),FString::Printf(TEXT("%d"),Level));}
    V.bHasArmor=false;V.bDowned=true;W->UpdateVitals(V);TestEqual(TEXT("Downed and armor visibility never clear the level"),Label->GetText().ToString(),FString(TEXT("37")));
    V=Baseline();W->UpdateVitals(V);TestEqual(TEXT("A new run resets the label"),Label->GetText().ToString(),FString(TEXT("1")));
    TestEqual(TEXT("Another local HUD retains its own level"),Find<UTextBlock>(Other,TEXT("LevelText"))->GetText().ToString(),FString(TEXT("1")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePendingUpgradeHintTest,
    "Rogue.UI.VitalsFeedback.PendingUpgradeHintBreathesUntilSpent", RogueVitalsFeedbackTests::Flags)
bool FRoguePendingUpgradeHintTest::RunTest(const FString&)
{
    using namespace RogueVitalsFeedbackTests;
    auto* Widget = Create(*this);
    if (!Widget) return false;
    auto* Hint = Find<UTextBlock>(Widget, TEXT("PendingUpgradeHintText"));
    if (!TestNotNull(TEXT("Saved WBP contains the optional upgrade reminder"), Hint)) return false;
    auto View = Baseline();
    Widget->UpdateVitals(View);
    TestEqual(TEXT("No unspent upgrade means no reminder"), Hint->GetVisibility(), ESlateVisibility::Collapsed);
    View.PendingUpgradeSelections = 2;
    Widget->UpdateVitals(View);
    TestEqual(TEXT("Available upgrades show a non-interactive reminder"), Hint->GetVisibility(), ESlateVisibility::HitTestInvisible);
    TestTrue(TEXT("The reminder names the count and key"), Hint->GetText().ToString().Contains(TEXT("2")) && Hint->GetText().ToString().Contains(TEXT("Q")));
    const float Bright = Hint->GetRenderOpacity();
    FAccess::Step(Widget, .8f);
    TestTrue(TEXT("An unchanged view still animates a gentle dim phase"), Hint->GetRenderOpacity() < Bright - .25f);
    FAccess::Step(Widget, .9f);
    TestTrue(TEXT("The pulse returns to a readable bright phase"), Hint->GetRenderOpacity() > .85f);
    View.PendingUpgradeSelections = 0;
    Widget->UpdateVitals(View);
    TestEqual(TEXT("Spending the final upgrade immediately removes the reminder"), Hint->GetVisibility(), ESlateVisibility::Collapsed);
    View.PendingUpgradeSelections = 1; View.bDowned = true;
    Widget->UpdateVitals(View);
    TestEqual(TEXT("A downed player is not prompted to open an unavailable menu"), Hint->GetVisibility(), ESlateVisibility::Collapsed);
    View.bDowned = false;
    Widget->UpdateVitals(View);
    TestEqual(TEXT("Retained upgrades are prompted after revival"), Hint->GetVisibility(), ESlateVisibility::HitTestInvisible);
    return true;
}

#endif
