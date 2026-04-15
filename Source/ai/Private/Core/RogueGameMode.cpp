#include "Core/RogueGameMode.h"

#include "World/RogueArena.h"
#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "World/RogueExperiencePickup.h"
#include "World/RogueShopTerminal.h"
#include "UI/RogueHUD.h"
#include "Core/RogueGameBalanceAsset.h"
#include "Core/RogueUpgradeDefinitionAsset.h"
#include "Core/RogueUpgradeRuleAsset.h"
#include "Core/RogueGameModeRules.h"
#include "Core/RogueUpgradeSystem.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "Subsystems/RogueSpawnSubsystem.h"
#include "Subsystems/RogueSceneSubsystem.h"
#include "Kismet/GameplayStatics.h"

ARogueGameMode::ARogueGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ARogueCharacter::StaticClass();
	HUDClass = ARogueHUD::StaticClass();
	ArenaClass = ARogueArena::StaticClass();
	ShopTerminalClass = ARogueShopTerminal::StaticClass();
}

void ARogueGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!SpawnedArena.IsValid())
	{
		UClass* ArenaToSpawn = ArenaClass ? ArenaClass.Get() : ARogueArena::StaticClass();
		SpawnedArena = GetWorld()->SpawnActor<ARogueArena>(ArenaToSpawn, FVector::ZeroVector, FRotator::ZeroRotator);
	}

	if (!SpawnedShopTerminal.IsValid())
	{
		UClass* ShopTerminalToSpawn = ShopTerminalClass ? ShopTerminalClass.Get() : ARogueShopTerminal::StaticClass();
		const float TerminalOffset = SpawnedArena.IsValid() ? FMath::Min(SpawnedArena->GetArenaHalfExtent() * 0.42f, 2400.0f) : 1800.0f;
		SpawnedShopTerminal = GetWorld()->SpawnActor<ARogueShopTerminal>(ShopTerminalToSpawn, FVector(0.0f, -TerminalOffset, 0.0f), FRotator::ZeroRotator);
	}

	// 场景优化 —— 委托给 SceneSubsystem
	if (URogueSceneSubsystem* SceneSubsystem = GetWorld()->GetSubsystem<URogueSceneSubsystem>())
	{
		SceneSubsystem->OptimizeLevelEnvironment(SceneOptimizationSettings);
	}

	CachedCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (CachedCharacter.IsValid())
	{
		CachedCharacter->SetActorLocation(FVector(0.0f, 0.0f, 120.0f));
	}

	LoadedGameBalanceAsset = GameBalanceAsset.IsNull() ? nullptr : GameBalanceAsset.LoadSynchronous();
	UpgradeSystem.SetDefinitionAsset(UpgradeDefinitionAsset.IsNull() ? nullptr : UpgradeDefinitionAsset.LoadSynchronous());
	UpgradeSystem.SetRuleAsset(UpgradeRuleAsset.IsNull() ? nullptr : UpgradeRuleAsset.LoadSynchronous());
	ShopSystem.Configure(
		RogueGameModeRules::GetShopOfferCount(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopOfferCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopRefreshCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopAutoRefreshInterval(LoadedGameBalanceAsset));

	// 配置敌人追踪子系统
	if (URogueEnemyTrackerSubsystem* TrackerSubsystem = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>())
	{
		TrackerSubsystem->Configure(RuntimePerformanceSettings);
	}

	// 配置生成子系统
	if (URogueSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<URogueSpawnSubsystem>())
	{
		SpawnSubsystem->Configure(DefaultEnemyClass, EnemyClassMap, SpawnSettings, BossSettings, LoadedGameBalanceAsset);
	}

	// 预热对象池 —— 委托给 CombatPoolSubsystem
	if (URogueCombatPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>())
	{
		PoolSubsystem->Prewarm(this, PoolSettings, DefaultEnemyClass, ExperiencePickupClass);
	}
}

void ARogueGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (RunState.bGameOver || UpgradeSystem.IsAwaitingChoice() || ShopSystem.IsOpen())
	{
		return;
	}

	RunState.RunTimeSeconds += DeltaSeconds;
	ShopSystem.AdvanceAutoRefresh(DeltaSeconds, CachedCharacter.Get(), UpgradeSystem);

	// 生成调度 —— 委托给 SpawnSubsystem
	if (URogueSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<URogueSpawnSubsystem>())
	{
		if (!CachedCharacter.IsValid())
		{
			CachedCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		}

		SpawnSubsystem->TickSpawning(DeltaSeconds, RunState, CachedCharacter.Get(), SpawnedArena.Get());
	}
}

void ARogueGameMode::HandleEnemyKilled(ARogueEnemy* Enemy, AActor* Killer)
{
	++RunState.EnemiesDefeated;

	URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	URogueCombatPoolSubsystem* Pools = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>();

	if (Tracker != nullptr)
	{
		Tracker->UnregisterEnemy(Enemy);
	}

	if (IsValid(Enemy))
	{
		ARogueCharacter* RewardCharacter = Cast<ARogueCharacter>(Killer);
		if (RewardCharacter == nullptr && IsValid(Killer))
		{
			RewardCharacter = Cast<ARogueCharacter>(Killer->GetOwner());
			if (RewardCharacter == nullptr)
			{
				RewardCharacter = Cast<ARogueCharacter>(Killer->GetInstigator());
			}
		}
		if (RewardCharacter == nullptr)
		{
			RewardCharacter = CachedCharacter.Get();
		}
		if (RewardCharacter != nullptr)
		{
			RewardCharacter->AddMoney(RogueGameModeRules::BuildCurrencyReward(RunState.CurrentWave, Enemy->IsBoss(), LoadedGameBalanceAsset));
		}

		ARogueExperiencePickup* MergeTargetPickup = Tracker != nullptr ? Tracker->FindPickupMergeTarget(Enemy->GetActorLocation(), 170.0f) : nullptr;

		if (MergeTargetPickup != nullptr)
		{
			MergeTargetPickup->AddExperienceValue(Enemy->GetExperienceReward());
		}
		else if (Pools != nullptr)
		{
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

		if (Enemy->IsBoss())
		{
			RunState.bBossAlive = false;
			if (!CachedCharacter.IsValid())
			{
				CachedCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
			}

			QueueUpgradeSelections(BossSettings.BossBonusSelections, CachedCharacter.Get());
		}
	}
}

bool ARogueGameMode::GetShopPromptWorldLocation(FVector& OutLocation) const
{
	const ARogueShopTerminal* ShopTerminal = SpawnedShopTerminal.Get();
	if (!IsValid(ShopTerminal))
	{
		return false;
	}

	OutLocation = ShopTerminal->GetPromptWorldLocation();
	return true;
}

void ARogueGameMode::StartUpgradeSelection(ARogueCharacter* Character)
{
	QueueUpgradeSelections(1, Character);
}

void ARogueGameMode::TryOpenShop(ARogueCharacter* Character)
{
	if (RunState.bGameOver || UpgradeSystem.IsAwaitingChoice() || ShopSystem.IsOpen() || !IsValid(Character) || Character->IsDead())
	{
		return;
	}

	CachedCharacter = Character;
	ShopSystem.Configure(
		RogueGameModeRules::GetShopOfferCount(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopOfferCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopRefreshCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopAutoRefreshInterval(LoadedGameBalanceAsset));

	if (!ShopSystem.OpenShop(Character, UpgradeSystem))
	{
		return;
	}

	Character->SetMenuInteractionInput(true);
	UGameplayStatics::SetGamePaused(this, true);
}

void ARogueGameMode::CloseShop()
{
	if (!ShopSystem.IsOpen())
	{
		return;
	}

	ShopSystem.CloseShop();
	if (CachedCharacter.IsValid() && !UpgradeSystem.IsAwaitingChoice() && !RunState.bGameOver)
	{
		CachedCharacter->SetMenuInteractionInput(false);
	}
	UGameplayStatics::SetGamePaused(this, false);
}

void ARogueGameMode::TryBuyShopOffer(int32 OfferIndex)
{
	if (!ShopSystem.IsOpen() || !CachedCharacter.IsValid())
	{
		return;
	}

	FRogueUpgradeOption PurchasedUpgrade;
	if (ShopSystem.TryBuyOffer(OfferIndex, CachedCharacter.Get(), PurchasedUpgrade))
	{
		CachedCharacter->ApplyUpgrade(PurchasedUpgrade);
	}
}

void ARogueGameMode::TryRefreshShop()
{
	if (!ShopSystem.IsOpen() || !CachedCharacter.IsValid())
	{
		return;
	}

	ShopSystem.TryRefresh(CachedCharacter.Get(), UpgradeSystem);
}

void ARogueGameMode::TrySelectUpgrade(int32 UpgradeIndex)
{
	if (!CachedCharacter.IsValid())
	{
		return;
	}

	FRogueUpgradeOption SelectedUpgrade;
	if (!UpgradeSystem.TryConsumeChoice(UpgradeIndex, SelectedUpgrade))
	{
		return;
	}

	CachedCharacter->ApplyUpgrade(SelectedUpgrade);

	if (UpgradeSystem.GetQueuedSelections() > 0)
	{
		OpenNextUpgradeSelection();
		return;
	}

	CachedCharacter->SetUpgradeSelectionInput(false);
	UGameplayStatics::SetGamePaused(this, false);
}

void ARogueGameMode::NotifyPlayerDied()
{
	RunState.bGameOver = true;
	RunState.bBossAlive = false;
	UpgradeSystem.Reset();
	ShopSystem.CloseShop();
	UGameplayStatics::SetGamePaused(this, false);

	if (CachedCharacter.IsValid())
	{
		CachedCharacter->SetMenuInteractionInput(true);
	}
}

void ARogueGameMode::QueueUpgradeSelections(int32 Count, ARogueCharacter* Character)
{
	if (Count <= 0 || RunState.bGameOver)
	{
		return;
	}

	if (IsValid(Character))
	{
		CachedCharacter = Character;
	}

	if (!CachedCharacter.IsValid() || CachedCharacter->IsDead())
	{
		return;
	}

	UpgradeSystem.QueueSelections(Count);
	if (!UpgradeSystem.IsAwaitingChoice())
	{
		OpenNextUpgradeSelection();
	}
}

void ARogueGameMode::OpenNextUpgradeSelection()
{
	if (!CachedCharacter.IsValid() || CachedCharacter->IsDead())
	{
		return;
	}

	if (!UpgradeSystem.OpenNextSelection(CachedCharacter.Get()))
	{
		CachedCharacter->SetUpgradeSelectionInput(false);
		UGameplayStatics::SetGamePaused(this, false);
		return;
	}

	CachedCharacter->SetUpgradeSelectionInput(true);
	UGameplayStatics::SetGamePaused(this, true);
}