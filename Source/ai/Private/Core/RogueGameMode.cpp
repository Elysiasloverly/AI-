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
#include "Combat/RogueMortarProjectile.h"
#include "Combat/RogueWeaponBase.h"
#include "Enemies/RogueEnemyProjectile.h"
#include "Engine/DataTable.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "Subsystems/RogueSpawnSubsystem.h"
#include "Subsystems/RogueSceneSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const TCHAR* DefaultGameBalanceAssetPath = TEXT("/Game/DataTable/AS_RogueGameBalanceAsse.AS_RogueGameBalanceAsse");
	const TCHAR* DefaultUpgradeDefinitionAssetPath = TEXT("/Game/DataTable/AS_UpgradeDefinitionAsset.AS_UpgradeDefinitionAsset");
	const TCHAR* DefaultUpgradeRuleAssetPath = TEXT("/Game/DataTable/AS_RogueUpgradeRuleAsset.AS_RogueUpgradeRuleAsset");

	template <typename AssetType>
	AssetType* LoadConfiguredOrDefaultAsset(const TSoftObjectPtr<AssetType>& ConfiguredAsset, const TCHAR* DefaultAssetPath)
	{
		if (!ConfiguredAsset.IsNull())
		{
			if (AssetType* LoadedAsset = ConfiguredAsset.LoadSynchronous())
			{
				return LoadedAsset;
			}
		}

		const FSoftObjectPath DefaultObjectPath(DefaultAssetPath);
		const TSoftObjectPtr<AssetType> DefaultAsset(DefaultObjectPath);
		return DefaultAsset.IsNull() ? nullptr : DefaultAsset.LoadSynchronous();
	}
}

ARogueGameMode::ARogueGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	DefaultPawnClass = ARogueCharacter::StaticClass();
	HUDClass = ARogueHUD::StaticClass();
	ArenaClass = ARogueArena::StaticClass();
	ShopTerminalClass = ARogueShopTerminal::StaticClass();
}

void ARogueGameMode::BeginPlay()
{
	Super::BeginPlay();

	SpawnRuntimeWorldActors();
	ConfigureRuntimeAssets();
	ConfigureRuntimeSubsystems();
	PrewarmCombatPools();
}

void ARogueGameMode::SpawnRuntimeWorldActors()
{
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
}

void ARogueGameMode::ConfigureRuntimeAssets()
{
	LoadedGameBalanceAsset = LoadConfiguredOrDefaultAsset(GameBalanceAsset, DefaultGameBalanceAssetPath);
	UpgradeSystem.SetDefinitionAsset(LoadConfiguredOrDefaultAsset(UpgradeDefinitionAsset, DefaultUpgradeDefinitionAssetPath));
	UpgradeSystem.SetRuleAsset(LoadConfiguredOrDefaultAsset(UpgradeRuleAsset, DefaultUpgradeRuleAssetPath));
	ShopSystem.Configure(
		RogueGameModeRules::GetShopOfferCount(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopOfferCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopRefreshCost(LoadedGameBalanceAsset),
		RogueGameModeRules::GetShopAutoRefreshInterval(LoadedGameBalanceAsset));
}

void ARogueGameMode::ConfigureRuntimeSubsystems()
{
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
}

void ARogueGameMode::PrewarmCombatPools()
{
	// 预热对象池 —— 委托给 CombatPoolSubsystem
	if (URogueCombatPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>())
	{
		FRogueCombatPoolPrewarmClasses PrewarmClasses;
		BuildCombatPoolPrewarmClasses(PrewarmClasses);
		PoolSubsystem->Prewarm(this, PoolSettings, DefaultEnemyClass, ExperiencePickupClass, PrewarmClasses);
	}
}

void ARogueGameMode::BuildCombatPoolPrewarmClasses(FRogueCombatPoolPrewarmClasses& OutClasses) const
{
	OutClasses.EnemyClasses.Add(DefaultEnemyClass);
	for (const TPair<ERogueEnemyType, TSubclassOf<ARogueEnemy>>& EnemyClassPair : EnemyClassMap)
	{
		OutClasses.EnemyClasses.Add(EnemyClassPair.Value);
	}

	OutClasses.EnemyProjectileClasses.Add(ARogueEnemyProjectile::StaticClass());
	OutClasses.RocketProjectileClasses.Add(ARogueMortarProjectile::StaticClass());

	const ARogueCharacter* Character = CachedCharacter.Get();
	if (!IsValid(Character))
	{
		return;
	}

	const UDataTable* WeaponTable = Character->GetWeaponDataTable();
	if (WeaponTable == nullptr)
	{
		return;
	}

	const TArray<FName> RowNames = WeaponTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FRogueWeaponTableRow* Row = WeaponTable->FindRow<FRogueWeaponTableRow>(RowName, TEXT("CombatPoolPrewarm"));
		if (Row != nullptr)
		{
			Row->CollectPoolPrewarmClasses(OutClasses);
		}
	}
}

void ARogueGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (RunState.bGameOver)
	{
		LastShopAutoRefreshRealTimeSeconds = 0.0f;
		return;
	}

	UWorld* World = GetWorld();
	const bool bWorldPaused = World != nullptr && World->IsPaused();

	AdvanceShopAutoRefresh(DeltaSeconds, bWorldPaused);

	if (UpgradeSystem.IsAwaitingChoice() || ShopSystem.IsOpen() || bWorldPaused)
	{
		return;
	}

	TickRunSystems(DeltaSeconds);
}

ARogueCharacter* ARogueGameMode::GetCachedCharacter()
{
	if (!CachedCharacter.IsValid())
	{
		CachedCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	return CachedCharacter.Get();
}

void ARogueGameMode::AdvanceShopAutoRefresh(float DeltaSeconds, bool bWorldPaused)
{
	UWorld* World = GetWorld();
	float ShopDeltaSeconds = DeltaSeconds;
	if (bWorldPaused && ShopSystem.IsOpen())
	{
		const float CurrentRealTimeSeconds = World != nullptr ? World->GetRealTimeSeconds() : 0.0f;
		if (LastShopAutoRefreshRealTimeSeconds > 0.0f)
		{
			ShopDeltaSeconds = FMath::Max(0.0f, CurrentRealTimeSeconds - LastShopAutoRefreshRealTimeSeconds);
		}
		else
		{
			ShopDeltaSeconds = 0.0f;
		}
		LastShopAutoRefreshRealTimeSeconds = CurrentRealTimeSeconds;
	}
	else
	{
		ShopDeltaSeconds = bWorldPaused ? 0.0f : DeltaSeconds;
		LastShopAutoRefreshRealTimeSeconds = 0.0f;
	}

	ShopSystem.AdvanceAutoRefresh(ShopDeltaSeconds, CachedCharacter.Get(), UpgradeSystem);
}

void ARogueGameMode::TickRunSystems(float DeltaSeconds)
{
	RunState.RunTimeSeconds += DeltaSeconds;

	// 生成调度 —— 委托给 SpawnSubsystem
	if (URogueSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<URogueSpawnSubsystem>())
	{
		SpawnSubsystem->TickSpawning(DeltaSeconds, RunState, GetCachedCharacter(), SpawnedArena.Get());
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
		AwardEnemyCurrency(Enemy, ResolveRewardCharacter(Killer));
		SpawnExperienceReward(Enemy, Tracker, Pools);

		if (Enemy->IsBoss())
		{
			HandleBossKilledReward();
		}
	}
}

ARogueCharacter* ARogueGameMode::ResolveRewardCharacter(AActor* Killer) const
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

	return RewardCharacter != nullptr ? RewardCharacter : CachedCharacter.Get();
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

void ARogueGameMode::HandleBossKilledReward()
{
	RunState.bBossAlive = false;
	QueueUpgradeSelections(BossSettings.BossBonusSelections, GetCachedCharacter());
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
