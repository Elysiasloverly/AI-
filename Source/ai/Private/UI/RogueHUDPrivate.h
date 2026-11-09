#pragma once
#include "Player/RogueCharacter.h"
#include "UI/RogueHUD.h"
#include "GameFramework/PlayerController.h"
#include "Framework/RogueGameState.h"

// 小型、无状态的界面刷新规则；每个 HUD 的缓存仍保存在自己的实例里。
namespace RogueHUDPrivate
{
// 使用所属控制器，避免分屏时固定索引0导致HUD读到另一位玩家。
inline ARogueCharacter *GetLocalPlayerCharacter(const AHUD *HUD)
{
	APlayerController *Controller = HUD ? HUD->GetOwningPlayerController() : nullptr;
	return Controller ? Cast<ARogueCharacter>(Controller->GetPawn()) : nullptr;
}

inline bool IsRunOver(const ARogueGameState *State, const ARogueCharacter *Player)
{
	return (State && State->IsGameOver()) || (Player && Player->IsEliminated());
}
inline bool UpdateSignature(FString &Cached, const FString &Value)
{
	if (Cached == Value)
		return false;
	Cached = Value;
	return true;
}
inline FText AutoRefreshText(int32 Seconds)
{
	return FText::FromString(FString::Printf(TEXT("自动补货 %d 秒"), FMath::Max(0, Seconds)));
}
} // namespace RogueHUDPrivate
