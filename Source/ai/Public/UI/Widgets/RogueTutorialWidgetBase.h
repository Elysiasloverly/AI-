#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RogueTutorialWidgetBase.generated.h"

class ARogueHUD;
class UButton;
class UWidgetSwitcher;

/**
 * 教程/背景说明 WBP 基类：C++ 只处理章节切换和“返回”按钮，正文、排版、滚动方式都在 WBP_Tutorial 里改。
 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueTutorialWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
    UFUNCTION(BlueprintCallable, Category="Rogue|Tutorial") void SelectChapter(int32 Index);

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION() void HandleCloseClicked();

	/** 与 WBP 里的按钮同名；没有也能显示，只是只能靠主菜单再点一次关闭。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidgetSwitcher> ChapterSwitcher;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Chapter0Button;
    UFUNCTION() void Chapter0Clicked() { SelectChapter(0); }
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Chapter1Button;
    UFUNCTION() void Chapter1Clicked() { SelectChapter(1); }
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Chapter2Button;
    UFUNCTION() void Chapter2Clicked() { SelectChapter(2); }
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Chapter3Button;
    UFUNCTION() void Chapter3Clicked() { SelectChapter(3); }
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Chapter4Button;
    UFUNCTION() void Chapter4Clicked() { SelectChapter(4); }
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Chapter5Button;
    UFUNCTION() void Chapter5Clicked() { SelectChapter(5); }

    TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};
