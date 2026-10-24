#include "Framework/RogueMainMenuGameMode.h"

#include "GameFramework/PlayerController.h"
#include "UI/RogueMainMenuHUD.h"

ARogueMainMenuGameMode::ARogueMainMenuGameMode()
{
	HUDClass = ARogueMainMenuHUD::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();

	// 菜单关卡不需要角色；没有 Pawn 时 OpenLevel / 退出都能正常工作。
	DefaultPawnClass = nullptr;
	bStartPlayersAsSpectators = false;
}
