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

		// 遍历当前 World 中的本地控制器，让分屏的各本地 HUD 都收到飘字。
		// 同进程 PIE 可有多个独立 World；此迭代器不会跨 World 路由到其他 PIE 窗口。
		// 独立进程同样可能有多个本地玩家，不能假定只有索引 0。
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

	void AddHealingNumber(AActor* WorldContextActor, const FVector& WorldLocation, float Amount)
	{
		if (!IsValid(WorldContextActor) || Amount <= 0.0f)
		{
			return;
		}

		UWorld* World = WorldContextActor->GetWorld();
		if (World == nullptr)
		{
			return;
		}

		// 与伤害飘字一致：分屏下每个本地 HUD 都显示。
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PlayerController = It->Get();
			if (PlayerController == nullptr || !PlayerController->IsLocalController())
			{
				continue;
			}

			if (ARogueHUD* RogueHUD = Cast<ARogueHUD>(PlayerController->GetHUD()))
			{
				RogueHUD->AddHealingNumber(WorldLocation, Amount);
			}
		}
	}
}
