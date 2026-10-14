#include "UI/Widgets/RogueDeathScreenWidgetBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UI/RogueHUD.h"

#include "RogueMenuWidgetHelpers.h"

void URogueDeathScreenWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueDeathScreenWidgetBase::UpdateDeathView(const FRogueDeathViewData& InViewData)
{
	OnDeathViewUpdated(InViewData);
	if (!InViewData.bVisible)
	{
		StopAllAnimations();
	}
	SetVisibility(RogueMenuWidgetHelpers::GetMenuVisibility(InViewData.bVisible));

	// 死亡界面**绝不能暂停世界** —— 联机下那会把队友一起冻住。
	//
	// 暂停不在这里、也不在任何 C++ 里：实测它来自这个界面的**蓝图**
	// （WBP 在 OnDeathViewUpdated 的实现里调了 Set Game Paused，是单机时代的写法）。
	// C++ 够不到蓝图节点，所以只能在蓝图回调**之后**立刻纠偏 ——
	// 顺序很关键：必须放在 OnDeathViewUpdated 后面，否则会被蓝图又暂停回去。
	// 主机侧这一下尤其重要：主机是服务端，它的世界一暂停就是"整局停住"。
	//
	// 正确做法仍然是把蓝图里那个 Set Game Paused 节点删掉，这里只是兜底。
	if (UWorld* World = GetWorld())
	{
		if (World->IsPaused())
		{
			UGameplayStatics::SetGamePaused(this, false);
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 死亡界面试图暂停世界：已立即恢复（请删除死亡界面蓝图里的 Set Game Paused 节点）"));
		}
	}
}

void URogueDeathScreenWidgetBase::RequestRestartRound()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestRestartAfterDeath();
	}
}

void URogueDeathScreenWidgetBase::RequestQuitGame()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestQuitAfterDeath();
	}
}
