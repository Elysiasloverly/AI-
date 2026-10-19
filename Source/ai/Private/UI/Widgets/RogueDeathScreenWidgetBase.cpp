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

	// 蓝图回调结束后检查当前世界是否暂停，并尝试恢复，避免监听服停止整局推进。
	// 这里只观察暂停结果，无法确定是哪个节点造成；客户端也不能据此保证解除服务器暂停。
	// 该兜底不代替检查实际 WBP 图表中的暂停调用。
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
