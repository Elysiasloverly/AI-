#include "Framework/RogueGameMode.h"
#include "Debug/RogueSelfTest.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "Shared/RogueTypes.h"
#include "AbilitySystem/RogueAttributeSet.h"

#include "World/RogueArena.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Player/RoguePlayerController.h"
#include "EngineUtils.h"
#include "Enemies/RogueEnemy.h"
#include "World/RogueExperiencePickup.h"
#include "World/RogueShopTerminal.h"
#include "UI/RogueHUD.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Progression/Upgrades/RogueUpgradeDefinitionAsset.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Framework/RogueGameModeRules.h"
#include "Framework/RogueGameState.h"
#include "Player/RoguePlayerQuery.h"
#include "Progression/Upgrades/RogueUpgradeSystem.h"
#include "Combat/Projectiles/RogueMortarProjectile.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Engine/DataTable.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "World/RogueSceneSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"

void ARogueGameMode::AdvanceShopAutoRefresh(float DeltaSeconds, bool bWorldPaused)
{
	// 此入口在世界暂停时不推进商店剩余时间；菜单开关本身不暂停世界。
	if (bWorldPaused)
	{
		return;
	}

	// 每个玩家推进自己的商店（每玩家一份运行态）
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PlayerController = It->Get())
		{
			if (ARoguePlayerState* RoguePlayerState = PlayerController->GetPlayerState<ARoguePlayerState>())
			{
				// 用**该玩家自己的 Pawn**：商店报价要按这个玩家的构筑来算
				RoguePlayerState->GetShopRuntime().AdvanceAutoRefresh(
					DeltaSeconds,
					Cast<ARogueCharacter>(PlayerController->GetPawn()),
					UpgradeSystem);
			}
		}
	}
}

void ARogueGameMode::HandleEnemyKilled(ARogueEnemy* Enemy, AActor* Killer)
{
	if (IsValid(Enemy) && Enemy->IsBoss() && RewardedBossSpawnIds.Contains(Enemy->GetSpawnCycleId())) { return; }
	++RunState.EnemiesDefeated;

	URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	URogueCombatPoolSubsystem* Pools = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>();

	if (Tracker != nullptr)
	{
		Tracker->UnregisterEnemy(Enemy);
	}

	if (IsValid(Enemy))
	{
		AwardEnemyCurrency(Enemy, ResolveRewardCharacter(Killer, Enemy));
		SpawnExperienceReward(Enemy, Tracker, Pools);

		if (Enemy->IsBoss())
		{
			HandleBossKilledReward(Enemy->GetSpawnCycleId());
		}
	}
}

ARogueCharacter* ARogueGameMode::ResolveRewardCharacter(AActor* Killer, const AActor* RewardOrigin) const
{
	// 1) 击杀者本身就是玩家
	ARogueCharacter* RewardCharacter = Cast<ARogueCharacter>(Killer);
	if (RewardCharacter == nullptr && IsValid(Killer))
	{
		// 2) 击杀者是弹体 / 刀片 / 地狱塔这类"玩家的延伸物" → 顺着 Owner / Instigator 找回玩家
		RewardCharacter = Cast<ARogueCharacter>(Killer->GetOwner());
		if (RewardCharacter == nullptr)
		{
			RewardCharacter = Cast<ARogueCharacter>(Killer->GetInstigator());
		}
	}

	if (RewardCharacter != nullptr)
	{
		return RewardCharacter;
	}

	// 无法从 Killer、本层 Owner 或 Instigator 解析时，回退到奖励位置最近的存活玩家。
	return FindNearestAlivePlayer(RewardOrigin != nullptr ? RewardOrigin->GetActorLocation() : FVector::ZeroVector);
}

void ARogueGameMode::AwardEnemyCurrency(const ARogueEnemy* Enemy, ARogueCharacter* RewardCharacter)
{
	if (!IsValid(Enemy) || RewardCharacter == nullptr)
	{
		return;
	}

	RewardCharacter->AddMoney(RogueGameModeRules::BuildCurrencyReward(RunState.CurrentWave, Enemy->IsBoss(), LoadedGameBalanceAsset));
}

void ARogueGameMode::SpawnExperienceReward(ARogueEnemy* Enemy, URogueEnemyTrackerSubsystem* Tracker, URogueCombatPoolSubsystem* Pools)
{
	if (!IsValid(Enemy))
	{
		return;
	}

	ARogueExperiencePickup* MergeTargetPickup = Tracker != nullptr ? Tracker->FindPickupMergeTarget(Enemy->GetActorLocation(), 170.0f) : nullptr;
	if (MergeTargetPickup != nullptr)
	{
		MergeTargetPickup->AddExperienceValue(Enemy->GetExperienceReward());
		return;
	}

	if (Pools == nullptr)
	{
		return;
	}

	ARogueExperiencePickup* Pickup = Pools->AcquireExperiencePickup(ExperiencePickupClass, this, Enemy->GetActorLocation());
	if (Pickup != nullptr)
	{
		Pickup->ActivatePooledPickup(this, Enemy->GetActorLocation(), Enemy->GetExperienceReward());
		if (Tracker != nullptr)
		{
			Tracker->RegisterExperiencePickup(Pickup);
		}
	}
}

void ARogueGameMode::HandleBossKilledReward(const FGuid& RewardId)
{
	if (!RewardId.IsValid() || RewardedBossSpawnIds.Contains(RewardId))
	{
		return;
	}
	RewardedBossSpawnIds.Add(RewardId);
	RunState.bBossAlive = false;

	// 冰砖发给本局每个参与者，包括正在濒死等待复活的玩家。
	// 各自客户端写自己的本地存档；旁观者和已经断开的控制器不参与。
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!IsValid(Controller) || Controller->IsActorBeingDestroyed()) { continue; }
		ARoguePlayerState* State = Controller->GetPlayerState<ARoguePlayerState>();
		if (State != nullptr && !State->IsOnlyASpectator()) { State->GrantBossIceBrick(RewardId); }
	}

	// 本局即时选卡仍只给存活玩家，避免濒死界面被无法操作的卡牌遮挡。
	TArray<ARogueCharacter*, TInlineAllocator<4>> AlivePlayers;
	CollectAlivePlayers(AlivePlayers);
	for (ARogueCharacter* Character : AlivePlayers)
	{
		QueueUpgradeSelections(BossSettings.BossBonusSelections, Character);
	}
}

// 注：原 ARogueGameMode::GetShopPromptWorldLocation 已删除。
// 它读的是 GameMode 上的 SpawnedShopTerminal，客户端拿不到 GameMode 也就永远拿不到提示。
// 现在 HUD 直接问角色（ARogueCharacter::GetShopPromptWorldLocation），
// 走的是商店终端的重叠检测 —— 终端本身是复制到客户端的。

void ARogueGameMode::StartUpgradeSelection(ARogueCharacter* Character)
{
	QueueUpgradeSelections(1, Character);
}

void ARogueGameMode::TryOpenShop(ARogueCharacter* Character)
{
	if (RunState.bGameOver || !IsValid(Character) || Character->IsDead())
	{
		return;
	}

	ARoguePlayerState* PlayerState = Character->GetPlayerState<ARoguePlayerState>();
	if (PlayerState == nullptr)
	{
		return;
	}

	// ── 每玩家一份商店 ──
	// 商店运行态挂在玩家自己的 PlayerState 上：商品列表、刷新价格、自动刷新计时、
	// 各升级的已购次数都是独立的。旧实现是 GameMode 上一个共享 ShopSystem，
	// 联机下所有玩家抢同一批货，而且"谁买的"还会算错人。
	FRogueShopSystem& Shop = PlayerState->GetShopRuntime();
	if (Shop.IsOpen())
	{
		return;
	}

	Shop.Configure(
		RogueGameModeRules::GetShopOfferCount(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopOfferCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopRefreshCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopAutoRefreshInterval(LoadedGameBalanceAsset));

	if (!Shop.OpenShop(Character, UpgradeSystem))
	{
		return;
	}

	// 此处可更新监听服本机输入；远端玩家仍由客户端 HUD 收到商店镜像后推导。
	// 关店时需结合其他菜单状态决定输入模式，见 CloseShop。
	Character->SetMenuInteractionInput(true);

	// 立即更新服务端的商店镜像，避免等待下次 Tick；客户端显示仍取决于复制到达。
	PushShopSnapshotForPlayer(PlayerState);

	// 打开商店只改变该玩家的运行态；本地 HUD 另行推导光标、输入模式和菜单移动限制。
}

void ARogueGameMode::CloseShop(ARogueCharacter* Requester)
{
	ARoguePlayerState* PlayerState = IsValid(Requester) ? Requester->GetPlayerState<ARoguePlayerState>() : nullptr;
	if (PlayerState == nullptr || !PlayerState->GetShopRuntime().IsOpen())
	{
		return;
	}

	PlayerState->GetShopRuntime().CloseShop();
	PushShopSnapshotForPlayer(PlayerState);

	// 光标与输入模式由本地 HUD::UpdateInteractionInputMode 结合全部菜单/生命周期状态推导。
	// 关店后仍可能有其他 UI 需要输入，因此此服务端入口只更新商店状态。
}

void ARogueGameMode::TryBuyShopOffer(int32 OfferIndex, ARogueCharacter* Purchaser)
{
	ARoguePlayerState* PlayerState = IsValid(Purchaser) ? Purchaser->GetPlayerState<ARoguePlayerState>() : nullptr;
	if (PlayerState == nullptr)
	{
		return;
	}

	// 与 TryOpenShop 对齐：局已结束或人已经死了就不许再买东西。
	// 原来这里只判 Shop.IsOpen()，安全性全靠"死亡时会有人去关店"这条路兜底 ——
	// 一旦将来多出一条漏关店的路径（结算、换人、暂停…），客户端就能在死亡后继续消费。
	if (RunState.bGameOver || Purchaser->IsDead())
	{
		return;
	}

	FRogueShopSystem& Shop = PlayerState->GetShopRuntime();
	if (!Shop.IsOpen())
	{
		return;
	}

	FRogueUpgradeOption PurchasedUpgrade;
	if (Shop.TryBuyOffer(OfferIndex, Purchaser, PurchasedUpgrade))
	{
		// 升级施加给**购买者**（旧实现固定给 CachedCharacter，联机下会把玩家 2 的购买算到玩家 1 头上）
		Purchaser->ApplyUpgrade(PurchasedUpgrade);
		// 购买会改变"已购"标记与刷新价格，立刻同步
		PushShopSnapshotForPlayer(PlayerState);
	}
}

void ARogueGameMode::TryRefreshShop(ARogueCharacter* Requester)
{
	ARoguePlayerState* PlayerState = IsValid(Requester) ? Requester->GetPlayerState<ARoguePlayerState>() : nullptr;
	if (PlayerState == nullptr)
	{
		return;
	}

	// 同 TryBuyShopOffer：结算后 / 死亡后都不允许再刷新商店
	if (RunState.bGameOver || Requester->IsDead())
	{
		return;
	}

	FRogueShopSystem& Shop = PlayerState->GetShopRuntime();
	if (!Shop.IsOpen())
	{
		return;
	}

	Shop.TryRefresh(Requester, UpgradeSystem);
	PushShopSnapshotForPlayer(PlayerState);
}

void ARogueGameMode::PushShopSnapshotForPlayer(ARoguePlayerState* PlayerState)
{
	if (PlayerState == nullptr)
	{
		return;
	}

	// 发送服务器期限，让客户端自行显示剩余时间，减少持续变化字段。
	// 当前每次快照都会以 ServerTime + Remaining 重算期限，不能保证只在刷新时改变。
	const FRogueShopSystem& Shop = PlayerState->GetShopRuntime();

	float AutoRefreshDeadline = 0.0f;
	if (Shop.IsOpen())
	{
		const float RemainingSeconds = Shop.GetSecondsUntilAutoRefresh();
		if (RemainingSeconds > 0.0f)
		{
			const AGameStateBase* CurrentGameState = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
			const float ServerTime = CurrentGameState != nullptr
				? CurrentGameState->GetServerWorldTimeSeconds()
				: (GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);
			AutoRefreshDeadline = ServerTime + RemainingSeconds;
		}
	}

	PlayerState->ApplyShopSnapshot(
		Shop.IsOpen(),
		Shop.GetOffers(),
		Shop.GetRefreshCost(),
		AutoRefreshDeadline);
}

void ARogueGameMode::PushShopStateToPlayers()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PlayerController = It->Get())
		{
			PushShopSnapshotForPlayer(PlayerController->GetPlayerState<ARoguePlayerState>());
		}
	}
}

void ARogueGameMode::HandlePlayerUpgradeSelected(ARoguePlayerState* PlayerState, const FRogueUpgradeOption& SelectedUpgrade)
{
	ARogueCharacter* Character = PlayerState != nullptr ? Cast<ARogueCharacter>(PlayerState->GetPawn()) : nullptr;
	if (!IsValid(Character))
	{
		return;
	}

	Character->ApplyUpgrade(SelectedUpgrade);

	// 这个玩家还有排队的选择 → 继续给他开下一轮
	if (PlayerState->GetQueuedUpgradeSelections() > 0)
	{
		OpenNextUpgradeSelectionForPlayer(PlayerState);
		return;
	}

	PlayerState->CloseUpgradeSelection();
	Character->SetUpgradeSelectionInput(false);

	// 不暂停游戏：联机下暂停是全局的，一个玩家在选卡不该冻住其他人。
	// 世界继续跑意味着选卡时要自己找安全位置 —— 这是刻意的玩法张力。
}

bool ARogueGameMode::IsAwaitingUpgradeChoice() const
{
	// 遍历 PlayerController 而不是 GameState->PlayerArray：
	// 此查询遍历当前服务端控制器，再检查已就绪的 PlayerState；不等待客户端复制。
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PlayerController = It->Get())
		{
			if (const ARoguePlayerState* RoguePlayerState = PlayerController->GetPlayerState<ARoguePlayerState>())
			{
				if (RoguePlayerState->IsAwaitingUpgradeChoice())
				{
					return true;
				}
			}
		}
	}
	return false;
}

void ARogueGameMode::QueueUpgradeSelections(int32 Count, ARogueCharacter* Character)
{
	if (Count <= 0 || RunState.bGameOver || !IsValid(Character))
	{
		return;
	}

	CachedCharacter = Character;

	// 升级状态是**每玩家一份**的：写入该玩家的 PlayerState（会自动复制给他的客户端）。
	ARoguePlayerState* PlayerState = Character->GetPlayerState<ARoguePlayerState>();
	if (PlayerState == nullptr)
	{
		return;
	}

	PlayerState->QueueUpgradeSelections(Count);
	if (!PlayerState->IsAwaitingUpgradeChoice())
	{
		OpenNextUpgradeSelectionForPlayer(PlayerState);
	}
}

void ARogueGameMode::OpenNextUpgradeSelectionForPlayer(ARoguePlayerState* PlayerState)
{
	ARogueCharacter* Character = PlayerState != nullptr ? Cast<ARogueCharacter>(PlayerState->GetPawn()) : nullptr;
	if (!IsValid(Character) || Character->IsDead())
	{
		if (PlayerState != nullptr)
		{
			PlayerState->CloseUpgradeSelection();
		}
		return;
	}

	if (!PlayerState->ConsumeQueuedSelection())
	{
		PlayerState->CloseUpgradeSelection();
		Character->SetUpgradeSelectionInput(false);
		return;
	}

	// 卡池由 GameMode 持有的配置（升级定义表 + 规则资产）生成 —— 这些是服务端资产，不复制。
	TArray<FRogueUpgradeOption> Options;
	UpgradeSystem.BuildRandomUpgradeOptions(Character, 3, Options);
	if (Options.Num() == 0)
	{
		PlayerState->CloseUpgradeSelection();
		Character->SetUpgradeSelectionInput(false);
		return;
	}

	PlayerState->OpenUpgradeSelection(Options);
	Character->SetUpgradeSelectionInput(true);
	// 不暂停（见 HandlePlayerUpgradeSelected 的说明）
}
