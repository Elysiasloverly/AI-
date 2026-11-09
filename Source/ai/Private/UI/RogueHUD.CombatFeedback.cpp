// 战斗反馈：血条缓存、飘字与受伤表现；所有缓存按本地 HUD 实例保存。
#include "UI/RogueHUD.h"
#include "UI/Widgets/RogueHUDViewData.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "UI/Widgets/RogueOwnedItemsWidget.h"
#include "UI/Widgets/RogueDamageFlashWidgetBase.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "UI/Widgets/RogueFloatingNumberMotion.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameState.h"
#include "UI/Widgets/RogueEnemyHealthBarWidgetBase.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "RogueHUDWidgetFactory.h"
using namespace RogueHUDWidgetFactory;

namespace
{
constexpr int32 MaxDamageNumbersNormal = 42;
constexpr int32 MaxDamageNumbersHeavy = 24;
constexpr int32 MaxRegularHealthBarsNormal = 22;
constexpr int32 MaxRegularHealthBarsHeavy = 12;
constexpr float EnemyHealthBarRefreshIntervalNormal = 0.05f;
constexpr float EnemyHealthBarRefreshIntervalHeavy = 0.10f;
constexpr float DamageProjectionRefreshIntervalNormal = 0.05f;
constexpr float DamageProjectionRefreshIntervalHeavy = 0.09f;

} // namespace

void ARogueHUD::AddDamageNumber(const FVector &WorldLocation, float Damage, bool bIsBossDamage)
{
	if (GetWorld() == nullptr || Damage <= 0.0f)
	{
		return;
	}

	const URogueEnemyTrackerSubsystem *TrackerForDamage =
		GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	const bool bCrowded = TrackerForDamage && TrackerForDamage->ShouldCullCombatEffects();
	const int32 MaxDamageNumbers = bCrowded ? MaxDamageNumbersHeavy : MaxDamageNumbersNormal;
	if (DamageNumbers.Num() >= MaxDamageNumbers)
	{
		const int32 OverflowCount = DamageNumbers.Num() - MaxDamageNumbers + 1;
		DamageNumbers.RemoveAt(0, OverflowCount, EAllowShrinking::No);
	}

	FRogueDamageNumber DamageNumber;
	DamageNumber.WorldLocation = WorldLocation;
	DamageNumber.Damage = Damage;
	DamageNumber.SpawnTime = GetWorld()->GetTimeSeconds();
	DamageNumber.Lifetime = bIsBossDamage ? 0.95f : (bCrowded ? 0.52f : 0.72f);
	DamageNumber.Color =
		bIsBossDamage ? FLinearColor(1.0f, 0.55f, 0.25f, 1.0f) : FLinearColor(1.0f, 0.92f, 0.55f, 1.0f);
	if (bCrowded && !bIsBossDamage) DamageNumber.Color.A = .72f;
	DamageNumber.bScreenPositionValid = false;
	DamageNumber.bBoss = bIsBossDamage;
	const uint32 VisualIndex = NumberVisualSequence++;
	DamageNumber.HorizontalDrift = (VisualIndex % 2 ? 1.f : -1.f) * (8.f + float(VisualIndex % 5) * 2.f);
	// 文本在这里算一次（数值之后不会再变，见 FRogueDamageNumber::CachedText 的说明）
	DamageNumber.CachedText = FString::Printf(TEXT("%.0f"), Damage);
	DamageNumbers.Add(MoveTemp(DamageNumber));
}

void ARogueHUD::AddHealingNumber(const FVector &WorldLocation, float Amount)
{
	if (GetWorld() == nullptr || Amount <= 0.0f)
	{
		return;
	}

	// 与伤害飘字共用数组与上限，避免玩家回血时额外撑高绘制开销。
	const URogueEnemyTrackerSubsystem *TrackerForNumbers =
		GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	const int32 MaxNumbers = TrackerForNumbers != nullptr && TrackerForNumbers->ShouldCullCombatEffects()
								 ? MaxDamageNumbersHeavy
								 : MaxDamageNumbersNormal;
	if (DamageNumbers.Num() >= MaxNumbers)
	{
		const int32 OverflowCount = DamageNumbers.Num() - MaxNumbers + 1;
		DamageNumbers.RemoveAt(0, OverflowCount, EAllowShrinking::No);
	}

	FRogueDamageNumber HealingNumber;
	HealingNumber.WorldLocation = WorldLocation;
	HealingNumber.Damage = Amount;
	HealingNumber.SpawnTime = GetWorld()->GetTimeSeconds();
	HealingNumber.Lifetime = 0.9f;
	HealingNumber.Color = FLinearColor(0.35f, 1.0f, 0.5f, 1.0f);
	HealingNumber.bScreenPositionValid = false;
	HealingNumber.bHealing = true;
	const uint32 VisualIndex = NumberVisualSequence++;
	HealingNumber.HorizontalDrift = (VisualIndex % 2 ? 1.f : -1.f) * (6.f + float(VisualIndex % 4) * 2.f);
	// 小额实际回复保留小数，避免将0.1点写成1点。
	HealingNumber.CachedText = Amount < .1f ? FString::Printf(TEXT("+%.2f"), Amount)
											: (Amount < 1.f ? FString::Printf(TEXT("+%.1f"), Amount)
															: FString::Printf(TEXT("+%.0f"), Amount));
	DamageNumbers.Add(MoveTemp(HealingNumber));
}

void ARogueHUD::DrawEnemyHealthBars(UFont *Font)
{
	APlayerController *PlayerController = GetOwningPlayerController();
	URogueEnemyTrackerSubsystem *Tracker =
		GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	if (Canvas == nullptr || PlayerController == nullptr || Tracker == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const bool bHeavyCombat = Tracker->ShouldCullCombatEffects();
	const float RefreshInterval =
		bHeavyCombat ? EnemyHealthBarRefreshIntervalHeavy : EnemyHealthBarRefreshIntervalNormal;
	if (CurrentTime >= NextHealthBarCacheRefreshTime)
	{
		RefreshEnemyHealthBarCache(Tracker);
		NextHealthBarCacheRefreshTime = CurrentTime + RefreshInterval;
	}

	for (const FRogueCachedEnemyHealthBar &CachedBar : CachedEnemyHealthBars)
	{
		ARogueEnemy *Enemy = CachedBar.Enemy.Get();
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(Enemy->GetHealthBarWorldLocation(),
															ScreenPosition, true))
		{
			continue;
		}

		if (ScreenPosition.X < -120.0f || ScreenPosition.X > Canvas->SizeX + 120.0f ||
			ScreenPosition.Y < -80.0f || ScreenPosition.Y > Canvas->SizeY + 80.0f)
		{
			continue;
		}

		const float BarWidth = CachedBar.bBoss ? 136.0f : 62.0f;
		const float BarHeight = CachedBar.bBoss ? 10.0f : 6.0f;
		const float BarX = ScreenPosition.X - BarWidth * 0.5f;
		const float BarY = ScreenPosition.Y;
		const FLinearColor FillColor = CachedBar.bBoss ? FLinearColor(0.95f, 0.30f, 0.20f, 0.95f)
													   : FLinearColor(0.95f, 0.15f, 0.20f, 0.90f);
		const float RawHealthPercent = Enemy->GetHealthPercent();
		const float HealthPercent =
			FMath::IsFinite(RawHealthPercent) ? FMath::Clamp(RawHealthPercent, 0.0f, 1.0f) : 0.0f;

		DrawRect(FLinearColor(0.04f, 0.04f, 0.05f, 0.70f), BarX - 2.0f, BarY - 2.0f, BarWidth + 4.0f,
				 BarHeight + 4.0f);
		DrawRect(FLinearColor(0.10f, 0.10f, 0.12f, 0.90f), BarX, BarY, BarWidth, BarHeight);
		DrawRect(FillColor, BarX, BarY, BarWidth * HealthPercent, BarHeight);

		if (CachedBar.bBoss)
		{
			DrawText(TEXT("BOSS"), FLinearColor(1.0f, 0.8f, 0.45f, 0.95f), BarX + 44.0f, BarY - 18.0f, Font,
					 0.9f, false);
		}
	}
}

void ARogueHUD::UpdateVitalsWidget(const ARogueCharacter *PlayerCharacter)
{
	if (VitalsWidget == nullptr)
	{
		return;
	}

	auto SafePercent = [](float Value) {
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f;
	};

	FRoguePlayerVitalsView View;
	if (PlayerCharacter != nullptr)
	{
		View.SourceActor = const_cast<ARogueCharacter *>(PlayerCharacter);
		View.bVisible = true;
		View.bHasArmor = PlayerCharacter->HasArmor();
		View.bDowned = PlayerCharacter->IsDowned();
		View.HealthPercent = SafePercent(PlayerCharacter->GetHealthPercent());
		View.ArmorPercent = SafePercent(PlayerCharacter->GetArmorPercent());
		View.ExperiencePercent = SafePercent(PlayerCharacter->GetExperiencePercent());
		View.CurrentHealth = PlayerCharacter->GetCurrentHealth();
		View.MaxHealth = PlayerCharacter->GetMaxHealth();
		View.CurrentArmor = PlayerCharacter->GetCurrentArmor();
		View.MaxArmor = PlayerCharacter->GetMaxArmor();
		View.Level = PlayerCharacter->GetPlayerLevel();
        if(const auto* State=GetLocalRoguePlayerState())View.PendingUpgradeSelections=State->GetPendingUpgradeCount();
		View.Experience = PlayerCharacter->GetCurrentExperience();
		View.ExperienceToNextLevel = PlayerCharacter->GetExperienceToNextLevel();
		View.Money = PlayerCharacter->GetMoney();
		if (const ARogueGameState *RogueGameState = GetRogueGameState())
		{
			View.CurrentWave = RogueGameState->GetCurrentWave();
            View.RemainingEnemies = RogueGameState->GetEnemyLocatorView().Remaining;
		}
	}

	RogueVitalsView::UpdateDashState(PlayerCharacter, View);
	VitalsWidget->UpdateVitals(View);
    // 使用实际 WBP 几何而不是另一套固定宽度，设计器调整状态条后仍能自动避让。
    if (OwnedItemsWidget)
    {
        if (const UWidget* Panel=VitalsWidget->GetWidgetFromName(TEXT("VitalsPanel")))
        {
            const auto& Geometry=Panel->GetCachedGeometry();
            if (Geometry.GetLocalSize().X>0)
            {
                const FVector2D Right=OwnedItemsWidget->GetCachedGeometry().AbsoluteToLocal(
                    Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
                OwnedItemsWidget->SetVitalsExclusionRightEdge(Right.X+24.f);
            }
        }
    }
}

void ARogueHUD::UpdateLocalDamageFeedback(ARogueCharacter *PlayerCharacter)
{
	// 泛红淡出由 HUD 按帧推进：即使这一帧没有受伤，也要继续把上一次的残影收干净。
	if (DamageFlashWidget != nullptr)
	{
		const UWorld *World = GetWorld();
		DamageFlashWidget->AdvanceFlash(World != nullptr ? World->GetDeltaSeconds() : 0.0f);
	}

	if (PlayerCharacter == nullptr)
	{
		// 观战 / 团灭 / 尚未 Possess：没有"我的血量"可比，清掉基线。
		// 否则下一个角色上线时会把它的满血当成"从 0 涨回来"，或者把残血当成一次重击。
		LastObservedHealthPawn = nullptr;
		LastObservedHealth = -1.0f;
		return;
	}

	if (LastObservedHealthPawn.Get() != PlayerCharacter)
	{
		// 换 Pawn（重生、重连、分屏另一个玩家）：只建立基线，不触发反馈。
		LastObservedHealthPawn = PlayerCharacter;
		LastObservedHealth = PlayerCharacter->GetCurrentHealth();
		return;
	}

	const float CurrentHealth = PlayerCharacter->GetCurrentHealth();
	const float PreviousHealth = LastObservedHealth;
	LastObservedHealth = CurrentHealth;

	// 只在血量**下降**时触发。0.5 的容差用来吸收浮点与护甲/再生同帧抵消带来的抖动；
	// 治疗、上限提升（HandleMaxHealthChanged 补满增量）都走上升方向，不会误触发。
	const float DamageTaken = PreviousHealth - CurrentHealth;
	if (DamageTaken <= 0.5f)
	{
		return;
	}

	const float MaxHealth = FMath::Max(PlayerCharacter->GetMaxHealth(), 1.0f);
	TriggerDamageFeedback(DamageTaken / MaxHealth);
}

void ARogueHUD::TriggerDamageFeedback(float Intensity)
{
	const float Normalized = FMath::Clamp(Intensity, 0.0f, 1.0f);

	// 泛红交给 WBP：这里只传强度，颜色/边缘宽度/动画都由蓝图决定。
	if (DamageFlashWidget != nullptr)
	{
		DamageFlashWidget->PlayDamageFlash(Normalized);
	}

	APlayerController *PlayerController = GetOwningPlayerController();
	if (PlayerController == nullptr || PlayerController->PlayerCameraManager == nullptr ||
		DamageCameraShakeClass == nullptr)
	{
		return;
	}

	// 连续受击（多段伤害、多个敌人同帧命中）会叠加震动实例，抖成一片。
	// 用最小间隔压成一次，但下一次仍会按新伤害重新评估强度。
	const UWorld *World = GetWorld();
	const float Now = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastCameraShakeTime < CameraShakeMinInterval)
	{
		return;
	}
	LastCameraShakeTime = Now;

	// 小伤害也保留可感知的震动：缩放不低于 0.55，避免"掉了血但画面完全没反应"。
	const float Scale = FMath::Lerp(0.55f, 1.0f, Normalized);
	PlayerController->PlayerCameraManager->StartCameraShake(DamageCameraShakeClass, Scale);
}

void ARogueHUD::UpdateEnemyHealthBarWidgets()
{
	APlayerController *PlayerController = GetOwningPlayerController();
	UWorld *World = GetWorld();
	URogueEnemyTrackerSubsystem *Tracker =
		World != nullptr ? World->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	if (PlayerController == nullptr || World == nullptr || Tracker == nullptr ||
		EnemyHealthBarWidgetClass == nullptr)
	{
		HideAllEnemyHealthBarWidgets();
		return;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		HideAllEnemyHealthBarWidgets();
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const bool bHeavyCombat = Tracker->ShouldCullCombatEffects();
	const float RefreshInterval =
		bHeavyCombat ? EnemyHealthBarRefreshIntervalHeavy : EnemyHealthBarRefreshIntervalNormal;
	if (CurrentTime >= NextHealthBarCacheRefreshTime)
	{
		RefreshEnemyHealthBarCache(Tracker);
		NextHealthBarCacheRefreshTime = CurrentTime + RefreshInterval;
	}

	int32 UsedWidgets = 0;
	for (const FRogueCachedEnemyHealthBar &CachedBar : CachedEnemyHealthBars)
	{
		ARogueEnemy *Enemy = CachedBar.Enemy.Get();
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(Enemy->GetHealthBarWorldLocation(),
															ScreenPosition, true))
		{
			continue;
		}

		if (ScreenPosition.X < -120.0f || ScreenPosition.X > ViewportSizeX + 120.0f ||
			ScreenPosition.Y < -80.0f || ScreenPosition.Y > ViewportSizeY + 80.0f)
		{
			continue;
		}

		URogueEnemyHealthBarWidgetBase *Widget = AcquireEnemyHealthBarWidget(UsedWidgets);
		if (Widget == nullptr)
		{
			break;
		}

		// 显式排除非有限比例，再钳到 [0,1]，防止异常比例传给 WBP 的进度条。
		const float RawHealthPercent = Enemy->GetHealthPercent();

		FRogueEnemyHealthBarView View;
		View.SourceActor = Enemy;
		View.SpawnId = Enemy->GetSpawnCycleId();
		View.SpawnGeneration = Enemy->GetPresentationGeneration();
		View.bVisible = true;
		View.bBoss = CachedBar.bBoss;
		View.HealthPercent =
			FMath::IsFinite(RawHealthPercent) ? FMath::Clamp(RawHealthPercent, 0.0f, 1.0f) : 0.0f;
		View.ScreenPosition = ScreenPosition;
		Widget->UpdateBar(View);

		++UsedWidgets;
	}

	for (int32 Index = UsedWidgets; Index < EnemyHealthBarWidgets.Num(); ++Index)
	{
		if (EnemyHealthBarWidgets[Index] != nullptr)
		{
			EnemyHealthBarWidgets[Index]->UpdateBar(FRogueEnemyHealthBarView());
		}
	}
}

URogueEnemyHealthBarWidgetBase *ARogueHUD::AcquireEnemyHealthBarWidget(int32 Index)
{
	while (EnemyHealthBarWidgets.Num() <= Index)
	{
		URogueEnemyHealthBarWidgetBase *Widget =
			CreateRogueMenuWidget(GetOwningPlayerController(), EnemyHealthBarWidgetClass, this, 6);
		if (Widget == nullptr)
		{
			return nullptr;
		}

		// 血条是屏幕空间的小控件：先按 WBP 的 BarSize 固定尺寸，再以顶部中心对齐。
		Widget->SetDesiredSizeInViewport(Widget->GetBarSize());
		Widget->SetAlignmentInViewport(FVector2D(0.5f, 0.0f));
		EnemyHealthBarWidgets.Add(Widget);
	}

	return EnemyHealthBarWidgets[Index];
}

void ARogueHUD::HideAllEnemyHealthBarWidgets()
{
	for (TObjectPtr<URogueEnemyHealthBarWidgetBase> &Widget : EnemyHealthBarWidgets)
	{
		if (Widget != nullptr)
		{
			Widget->UpdateBar(FRogueEnemyHealthBarView());
		}
	}
}

void ARogueHUD::DrawDamageNumbers(UFont *Font)
{
	APlayerController *PlayerController = GetOwningPlayerController();
	URogueEnemyTrackerSubsystem *Tracker =
		GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	if (Canvas == nullptr || PlayerController == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const bool bHeavyCombat = Tracker != nullptr && Tracker->ShouldCullCombatEffects();
	const int32 MaxDamageNumbers = bHeavyCombat ? MaxDamageNumbersHeavy : MaxDamageNumbersNormal;
	if (DamageNumbers.Num() > MaxDamageNumbers)
	{
		DamageNumbers.RemoveAt(0, DamageNumbers.Num() - MaxDamageNumbers, EAllowShrinking::No);
	}

	for (int32 Index = DamageNumbers.Num() - 1; Index >= 0; --Index)
	{
		const float Age = CurrentTime - DamageNumbers[Index].SpawnTime;
		if (Age >= DamageNumbers[Index].Lifetime)
		{
			DamageNumbers.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}

	const float RefreshInterval =
		bHeavyCombat ? DamageProjectionRefreshIntervalHeavy : DamageProjectionRefreshIntervalNormal;

	// 飘字为空时跳过投影缓存更新；有数据时按设定间隔更新屏幕位置。
	// 是否为 HUD 性能瓶颈需要在具体场景测量，不能仅由元素数推断。
	if (DamageNumbers.Num() > 0 &&
		(CurrentTime >= NextDamageNumberProjectionRefreshTime || GIsHighResScreenshot))
	{
		RefreshDamageNumberProjectionCache(PlayerController, Tracker);
		NextDamageNumberProjectionRefreshTime = CurrentTime + RefreshInterval;
	}

	for (FRogueDamageNumber &DamageNumber : DamageNumbers)
	{
		const float Age = CurrentTime - DamageNumber.SpawnTime;
		// Newly received numbers need one immediate projection, without refreshing the whole cache.
		if (!DamageNumber.bScreenPositionValid)
		{
			const FVector P = Canvas->Project(DamageNumber.WorldLocation);
			DamageNumber.CachedScreenPosition = FVector2D(P.X, P.Y);
			DamageNumber.bScreenPositionValid = P.Z > 0;
		}
		if (Age >= DamageNumber.Lifetime || !DamageNumber.bScreenPositionValid)
		{
			continue;
		}
		const auto Motion =
			FRogueFloatingNumberMotion::Sample(Age, DamageNumber.Lifetime, DamageNumber.HorizontalDrift,
											   DamageNumber.bHealing, DamageNumber.bBoss);
		const float Alpha = Motion.Opacity * DamageNumber.Color.A;
		FLinearColor TextColor = DamageNumber.Color;
		TextColor.A = Alpha;
		const float Scale = Motion.Scale;
		const FString &DamageText = DamageNumber.CachedText;
		if (DamageNumber.CachedTextSize.X <= 0)
		{
			float TextWidth = 0, TextHeight = 0;
			Canvas->StrLen(Font, DamageText, TextWidth, TextHeight);
			DamageNumber.CachedTextSize = FVector2D(TextWidth, TextHeight);
		}
		const FVector2D Pos = DamageNumber.CachedScreenPosition + Motion.Offset -
							  FVector2D(DamageNumber.CachedTextSize.X * .5f * Scale, 0);
		DrawText(DamageText, FLinearColor(.015f, .018f, .035f, Alpha * .78f), Pos.X + 1.2f, Pos.Y + 1.6f,
				 Font, Scale, false);
		DrawText(DamageText, TextColor, Pos.X, Pos.Y, Font, Scale, false);
	}
}

void ARogueHUD::RefreshEnemyHealthBarCache(const URogueEnemyTrackerSubsystem *Tracker)
{
	CachedEnemyHealthBars.Reset();
	if (Tracker == nullptr)
	{
		return;
	}

	const APlayerController *PlayerController = GetOwningPlayerController();
	const APawn *PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	const FVector PlayerLocation =
		PlayerPawn != nullptr ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	const bool bHeavyCombat = Tracker->ShouldCullCombatEffects();
	const float MaxBarDistance = bHeavyCombat ? 1300.0f : 1800.0f;
	const int32 MaxRegularBars = bHeavyCombat ? MaxRegularHealthBarsHeavy : MaxRegularHealthBarsNormal;
	struct FCandidateBar
	{
		ARogueEnemy *Enemy = nullptr;
		float DistanceSquared = 0.0f;
		bool bBoss = false;
	};

	TArray<FCandidateBar> RegularCandidates;
	RegularCandidates.Reserve(MaxRegularBars + 12);

	int32 DrawnRegularBars = 0;

	for (const TWeakObjectPtr<ARogueEnemy> &EnemyPtr : Tracker->GetActiveEnemies())
	{
		ARogueEnemy *Enemy = EnemyPtr.Get();
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		const bool bBoss = Enemy->IsBoss();
		const float DistanceSquared = FVector::DistSquared2D(PlayerLocation, Enemy->GetActorLocation());
		if (!bBoss && DistanceSquared > FMath::Square(MaxBarDistance))
		{
			continue;
		}

		if (bBoss)
		{
			FRogueCachedEnemyHealthBar &CachedBar = CachedEnemyHealthBars.AddDefaulted_GetRef();
			CachedBar.Enemy = Enemy;
			CachedBar.bBoss = true;
			continue;
		}

		FCandidateBar &Candidate = RegularCandidates.AddDefaulted_GetRef();
		Candidate.Enemy = Enemy;
		Candidate.DistanceSquared = DistanceSquared;
		Candidate.bBoss = false;
	}

	RegularCandidates.Sort([](const FCandidateBar &Left, const FCandidateBar &Right) {
		return Left.DistanceSquared < Right.DistanceSquared;
	});

	for (const FCandidateBar &Candidate : RegularCandidates)
	{
		if (DrawnRegularBars >= MaxRegularBars)
		{
			break;
		}

		FRogueCachedEnemyHealthBar &CachedBar = CachedEnemyHealthBars.AddDefaulted_GetRef();
		CachedBar.Enemy = Candidate.Enemy;
		CachedBar.bBoss = false;
		++DrawnRegularBars;
	}
}

void ARogueHUD::RefreshDamageNumberProjectionCache(APlayerController *PlayerController,
												   const URogueEnemyTrackerSubsystem *Tracker)
{
	if (PlayerController == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const bool bHeavyCombat = Tracker != nullptr && Tracker->ShouldCullCombatEffects();

	for (FRogueDamageNumber &DamageNumber : DamageNumbers)
	{
		const float Age = CurrentTime - DamageNumber.SpawnTime;
		DamageNumber.bScreenPositionValid = false;
		if (Age >= DamageNumber.Lifetime)
		{
			continue;
		}

		const FVector PopupLocation = DamageNumber.WorldLocation;
		// Canvas使用当前绘制视图和尺寸，避免高分辨率输出仍套用原窗口像素坐标。
		if (Canvas)
		{
			const FVector Projected = Canvas->Project(PopupLocation);
			DamageNumber.CachedScreenPosition = FVector2D(Projected.X, Projected.Y);
			DamageNumber.bScreenPositionValid = Projected.Z > 0;
		}
		else
			DamageNumber.bScreenPositionValid = PlayerController->ProjectWorldLocationToScreen(
				PopupLocation, DamageNumber.CachedScreenPosition, true);
	}
}
