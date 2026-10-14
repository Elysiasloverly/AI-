#include "Combat/Visuals/RogueCombatFeedback.h"

#include "UI/RogueHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace RogueCombatFeedback
{
	void AddDamageNumber(AActor* WorldContextActor, const FVector& WorldLocation, float Damage, bool bIsBossDamage)
	{
		if (!IsValid(WorldContextActor) || Damage <= 0.0f)
		{
			return;
		}

		UWorld* World = WorldContextActor->GetWorld();
		if (World == nullptr)
		{
			return;
		}

		// 给**本机的每个本地玩家**各加一条飘字。
		//
		// 为什么不能用 UGameplayStatics::GetPlayerController(WorldContextActor, 0)：
		// 索引 0 是"这个进程里的第一个玩家控制器"。PIE 的 2 人模式（Run Under One Process）
		// 与分屏下两个玩家共用一个进程 —— 于是**客户端算出来的飘字会被加到主机的 HUD 上**，
		// 表现就是"主机上飘字特别多、客户端一条都没有"。
		//
		// 遍历 + IsLocalController() 对三种情况都正确：单机（一个）、分屏（两个都该看到）、
		// 同进程 PIE（两个都该看到）；而真正的联机（各自独立进程）每台机器只有一个本地玩家。
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PlayerController = It->Get();
			if (PlayerController == nullptr || !PlayerController->IsLocalController())
			{
				continue;
			}

			if (ARogueHUD* RogueHUD = Cast<ARogueHUD>(PlayerController->GetHUD()))
			{
				RogueHUD->AddDamageNumber(WorldLocation, Damage, bIsBossDamage);
			}
		}
	}
}
