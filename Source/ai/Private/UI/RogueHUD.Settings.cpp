// 设置页：只管理本地选项及应用，不负责战斗结算。
#include "UI/RogueHUD.h"
#include "UI/Widgets/RogueMenuWidgetBases.h"
#include "Camera/CameraShakeBase.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Player/RogueCharacter.h"
#include "Framework/RogueGameState.h"
#include "Audio/RogueMusicSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "RogueHUDPrivate.h"

void ARogueHUD::RequestBackFromSettingsMenu()
{
	if (bMainMenuMode)
	{
		CloseMainMenuSettings();
		return;
	}
	PauseMenuPage = ERoguePauseMenuPage::Main;
}

void ARogueHUD::RequestApplyFromSettingsMenu()
{
	ApplyDisplaySettings();
}

void ARogueHUD::RequestVolumeAdjustFromSettingsMenu(bool bIncrease)
{
	const float Direction = bIncrease ? 1.0f : -1.0f;
	if (auto *Audio = GetGameInstance()->GetSubsystem<URogueMusicSubsystem>())
	{
		Audio->SetSoundEffectsVolume(Audio->GetSoundEffectsVolume() + Direction * .1f);
		RefreshAudioVolumes();
	}
}

void ARogueHUD::RequestMusicVolumeAdjustFromSettingsMenu(bool bIncrease)
{
	if (auto *Audio = GetGameInstance()->GetSubsystem<URogueMusicSubsystem>())
	{
		Audio->SetMusicVolume(Audio->GetMusicVolume() + (bIncrease ? .1f : -.1f));
		RefreshAudioVolumes();
	}
}

void ARogueHUD::RequestQualityAdjustFromSettingsMenu(bool bIncrease)
{
	GraphicsQualityLevel = FMath::Clamp(GraphicsQualityLevel + (bIncrease ? 1 : -1), 0, 4);
	bGraphicsQualityCustom = false;
	bDisplaySettingsDirty = true;
}

void ARogueHUD::RequestResolutionAdjustFromSettingsMenu(bool bIncrease)
{
	StepResolution(bIncrease ? 1 : -1);
	bDisplaySettingsDirty = true;
}

void ARogueHUD::RequestFrameLimitAdjustFromSettingsMenu(bool bIncrease)
{
	StepFrameRateLimit(bIncrease ? 1 : -1);
	bDisplaySettingsDirty = true;
}

void ARogueHUD::RequestToggleDisplayModeFromSettingsMenu()
{
	CycleDisplayMode();
	bDisplaySettingsDirty = true;
}

void ARogueHUD::UpdateSettingsWidget()
{
	if (SettingsMenuWidget == nullptr)
	{
		return;
	}

	const ARogueCharacter *PlayerCharacter = RogueHUDPrivate::GetLocalPlayerCharacter(this);
	const ARogueGameState *RogueGameState = GetRogueGameState();
	const bool bVisible = bMainMenuMode ? bMainMenuSettingsOpen
										: (PauseMenuPage == ERoguePauseMenuPage::Settings &&
										   PlayerCharacter != nullptr && PlayerCharacter->IsAlive() &&
										   !RogueHUDPrivate::IsRunOver(RogueGameState, PlayerCharacter));
	FString Signature = bVisible ? TEXT("Settings|Visible") : TEXT("Settings|Hidden");
	if (bVisible)
	{
		Signature += FString::Printf(
			TEXT("|SFX=%.2f|Music=%.2f|Quality=%d|Res=%d|Mode=%d|Frame=%d|FrameValue=%.1f|QualityCustom=%d|"
				 "FrameCustom=%d|Dirty=%d"),
			SoundEffectsVolume, MusicVolume, GraphicsQualityLevel, ResolutionOptionIndex,
			static_cast<int32>(DisplayWindowMode), FrameRateLimitOptionIndex, FrameRateLimitValue,
			bGraphicsQualityCustom ? 1 : 0, bFrameRateLimitCustom ? 1 : 0, bDisplaySettingsDirty ? 1 : 0);
	}

	if (!RogueHUDPrivate::UpdateSignature(LastSettingsWidgetSignature, Signature))
	{
		return;
	}

	FRogueSettingsMenuViewData ViewData;
	ViewData.bVisible = bVisible;
	ViewData.TitleText = FText::FromString(TEXT("设置"));
	ViewData.bCanApply = bDisplaySettingsDirty;
	ViewData.StatusText =
		FText::FromString(bDisplaySettingsDirty ? TEXT("音量即时保存；画面修改需点击“应用设置”")
												: TEXT("音量即时生效并保存；当前画面设置已应用"));

	const TArray<FString> RowLabels = {TEXT("音效音量"), TEXT("画质等级"), TEXT("分辨率"),
									   TEXT("显示模式"), TEXT("限制帧率"), TEXT("背景音乐音量")};

	const TArray<FString> RowValues = {
		FString::Printf(TEXT("%d%%"), FMath::RoundToInt(SoundEffectsVolume * 100.0f)),
		GetGraphicsQualityLabel(),
		GetResolutionLabel(),
		GetDisplayModeLabel(),
		GetFrameRateLimitLabel(),
		FString::Printf(TEXT("%d%%"), FMath::RoundToInt(MusicVolume * 100.0f))};

	for (int32 RowIndex = 0; RowIndex < RowLabels.Num() && RowIndex < RowValues.Num(); ++RowIndex)
	{
		FRogueSettingsRowViewData &Row = ViewData.Rows.AddDefaulted_GetRef();
		Row.LabelText = FText::FromString(RowLabels[RowIndex]);
		Row.ValueText = FText::FromString(RowValues[RowIndex]);
		Row.bUseAdjustButtons = RowIndex != 3;
	}

	SettingsMenuWidget->UpdateSettingsView(ViewData);
}

void ARogueHUD::RefreshPauseSettings()
{
	if (GEngine == nullptr)
	{
		return;
	}

	if (UGameUserSettings *UserSettings = GEngine->GetGameUserSettings())
	{
		UserSettings->LoadSettings(false);
		GraphicsQualityLevel = ResolveGraphicsQualityLevel(UserSettings);
		DisplayWindowMode = UserSettings->GetFullscreenMode();
		DesktopResolution = UserSettings->GetDesktopResolution();
		RefreshResolutionOptions(UserSettings->GetScreenResolution(), DesktopResolution);
		RefreshFrameRateLimitOptions(UserSettings->GetFrameRateLimit());
		bDisplaySettingsDirty = false;
	}

	RefreshAudioVolumes();
}

void ARogueHUD::RefreshResolutionOptions(const FIntPoint &CurrentResolution,
										 const FIntPoint &InDesktopResolution)
{
	DesktopResolution =
		InDesktopResolution.X > 0 && InDesktopResolution.Y > 0 ? InDesktopResolution : FIntPoint(1920, 1080);
	ResolutionOptions.Reset();

	const auto AddResolution = [this](const FIntPoint &Resolution) {
		if (Resolution.X > 0 && Resolution.Y > 0 && !ResolutionOptions.Contains(Resolution))
		{
			ResolutionOptions.Add(Resolution);
		}
	};

	const auto AddFilteredResolution = [this, &AddResolution](const FIntPoint &Resolution) {
		if (Resolution.X <= DesktopResolution.X && Resolution.Y <= DesktopResolution.Y)
		{
			AddResolution(Resolution);
		}
	};

	AddResolution(CurrentResolution);
	AddFilteredResolution(FIntPoint(960, 540));
	AddFilteredResolution(FIntPoint(1280, 720));
	AddFilteredResolution(FIntPoint(1366, 768));
	AddFilteredResolution(FIntPoint(1600, 900));
	AddFilteredResolution(FIntPoint(1920, 1080));
	AddFilteredResolution(FIntPoint(2560, 1440));
	AddFilteredResolution(FIntPoint(3840, 2160));
	AddResolution(DesktopResolution);

	ResolutionOptions.Sort(
		[](const FIntPoint &Left, const FIntPoint &Right) { return Left.X * Left.Y < Right.X * Right.Y; });

	if (ResolutionOptions.Num() == 0)
	{
		ResolutionOptions.Add(DesktopResolution);
	}

	ResolutionOptionIndex = ResolutionOptions.IndexOfByKey(CurrentResolution);
	if (ResolutionOptionIndex == INDEX_NONE)
	{
		ResolutionOptionIndex = ResolutionOptions.IndexOfByKey(DesktopResolution);
	}
	if (ResolutionOptionIndex == INDEX_NONE)
	{
		ResolutionOptionIndex = 0;
	}
}

void ARogueHUD::StepResolution(int32 Direction)
{
	if (ResolutionOptions.Num() == 0)
	{
		return;
	}

	ResolutionOptionIndex =
		(ResolutionOptionIndex + Direction + ResolutionOptions.Num()) % ResolutionOptions.Num();
}

void ARogueHUD::RefreshFrameRateLimitOptions(float CurrentFrameRateLimit)
{
	if (FrameRateLimitOptions.Num() == 0)
	{
		FrameRateLimitOptions = {0.0f, 30.0f, 60.0f, 90.0f, 120.0f, 144.0f, 165.0f, 240.0f};
	}

	FrameRateLimitValue = FMath::Max(0.0f, CurrentFrameRateLimit);
	if (FrameRateLimitValue <= KINDA_SMALL_NUMBER)
	{
		FrameRateLimitOptionIndex = 0;
		bFrameRateLimitCustom = false;
		return;
	}

	const int32 ExactIndex = FrameRateLimitOptions.IndexOfByPredicate(
		[this](float Option) { return FMath::IsNearlyEqual(Option, FrameRateLimitValue, 0.5f); });

	if (ExactIndex != INDEX_NONE)
	{
		FrameRateLimitOptionIndex = ExactIndex;
		bFrameRateLimitCustom = false;
		return;
	}

	float BestDifference = FLT_MAX;
	int32 BestIndex = 0;
	for (int32 Index = 0; Index < FrameRateLimitOptions.Num(); ++Index)
	{
		const float Difference = FMath::Abs(FrameRateLimitOptions[Index] - FrameRateLimitValue);
		if (Difference < BestDifference)
		{
			BestDifference = Difference;
			BestIndex = Index;
		}
	}

	FrameRateLimitOptionIndex = BestIndex;
	bFrameRateLimitCustom = true;
}

void ARogueHUD::StepFrameRateLimit(int32 Direction)
{
	if (FrameRateLimitOptions.Num() == 0)
	{
		RefreshFrameRateLimitOptions(FrameRateLimitValue);
	}

	if (FrameRateLimitOptions.Num() == 0)
	{
		return;
	}

	FrameRateLimitOptionIndex =
		(FrameRateLimitOptionIndex + Direction + FrameRateLimitOptions.Num()) % FrameRateLimitOptions.Num();
	FrameRateLimitValue = FrameRateLimitOptions[FrameRateLimitOptionIndex];
	bFrameRateLimitCustom = false;
}

void ARogueHUD::CycleDisplayMode()
{
	switch (DisplayWindowMode)
	{
	case EWindowMode::Windowed:
		DisplayWindowMode = EWindowMode::Fullscreen;
		break;
	case EWindowMode::Fullscreen:
		DisplayWindowMode = EWindowMode::WindowedFullscreen;
		break;
	default:
		DisplayWindowMode = EWindowMode::Windowed;
		break;
	}
}

void ARogueHUD::RefreshAudioVolumes()
{
	if (UGameInstance *Instance = GetGameInstance())
		if (auto *Audio = Instance->GetSubsystem<URogueMusicSubsystem>())
		{
			SoundEffectsVolume = Audio->GetSoundEffectsVolume();
			MusicVolume = Audio->GetMusicVolume();
		}
}

void ARogueHUD::ApplyDisplaySettings()
{
	if (GEngine == nullptr)
	{
		return;
	}

	if (UGameUserSettings *UserSettings = GEngine->GetGameUserSettings())
	{
		UserSettings->SetOverallScalabilityLevel(GraphicsQualityLevel);
		if (ResolutionOptions.IsValidIndex(ResolutionOptionIndex))
		{
			UserSettings->SetScreenResolution(ResolutionOptions[ResolutionOptionIndex]);
		}
		UserSettings->SetFullscreenMode(DisplayWindowMode);
		UserSettings->SetFrameRateLimit(FrameRateLimitValue);
		UserSettings->ApplySettings(false);
		UserSettings->ConfirmVideoMode();
		UserSettings->SaveSettings();

		const FString FrameLimitCommand = FrameRateLimitValue <= KINDA_SMALL_NUMBER
											  ? TEXT("t.MaxFPS 0")
											  : FString::Printf(TEXT("t.MaxFPS %.0f"), FrameRateLimitValue);
		UKismetSystemLibrary::ExecuteConsoleCommand(this, FrameLimitCommand, GetOwningPlayerController());

		RefreshPauseSettings();
	}
}

int32 ARogueHUD::ResolveGraphicsQualityLevel(const UGameUserSettings *UserSettings)
{
	if (UserSettings == nullptr)
	{
		bGraphicsQualityCustom = false;
		return 2;
	}

	const int32 OverallQualityLevel = UserSettings->GetOverallScalabilityLevel();
	if (OverallQualityLevel >= 0)
	{
		bGraphicsQualityCustom = false;
		return FMath::Clamp(OverallQualityLevel, 0, 4);
	}

	const TArray<int32> QualityLevels = {
		UserSettings->GetViewDistanceQuality(), UserSettings->GetAntiAliasingQuality(),
		UserSettings->GetShadowQuality(),		UserSettings->GetGlobalIlluminationQuality(),
		UserSettings->GetReflectionQuality(),	UserSettings->GetPostProcessingQuality(),
		UserSettings->GetTextureQuality(),		UserSettings->GetVisualEffectQuality(),
		UserSettings->GetFoliageQuality(),		UserSettings->GetShadingQuality()};

	int32 QualitySum = 0;
	int32 QualityCount = 0;
	for (const int32 QualityLevel : QualityLevels)
	{
		if (QualityLevel >= 0)
		{
			QualitySum += FMath::Clamp(QualityLevel, 0, 4);
			++QualityCount;
		}
	}

	bGraphicsQualityCustom = true;
	return QualityCount > 0
			   ? FMath::Clamp(FMath::RoundToInt(static_cast<float>(QualitySum) / QualityCount), 0, 4)
			   : 2;
}

FString ARogueHUD::GetGraphicsQualityLabel() const
{
	static const TCHAR *Labels[] = {TEXT("低"), TEXT("中"), TEXT("高"), TEXT("史诗"), TEXT("电影")};

	const FString BaseLabel = Labels[FMath::Clamp(GraphicsQualityLevel, 0, 4)];
	return bGraphicsQualityCustom ? FString::Printf(TEXT("自定义(%s)"), *BaseLabel) : BaseLabel;
}

FString ARogueHUD::GetResolutionLabel() const
{
	if (!ResolutionOptions.IsValidIndex(ResolutionOptionIndex))
	{
		return TEXT("未设置");
	}

	const FIntPoint Resolution = ResolutionOptions[ResolutionOptionIndex];
	return FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
}

FString ARogueHUD::GetDisplayModeLabel() const
{
	switch (DisplayWindowMode)
	{
	case EWindowMode::Fullscreen:
		return TEXT("全屏");
	case EWindowMode::WindowedFullscreen:
		return TEXT("全屏窗口");
	default:
		return TEXT("窗口");
	}
}

FString ARogueHUD::GetFrameRateLimitLabel() const
{
	if (FrameRateLimitValue <= KINDA_SMALL_NUMBER)
	{
		return TEXT("无限制");
	}

	const FString BaseLabel = FString::Printf(TEXT("%.0f FPS"), FrameRateLimitValue);
	return bFrameRateLimitCustom ? FString::Printf(TEXT("自定义(%s)"), *BaseLabel) : BaseLabel;
}
