#pragma once

#include "CoreMinimal.h"
#include "UI/RogueHUD.h"
#include "RogueMainMenuHUD.generated.h"

/**
 * 主菜单 HUD：继承 ARogueHUD 以复用设置面板，但把 HUD 切到“主菜单模式”。
 * 只创建 WBP_MainMenu / WBP_Tutorial / WBP_SettingsMenu，不创建血条、伤害数字等战斗界面。
 */
UCLASS()
class AI_API ARogueMainMenuHUD : public ARogueHUD
{
	GENERATED_BODY()

public:
	ARogueMainMenuHUD();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	/** 把玩家视角切到带 MenuCamera 标签的相机，让 UMG 后面的 3D 背景可见。 */
	void ApplyMenuCamera();
	bool bMenuCameraApplied = false;
};
