#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RogueMainMenuWidgetBase.generated.h"

class ARogueHUD;
class UButton;
class UTextBlock;

/**
 * 开始菜单基类。
 *
 * 分工：C++ 只按名字绑定 6 个按钮并转交 HUD；配色、字体、排版、动画都在 WBP_MainMenu 里改。
 * 按钮改名字会静默失效（BindWidgetOptional），改样式不会。
 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueMainMenuWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 与其它菜单基类保持同一个注入接口，HUD 的 CreateRogueMenuWidget 会调用。 */
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);

	/** 主菜单操作提示文字，同时广播给蓝图做动效。 */
	void SetHint(const FText& InHint);

	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|MainMenu")
	void OnHintChanged(const FText& InHint);

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION() void HandleSinglePlayerClicked();
	UFUNCTION() void HandleMultiplayerClicked();
	UFUNCTION() void HandleSkillTreeClicked();
	UFUNCTION() void HandleTutorialClicked();
	UFUNCTION() void HandleSettingsClicked();
	UFUNCTION() void HandleQuitClicked();

	/** 按钮名与 WBP 里的控件名一致；可选绑定，缺一个不会导致整个 WBP 失效。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SinglePlayerButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MultiplayerButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SkillTreeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TutorialButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;

	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};
