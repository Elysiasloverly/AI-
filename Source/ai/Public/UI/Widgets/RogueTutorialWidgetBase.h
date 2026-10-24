#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RogueTutorialWidgetBase.generated.h"

class ARogueHUD;
class UButton;

/**
 * 教程/背景说明 WBP 基类：C++ 只处理“返回”按钮，正文、排版、滚动方式都在 WBP_Tutorial 里改。
 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueTutorialWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION() void HandleCloseClicked();

	/** 与 WBP 里的按钮同名；没有也能显示，只是只能靠主菜单再点一次关闭。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};
