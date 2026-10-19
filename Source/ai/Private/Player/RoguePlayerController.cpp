#include "Player/RoguePlayerController.h"

void ARoguePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void ARoguePlayerController::OnUnPossess()
{
	// 记下来：归属自愈时优先把"曾经属于我、现在没人控制"的 Pawn 认领回来。
	// 保留多个历史候选，避免仅能查到最近解除控制、但已被其他控制器持有的 Pawn。
	if (APawn* CurrentPawn = GetPawn())
	{
		PreviouslyOwnedPawns.RemoveAll([CurrentPawn](const TWeakObjectPtr<APawn>& Entry) { return Entry.Get() == CurrentPawn; });
		PreviouslyOwnedPawns.Add(CurrentPawn);
		while (PreviouslyOwnedPawns.Num() > 4)
		{
			PreviouslyOwnedPawns.RemoveAt(0);
		}
	}

	Super::OnUnPossess();
}

APawn* ARoguePlayerController::GetLastUnpossessedPawn() const
{
	// 从最新往旧找：第一个"还有效且当前没有控制器"的就是可以被认领回来的那个
	for (int32 Index = PreviouslyOwnedPawns.Num() - 1; Index >= 0; --Index)
	{
		APawn* Candidate = PreviouslyOwnedPawns[Index].Get();
		if (IsValid(Candidate) && Candidate->GetController() == nullptr)
		{
			return Candidate;
		}
	}

	return nullptr;
}
