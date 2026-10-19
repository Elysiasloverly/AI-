#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RoguePlayerController.generated.h"

/**
 * 保存最近解除控制的 Pawn 弱引用，供 GameMode 的归属修复查找未被控制的旧 Pawn。
 * OnPossess 当前只转调父类，没有调用栈审计或蓝图调用来源判断。
 * 记录历史控制关系不等于网络所有权校验，候选仍需由修复逻辑判断是否适合重新接管。
 */
UCLASS()
class AI_API ARoguePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** 从最近的历史记录中返回仍有效、且当前无控制器的 Pawn；没有则返回 nullptr。 */
	APawn* GetLastUnpossessedPawn() const;

private:
	/** 最近至多四个解除控制的 Pawn，新的在后；弱引用不延长 Pawn 生命周期。 */
	TArray<TWeakObjectPtr<APawn>> PreviouslyOwnedPawns;
};
