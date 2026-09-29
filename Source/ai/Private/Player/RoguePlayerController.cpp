#include "Player/RoguePlayerController.h"

void ARoguePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void ARoguePlayerController::OnUnPossess()
{
	// 记下来：归属自愈时优先把"曾经属于我、现在没人控制"的 Pawn 认领回来。
	// 存成列表而不是单个：引擎抢 Pawn 的时序里，最后一次放手的可能是别人的 Pawn。
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
