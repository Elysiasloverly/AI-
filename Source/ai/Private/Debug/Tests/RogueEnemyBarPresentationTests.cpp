#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Widgets/RogueEnemyHealthBarWidgetBase.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

struct FRogueEnemyBarTestAccess
{
	static void Step(URogueEnemyHealthBarWidgetBase* Widget, float Delta) { Widget->NativeTick(FGeometry(), Delta); }
	static float Fill(const URogueEnemyHealthBarWidgetBase* Widget) { return Widget->FillBar->GetPercent(); }
	static float Trail(const URogueEnemyHealthBarWidgetBase* Widget) { return Widget->DamageBar->GetPercent(); }
	static float Highlight(const URogueEnemyHealthBarWidgetBase* Widget) { return Widget->HitHighlightRemaining; }
};

namespace RogueEnemyBarTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	URogueEnemyHealthBarWidgetBase* ActualWidget()
	{
		UClass* Class = LoadClass<URogueEnemyHealthBarWidgetBase>(nullptr,
			TEXT("/Game/UMG/HUD/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
		if (!Class) { return nullptr; }
		URogueEnemyHealthBarWidgetBase* Widget = NewObject<URogueEnemyHealthBarWidgetBase>(GetTransientPackage(), Class);
		return Widget->Initialize() ? Widget : nullptr;
	}
	bool HasBindings(URogueEnemyHealthBarWidgetBase* Widget)
	{
		return Widget && Widget->GetWidgetFromName(TEXT("FillBar")) && Widget->GetWidgetFromName(TEXT("DamageBar"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyBarPoolIdentityTest,
	"Rogue.UI.EnemyBar.IdentityAndPoolReset", RogueEnemyBarTests::Flags)
bool FRogueEnemyBarPoolIdentityTest::RunTest(const FString& Parameters)
{
	URogueEnemyHealthBarWidgetBase* Widget = RogueEnemyBarTests::ActualWidget();
	if (!TestTrue(TEXT("Saved enemy WBP initializes both bound bars"), RogueEnemyBarTests::HasBindings(Widget))) { return false; }
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
	AActor* First = TestWorld.GetTestWorld()->SpawnActor<AActor>();
	AActor* Second = TestWorld.GetTestWorld()->SpawnActor<AActor>();
	FRogueEnemyHealthBarView View;
	View.bVisible = true; View.SourceActor = First; View.SpawnGeneration = 7; View.HealthPercent = 0.9f;
	Widget->UpdateBar(View);
	TestEqual(TEXT("Initial observation snaps to real health"), FRogueEnemyBarTestAccess::Fill(Widget), 0.9f);
	View.HealthPercent = 0.35f; Widget->UpdateBar(View); FRogueEnemyBarTestAccess::Step(Widget, 0.05f);
	TestTrue(TEXT("Damage starts an interpolation"), FRogueEnemyBarTestAccess::Fill(Widget) > 0.35f);
	TestEqual(TEXT("Damage briefly holds a warm trail"), FRogueEnemyBarTestAccess::Trail(Widget), 0.9f);
	TestTrue(TEXT("Damage starts a restrained highlight"), FRogueEnemyBarTestAccess::Highlight(Widget) > 0.0f);

	// 客户端即使没有可用 GUID，也能依靠本地 Actor 身份区分池中相邻的两只敌人。
	View.SourceActor = Second; View.HealthPercent = 0.6f; Widget->UpdateBar(View);
	TestEqual(TEXT("Slot transfer with empty GUID snaps to the new actor"), FRogueEnemyBarTestAccess::Fill(Widget), 0.6f);
	TestEqual(TEXT("Slot transfer discards the previous damage trail"), FRogueEnemyBarTestAccess::Trail(Widget), 0.6f);
	TestEqual(TEXT("Slot transfer discards the previous hit highlight"), FRogueEnemyBarTestAccess::Highlight(Widget), 0.0f);
	View.HealthPercent = 0.2f; Widget->UpdateBar(View); FRogueEnemyBarTestAccess::Step(Widget, 0.04f);
	View.SpawnGeneration = 8; View.HealthPercent = 0.75f; Widget->UpdateBar(View);
	TestEqual(TEXT("Same actor reused in another pool generation snaps"), FRogueEnemyBarTestAccess::Fill(Widget), 0.75f);
	TestEqual(TEXT("Pool generation clears the old trail"), FRogueEnemyBarTestAccess::Trail(Widget), 0.75f);
	Widget->UpdateBar(FRogueEnemyHealthBarView());
	TestEqual(TEXT("Hidden widgets clear fill"), FRogueEnemyBarTestAccess::Fill(Widget), 0.0f);
	TestEqual(TEXT("Hidden widgets clear trail"), FRogueEnemyBarTestAccess::Trail(Widget), 0.0f);
	TestEqual(TEXT("Hidden widgets collapse"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
	View.HealthPercent = 0.4f; Widget->UpdateBar(View);
	TestEqual(TEXT("Becoming visible again starts at true health"), FRogueEnemyBarTestAccess::Fill(Widget), 0.4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyBarDamageAndStyleTest,
	"Rogue.UI.EnemyBar.SmoothingAndCapsuleStyle", RogueEnemyBarTests::Flags)
bool FRogueEnemyBarDamageAndStyleTest::RunTest(const FString& Parameters)
{
	URogueEnemyHealthBarWidgetBase* Widget = RogueEnemyBarTests::ActualWidget();
	if (!TestTrue(TEXT("Saved enemy WBP initializes both bound bars"), RogueEnemyBarTests::HasBindings(Widget))) { return false; }
	UProgressBar* Fill = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("FillBar")));
	UProgressBar* Trail = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("DamageBar")));
	const FLinearColor DesignerColor = Fill->GetFillColorAndOpacity();
	for (UProgressBar* Bar : { Fill, Trail })
	{
		TestEqual(TEXT("Fill and trail use rounded capsule brushes"), Bar->GetWidgetStyle().FillImage.DrawAs.GetValue(), ESlateBrushDrawType::RoundedBox);
		TestEqual(TEXT("Capsule follows bar height"), Bar->GetWidgetStyle().FillImage.OutlineSettings.RoundingType.GetValue(), ESlateBrushRoundingType::HalfHeightRadius);
		TestEqual(TEXT("Fill scales its rounded end instead of clipping it square"), Bar->GetBarFillStyle(), EProgressBarFillStyle::Scale);
	}
	FRogueEnemyHealthBarView View;
	View.bVisible = true; View.SpawnId = FGuid::NewGuid(); View.HealthPercent = 1.0f;
	Widget->UpdateBar(View);
	View.HealthPercent = 0.5f; Widget->UpdateBar(View); FRogueEnemyBarTestAccess::Step(Widget, 0.05f);
	TestTrue(TEXT("Primary bar moves toward the authoritative ratio"), FRogueEnemyBarTestAccess::Fill(Widget) > 0.5f && FRogueEnemyBarTestAccess::Fill(Widget) < 1.0f);
	for (int32 Index = 0; Index < 120; ++Index)
	{
		// HUD submits unchanged data every frame; this must not restart hit feedback.
		Widget->UpdateBar(View); FRogueEnemyBarTestAccess::Step(Widget, 1.0f / 60.0f);
	}
	TestEqual(TEXT("Primary settles at real health"), FRogueEnemyBarTestAccess::Fill(Widget), 0.5f);
	TestEqual(TEXT("Damage trail fully clears"), FRogueEnemyBarTestAccess::Trail(Widget), 0.5f);
	TestEqual(TEXT("Unchanged snapshots do not prolong the highlight"), FRogueEnemyBarTestAccess::Highlight(Widget), 0.0f);
	TestTrue(TEXT("Designer fill color is restored after feedback"), Fill->GetFillColorAndOpacity().Equals(DesignerColor));
	View.HealthPercent = 0.8f; Widget->UpdateBar(View); FRogueEnemyBarTestAccess::Step(Widget, 0.05f);
	TestEqual(TEXT("Healing cannot leave a false damage trail"), FRogueEnemyBarTestAccess::Trail(Widget), FRogueEnemyBarTestAccess::Fill(Widget));
	View.HealthPercent = -5.0f; Widget->UpdateBar(View);
	TestEqual(TEXT("Invalid low health is clamped in the cached snapshot"), Widget->GetCachedBar().HealthPercent, 0.0f);
	return true;
}

#endif
