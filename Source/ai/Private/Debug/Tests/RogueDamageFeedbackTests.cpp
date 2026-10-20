#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/Widgets/RogueDamageFlashWidgetBase.h"

/**
 * 受伤泛红的强度曲线。
 *
 * 只测 `FRogueDamageFlashState`（峰值、保持、淡出、下限、连续受击取峰值）：
 * 它决定"每次受击的观感强度"，是最容易在调参时改错的部分，也能脱离 UMG 单独验证。
 * 外观本身归 WBP，不在这里测。
 */
namespace RogueDamageFeedbackTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	// 与 URogueDamageFlashWidgetBase 的默认属性保持一致，改默认值时这里也要跟着改。
	constexpr float HoldSeconds = 0.06f;
	constexpr float FadeOutSeconds = 0.55f;
	constexpr float MinIntensity = 0.2f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDamageFlashStrengthCurveTest,
	"Rogue.UI.DamageFlash.StrengthCurve", RogueDamageFeedbackTests::Flags)

bool FRogueDamageFlashStrengthCurveTest::RunTest(const FString& Parameters)
{
	using namespace RogueDamageFeedbackTests;

	FRogueDamageFlashState State;

	// 未受伤时不显示任何泛红。
	TestEqual(TEXT("Idle strength is zero"), State.GetStrength(HoldSeconds, FadeOutSeconds), 0.0f, 0.0001f);
	TestFalse(TEXT("Idle state is inactive"), State.IsActive());

	State.Play(0.5f, MinIntensity);
	TestEqual(TEXT("Peak equals the requested intensity"),
		State.GetStrength(HoldSeconds, FadeOutSeconds), 0.5f, 0.001f);

	// Hold 时段内保持峰值，给瞬间伤害留出可见时间。
	TestEqual(TEXT("Strength holds right after the hit"),
		State.Advance(0.03f, HoldSeconds, FadeOutSeconds), 0.5f, 0.001f);

	// 连续受击取较高值：弱的那一下不能把强的残留冲淡。
	State.Play(0.2f, MinIntensity);
	TestEqual(TEXT("A weaker hit cannot lower the peak"),
		State.GetStrength(HoldSeconds, FadeOutSeconds), 0.5f, 0.001f);

	State.Play(0.9f, MinIntensity);
	TestEqual(TEXT("A stronger hit raises the peak"),
		State.GetStrength(HoldSeconds, FadeOutSeconds), 0.9f, 0.001f);

	// 跨过保持 + 淡出后归零，且状态失活。
	State.Advance(HoldSeconds + FadeOutSeconds + 0.05f, HoldSeconds, FadeOutSeconds);
	TestEqual(TEXT("Strength fades to zero"), State.GetStrength(HoldSeconds, FadeOutSeconds), 0.0f, 0.0001f);
	TestFalse(TEXT("State deactivates after fading out"), State.IsActive());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDamageFlashMinimumIntensityTest,
	"Rogue.UI.DamageFlash.MinimumIntensity", RogueDamageFeedbackTests::Flags)

bool FRogueDamageFlashMinimumIntensityTest::RunTest(const FString& Parameters)
{
	using namespace RogueDamageFeedbackTests;

	FRogueDamageFlashState State;

	// 擦伤（1% 血量）也要有可感知的反馈，否则玩家会以为"没掉血"。
	State.Play(0.01f, MinIntensity);
	TestEqual(TEXT("A tiny hit is raised to the minimum intensity"),
		State.GetStrength(HoldSeconds, FadeOutSeconds), 0.2f, 0.001f);

	// 强度参数不该被放大：超过 1 的输入按 1 处理。
	State.Play(2.0f, MinIntensity);
	TestEqual(TEXT("Intensity is clamped to one"),
		State.GetStrength(HoldSeconds, FadeOutSeconds), 1.0f, 0.001f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
