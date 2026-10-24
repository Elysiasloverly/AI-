#include "UI/Widgets/RogueMainMenuWidgetBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/RogueHUD.h"

void URogueMainMenuWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueMainMenuWidgetBase::SetHint(const FText& InHint)
{
	if (HintText != nullptr)
	{
		HintText->SetText(InHint);
	}
	OnHintChanged(InHint);
}

void URogueMainMenuWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (SinglePlayerButton != nullptr)
	{
		SinglePlayerButton->OnClicked.AddUniqueDynamic(this, &URogueMainMenuWidgetBase::HandleSinglePlayerClicked);
	}
	if (MultiplayerButton != nullptr)
	{
		MultiplayerButton->OnClicked.AddUniqueDynamic(this, &URogueMainMenuWidgetBase::HandleMultiplayerClicked);
	}
	if (SkillTreeButton != nullptr)
	{
		SkillTreeButton->OnClicked.AddUniqueDynamic(this, &URogueMainMenuWidgetBase::HandleSkillTreeClicked);
	}
	if (TutorialButton != nullptr)
	{
		TutorialButton->OnClicked.AddUniqueDynamic(this, &URogueMainMenuWidgetBase::HandleTutorialClicked);
	}
	if (SettingsButton != nullptr)
	{
		SettingsButton->OnClicked.AddUniqueDynamic(this, &URogueMainMenuWidgetBase::HandleSettingsClicked);
	}
	if (QuitButton != nullptr)
	{
		QuitButton->OnClicked.AddUniqueDynamic(this, &URogueMainMenuWidgetBase::HandleQuitClicked);
	}

	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单按钮绑定：单人=%d 多人=%d 技能树=%d 教程=%d 设置=%d 退出=%d 提示=%d"),
		SinglePlayerButton != nullptr ? 1 : 0,
		MultiplayerButton != nullptr ? 1 : 0,
		SkillTreeButton != nullptr ? 1 : 0,
		TutorialButton != nullptr ? 1 : 0,
		SettingsButton != nullptr ? 1 : 0,
		QuitButton != nullptr ? 1 : 0,
		HintText != nullptr ? 1 : 0);
}

void URogueMainMenuWidgetBase::HandleSinglePlayerClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单按钮：单人游戏 "));
	if (ARogueHUD* HUD = OwningRogueHUD.Get())
	{
		HUD->StartSinglePlayerGame();
	}
}

void URogueMainMenuWidgetBase::HandleMultiplayerClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单按钮：多人游戏 "));
	if (ARogueHUD* HUD = OwningRogueHUD.Get())
	{
		HUD->OpenMultiplayerMenu();
	}
}

void URogueMainMenuWidgetBase::HandleSkillTreeClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单按钮：技能树 "));
	if (ARogueHUD* HUD = OwningRogueHUD.Get())
	{
		HUD->OpenUpgradeTree();
	}
}

void URogueMainMenuWidgetBase::HandleTutorialClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单按钮：教程 "));
	if (ARogueHUD* HUD = OwningRogueHUD.Get())
	{
		HUD->OpenMainMenuTutorial();
	}
}

void URogueMainMenuWidgetBase::HandleSettingsClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单按钮：设置 "));
	if (ARogueHUD* HUD = OwningRogueHUD.Get())
	{
		HUD->OpenMainMenuSettings();
	}
}

void URogueMainMenuWidgetBase::HandleQuitClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单按钮：退出游戏 "));
	if (ARogueHUD* HUD = OwningRogueHUD.Get())
	{
		HUD->QuitToDesktop();
	}
}
