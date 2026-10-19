#pragma once

#include "CoreMinimal.h"

class ARogueCharacter;
class APlayerController;
class UWorld;

/**
 * 常用的服务端玩家查询工具，统一复用 Pawn::IsAlive 存活口径。
 *
 * 通过当前世界的 PlayerController 迭代收集 Pawn：服务端可见全部控制器，
 * 客户端通常只有本地控制器，因此不能用它在客户端枚举所有远端玩家。
 * 距离查询使用 2D；TInlineAllocator<4> 内联容纳四个元素，超出后可分配堆内存，
 * 并不限制联机人数。团灭判定仍需额外处理观察者和尚无 Pawn 的参与者。
 */
namespace RoguePlayerQuery
{
	/** 该 PC 当前操控的玩家角色；没有、或不是玩家角色时为 nullptr。 */
	AI_API ARogueCharacter* GetCharacterOf(const APlayerController* PlayerController);

	/** 收集所有**存活**玩家角色（顺序 = PlayerController 迭代顺序）。 */
	AI_API void CollectAlivePlayers(const UWorld* World, TArray<ARogueCharacter*, TInlineAllocator<4>>& OutPlayers);

	/** 离 Location 最近的存活玩家（2D 距离）；一个都没有时返回 nullptr。 */
	AI_API ARogueCharacter* FindNearestAlivePlayer(const UWorld* World, const FVector& Location);

	/** 是否有活着的玩家落在 Location 的 MinDistance 之内（2D 距离）。 */
	AI_API bool IsAnyAlivePlayerWithin(const UWorld* World, const FVector& Location, float MinDistance);
}
