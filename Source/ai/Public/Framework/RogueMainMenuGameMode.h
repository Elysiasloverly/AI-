#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RogueMainMenuGameMode.generated.h"

/**
 * 主菜单关卡 GameMode：无 Pawn、无 HUD 战斗逻辑，只负责把 ARogueMainMenuHUD 挂上去。
 * 关卡本身在 /Game/Map/MainMenu，World Settings 里指定本类。
 */
UCLASS()
class AI_API ARogueMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARogueMainMenuGameMode();
};
