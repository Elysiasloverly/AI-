#pragma once

#include "CoreMinimal.h"

class ARogueCharacter;
class APlayerController;
class UWorld;

/**
 * 玩家查询的**唯一入口**。
 *
 * 为什么需要它：迁移期与联机修复期里，"遍历 PlayerController → 取 Pawn → 判 IsValid + 未倒地"
 * 这一段在四个地方各写了一遍（GameMode 的存活列表、敌人索敌、刷怪点检查、经验球磁吸）。
 * 这种重复的口径一旦分叉，就会出现"某个系统仍然把倒地的人当活人"这类只有联机才暴露的 bug
 * （实测就发生过：玩家倒地被队友看着，怪物还在朝他开火）。
 * 统一到这里之后，全项目只有一份"谁算活着"的定义。
 *
 * 约定：
 *   · 一律用 **Pawn 上的 IsDead()** 判存活（它现在是从服务端复制的权威位）；
 *   · 距离一律用 2D（俯视角游戏里 Z 轴不参与判断）；
 *   · 容器一律 TInlineAllocator<4>（联机上限 4 人，避免堆分配）。
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
