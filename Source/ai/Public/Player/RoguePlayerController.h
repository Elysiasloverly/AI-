#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RoguePlayerController.generated.h"

/**
 * 玩家控制器 —— 目前**只做一件事**：审计"谁在什么时候接管了哪个 Pawn"。
 *
 * 为什么需要它：实测发现联机时客户端加入的瞬间，**客户端的 Pawn 会被主机的 PlayerController
 * 先接管一次**（随后才纠正），结果主机自己的 Pawn 被挤掉控制器（`Ctrl=None`），表现为
 * "主机切到固定视角 + UI 消失 + 操控不了"。而项目 C++ 里搜不到任何 Possess/UnPossess/
 * RestartPlayer 调用，接管只可能来自蓝图 —— 但蓝图节点藏在哪里用肉眼很难找。
 *
 * 办法：在 OnPossess 里打印一次**原生调用栈**（FDebug::DumpStackTraceToLog）。
 * 调用栈里如果出现 ProcessEvent / ExecuteUbergraph / BP_xxx，就证明是蓝图发起的，且能看出是哪个函数；
 * 如果是引擎的 RestartPlayer 流程，也会直接显示出来。
 *
 * 顺手把所有权的异常情形显式喊出来：接管了一个"已经被别的 PC 占着"的 Pawn。
 */
UCLASS()
class AI_API ARoguePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** 上一次被解除接管的 Pawn。
	 *  引擎登录流程会把在场 PC 的 Pawn 抢走再还回来（时序见 ARogueGameMode::PostLogin 的注释），
	 *  这里记一下，好在归属自愈时直接认领回来，而不是重新生成一个新角色
	 *  （否则旧角色会变成没人控制的孤儿留在场上）。 */
	APawn* GetLastUnpossessedPawn() const;

private:
	/** 曾经拥有过、后来被解除接管的 Pawn（新的在后）。
	 *  为什么不只记一个：引擎抢 Pawn 的时序是"C_0 被放开 → 抢到 C_1 → 放开 C_1 → C_1 归新 PC"，
	 *  所以最后一次放手的其实是**别人的** Pawn，只记最后一个会认领失败。 */
	TArray<TWeakObjectPtr<APawn>> PreviouslyOwnedPawns;
};
