#include "Framework/RogueGameMode.h"
#include "Debug/RogueSelfTest.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "Shared/RogueTypes.h"
#include "AbilitySystem/RogueAttributeSet.h"

#include "World/RogueEnemySpawnRegion.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Player/RoguePlayerController.h"
#include "EngineUtils.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
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
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* DefaultGameBalanceAssetPath = TEXT("/Game/Data/Core/DA_GameBalance.DA_GameBalance");
	const TCHAR* DefaultUpgradeDefinitionAssetPath = TEXT("/Game/Data/Upgrades/DA_UpgradeDefinitions.DA_UpgradeDefinitions");
	const TCHAR* DefaultUpgradeRuleAssetPath = TEXT("/Game/Data/Upgrades/DA_UpgradeRules.DA_UpgradeRules");
	const TCHAR* DefaultEnemyArchetypeTablePath = TEXT("/Game/Data/Enemies/DT_EnemyArchetypes.DT_EnemyArchetypes");

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

bool ARogueGameMode::HasEnemyClassForType(ERogueEnemyType Type) const
{
	const TSubclassOf<ARogueEnemy>* Found = EnemyClassMap.Find(Type);
	return Found != nullptr && Found->Get() != nullptr;
}

ARogueGameMode::ARogueGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseSeamlessTravel = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	// 整局运行状态走 GameState（会复制到所有客户端）。
	// GameMode 是服务端独占对象，客户端拿不到 —— 所以 HUD 要显示的东西不能只放在这里。
	GameStateClass = ARogueGameState::StaticClass();

	// 本项目将玩家 ASC 放在 PlayerState，Character 作为 Avatar；其他项目也可按生命周期需求选宿主。
	// 注意：这是在 C++ 里设的，如果 BP_RogueGameMode 覆盖过 PlayerStateClass，
	// 需要同步改成 BP 或清掉那个覆盖。
	PlayerStateClass = ARoguePlayerState::StaticClass();

	// 用项目自己的 PlayerController：它记录"曾经拥有的 Pawn"，供下面的归属自愈认领回原角色。
	// （详见 ARoguePlayerController 与 RepairPlayerPawnOwnership 的注释）
	PlayerControllerClass = ARoguePlayerController::StaticClass();

	HUDClass = ARogueHUD::StaticClass();
}

void ARogueGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// Travel is a gameplay contract, including BP instances saved with an older default.
	bUseSeamlessTravel = true;

	// 玩家 Pawn 类从配置/BP 读取；缺失时回退原生 Character，避免空 DefaultPawnClass。
	DefaultPawnClass = nullptr;
	if (!PlayerPawnClass.IsNull())
	{
		DefaultPawnClass = PlayerPawnClass.LoadSynchronous();
	}
	if (DefaultPawnClass == nullptr)
	{
		DefaultPawnClass = ARogueCharacter::StaticClass();
		UE_LOG(LogTemp, Warning, TEXT("[Rogue] PlayerPawnClass 未配置或加载失败，回退原生 ARogueCharacter（无 BP 外观）。"));
	}
}

void ARogueGameMode::BeginPlay()
{
	Super::BeginPlay();

	FindLevelGameplayActors();
	ConfigureRuntimeAssets();
	ConfigureRuntimeSubsystems();
	InitializeBattleStage();
	PrewarmCombatPools();
}

void ARogueGameMode::FindLevelGameplayActors()
{
    ARogueEnemySpawnRegion* Candidate = nullptr;
    int32 Count = 0;
    for (TActorIterator<ARogueEnemySpawnRegion> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It)) { Candidate = *It; ++Count; }
    }
    LevelSpawnRegion = Count == 1 ? Candidate : nullptr;
    if (Count != 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RogueSpawn] Expected one Enemy Spawn Region in the map, found %d. Automatic spawning is disabled."), Count);
    }
    // 场景、商店和玩家出生点均由关卡配置；不再生成竞技场/商店、覆盖灯光或强制传送玩家。
    CachedCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
}

void ARogueGameMode::ConfigureRuntimeAssets()
{
	LoadedGameBalanceAsset = LoadConfiguredOrDefaultAsset(GameBalanceAsset, DefaultGameBalanceAssetPath);
	LoadedEnemyArchetypeTable = LoadConfiguredOrDefaultAsset(EnemyArchetypeTable, DefaultEnemyArchetypeTablePath);
	UpgradeSystem.SetDefinitionAsset(LoadConfiguredOrDefaultAsset(UpgradeDefinitionAsset, DefaultUpgradeDefinitionAssetPath));
	UpgradeSystem.SetRuleAsset(LoadConfiguredOrDefaultAsset(UpgradeRuleAsset, DefaultUpgradeRuleAssetPath));

	// 刷怪/Boss 节奏统一以 DA_GameBalance 为准；GameMode 上的结构体只保留代码兜底。
	if (LoadedGameBalanceAsset != nullptr)
	{
		const FRogueSpawnBalanceSettings& BalanceSpawn = LoadedGameBalanceAsset->Spawn;
		SpawnSettings.SpawnRadius = BalanceSpawn.SpawnRadius;
		SpawnSettings.SpawnInterval = BalanceSpawn.SpawnInterval;
		SpawnSettings.MaxAliveEnemies = BalanceSpawn.BaseMaxAliveEnemies;
		SpawnSettings.EnemySpawnMinimumDistance = BalanceSpawn.EnemySpawnMinimumDistance;
		SpawnSettings.GroundTraceHeight = BalanceSpawn.GroundTraceHeight;
		SpawnSettings.GroundTraceDepth = BalanceSpawn.GroundTraceDepth;
		SpawnSettings.GroundClearance = BalanceSpawn.GroundClearance;
		SpawnSettings.GroundTraceChannel = BalanceSpawn.GroundTraceChannel;

		const FRogueBossSpawnSettings& BalanceBoss = LoadedGameBalanceAsset->BossSpawn;
		BossSettings.BossWaveInterval = BalanceBoss.BossWaveInterval;
		BossSettings.BossBonusSelections = BalanceBoss.BossBonusSelections;
		BossSettings.BossSpawnDistance = BalanceBoss.BossSpawnDistance;
		BossSettings.BossSpawnMinimumDistance = BalanceBoss.BossSpawnMinimumDistance;

		PoolSettings = LoadedGameBalanceAsset->Pools;
		RuntimePerformanceSettings = LoadedGameBalanceAsset->Performance;
	}

	// 注：商店配置不在这里做。商店是**每玩家一份**（挂在各自的 PlayerState 上），
	// 每个玩家第一次打开商店时用同一份平衡资产配置自己那份（见 TryOpenShop）。
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
		SpawnSubsystem->Configure(DefaultEnemyClass, EnemyClassMap, SpawnSettings, BossSettings, LoadedGameBalanceAsset, LoadedEnemyArchetypeTable);
	}
}

void ARogueGameMode::PrewarmCombatPools()
{
	// 经验球的原生类只剩碰撞与拾取逻辑，没有任何网格和灯光：
	// 没配 BP 时球在画面上是隐形的，这里显式告警，避免"捡到经验但看不到球"这种哑故障。
	if (ExperiencePickupClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rogue] ExperiencePickupClass 未配置，经验球将使用原生类（无外观）。"
			"请在 DefaultGame.ini 的 [/Script/ai.RogueGameMode] 段配置 /Game/BP/World/BP_ExperiencePickup。"));
	}

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

	// 敌人弹体：敌人 BP 可在类默认值里指定自己的弹体蓝图；只预热实际会用到的类，
	// 避免蓝图接管后仍白占一份原生弹体池。没有任何敌人配置时才回退原生类。
	TSet<UClass*> EnemyProjectileClassSet;

	auto CollectEnemyProjectileClass = [&EnemyProjectileClassSet](const TSubclassOf<ARogueEnemy>& EnemyClass)
	{
		const ARogueEnemy* EnemyCDO = EnemyClass != nullptr ? EnemyClass->GetDefaultObject<ARogueEnemy>() : nullptr;
		if (EnemyCDO != nullptr)
		{
			// 只收集 BP 显式配置的类；没有配置的类型交给原型表或原生回退，
			// 避免在蓝图接管后还预热一份用不到的原生弹体池。
			if (EnemyCDO->EnemyProjectileClass != nullptr)
			{
				EnemyProjectileClassSet.Add(EnemyCDO->EnemyProjectileClass.Get());
			}
			if (EnemyCDO->BossProjectileClass != nullptr)
			{
				EnemyProjectileClassSet.Add(EnemyCDO->BossProjectileClass.Get());
			}
		}
	};
	CollectEnemyProjectileClass(DefaultEnemyClass);
	for (const TPair<ERogueEnemyType, TSubclassOf<ARogueEnemy>>& EnemyClassPair : EnemyClassMap)
	{
		CollectEnemyProjectileClass(EnemyClassPair.Value);
	}

	// 原型数据表可以按敌人类型单独指定弹体（优先于敌人 BP 默认值），一并预热。
	if (LoadedEnemyArchetypeTable != nullptr)
	{
		for (const FName& RowName : LoadedEnemyArchetypeTable->GetRowNames())
		{
			const FRogueEnemyArchetypeRow* Row = LoadedEnemyArchetypeTable->FindRow<FRogueEnemyArchetypeRow>(
				RowName, TEXT("EnemyProjectilePrewarm"));
			if (Row != nullptr && Row->ProjectileClass != nullptr)
			{
				EnemyProjectileClassSet.Add(Row->ProjectileClass.Get());
			}
		}
	}

	if (EnemyProjectileClassSet.Num() == 0)
	{
		EnemyProjectileClassSet.Add(ARogueEnemyProjectile::StaticClass());
	}
	for (UClass* ProjectileClass : EnemyProjectileClassSet)
	{
		if (ProjectileClass != nullptr)
		{
			OutClasses.EnemyProjectileClasses.AddUnique(TSubclassOf<ARogueEnemyProjectile>(ProjectileClass));
		}
	}

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

	// 在早退前刷新共享状态，确保结算或自检期间仍提供当前值；规则推进后的变化通常于后续推送可见。
	PushRunStateToGameState();

	// 世界暂停恢复兜底；构造时启用 bTickEvenWhenPaused，前提是本 Tick 仍被引擎调度。
	// 编辑器失焦节流、时间膨胀或线程阻塞需要分别定位，不能由此推断为暂停问题。
	{
		UWorld* WatchdogWorld = GetWorld();
		if (WatchdogWorld != nullptr && WatchdogWorld->IsPaused())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 世界被暂停（本作设计里不应发生）：已强制恢复。若你在用控制台 pause 调试，请临时注释掉这段看门狗"));
			UGameplayStatics::SetGamePaused(this, false);
		}
	}

	TickPawnOwnershipRepair();
	PushShopStateToPlayers();

	// 从各玩家属性集读取 13 项加成，写入各自角色快照；近似相等时跳过赋值。
	// 此接口统一武器输入，但不取代属性复制，也不保证与其他 Actor 的状态同时到达。
	PushWeaponBonusSnapshots();

	// 开发自检（默认关，见 RunWeaponUpgradeSelfTest）：验证升级 → GAS 属性 → 武器生效值这条链
	const bool bLifeCycleSelfTest = RunPlayerLifeCycleSelfTest();
	if (!bLifeCycleSelfTest)
	{
		RunWeaponUpgradeSelfTest();
	}

	// 团灭是终态；复活仅由各玩家的一次性服务端定时器执行。
	EvaluateTeamWipe();

	if (RunState.bGameOver || bLifeCycleSelfTest)
	{
		return;
	}

	UWorld* World = GetWorld();
	const bool bWorldPaused = World != nullptr && World->IsPaused();

	AdvanceShopAutoRefresh(DeltaSeconds, bWorldPaused);

	// 只有"世界真的被暂停"才冻结整局。
	//
	// 这里原来还有 `IsAwaitingUpgradeChoice() || ShopSystem.IsOpen()`：
	// 那是为"开商店/选升级会暂停游戏"的旧设计服务的。现在两者都不再暂停，
	// 波次计时与刷怪必须继续推进 —— 否则玩家一开商店，整局节奏就被人为停住了
	// （联机下更糟：一个人开商店，其他人的波次也跟着不走）。
	if (bWorldPaused)
	{
		return;
	}

	TickRunSystems(DeltaSeconds);
}

void ARogueGameMode::PushRunStateToGameState()
{
	ARogueGameState* RogueGameState = GetGameState<ARogueGameState>();
	if (RogueGameState == nullptr)
	{
		return;
	}

	// 集中更新服务端的整局字段；各 UPROPERTY 独立复制，本函数不提供跨字段原子同步。
	RogueGameState->SetBattleStage(StageView);
	RogueGameState->SetRunStateSnapshot(
		RunState.CurrentWave,
		RunState.EnemiesDefeated,
		RunState.RunTimeSeconds,
		RunState.bGameOver,
		RunState.bBossAlive);
	RogueGameState->RefreshBossHealthView();
}

void ARogueGameMode::PushWeaponBonusSnapshots()
{
	TArray<ARogueCharacter*, TInlineAllocator<4>> Players;
	CollectAlivePlayers(Players);

	for (ARogueCharacter* Character : Players)
	{
		if (Character != nullptr)
		{
			Character->RefreshWeaponBonusSnapshotFromAttributes();
		}
	}
}

void ARogueGameMode::CollectAlivePlayers(TArray<ARogueCharacter*, TInlineAllocator<4>>& OutPlayers) const
{
	// 实现统一在 RoguePlayerQuery 里（敌人索敌、刷怪点检查、经验球磁吸共用同一份口径）
	RoguePlayerQuery::CollectAlivePlayers(GetWorld(), OutPlayers);
}

void ARogueGameMode::TickRunSystems(float DeltaSeconds)
{
    // 存活时间独立于波次；清场战斗计时，休整与切图期间不计时。
    if (StageView.Phase != ERogueBattleStage::Countdown && StageView.Phase != ERogueBattleStage::Travelling)
        RunState.RunTimeSeconds += DeltaSeconds;
	if (TickBattleStage(DeltaSeconds)) { return; }

	// 生成调度 —— 委托给 SpawnSubsystem
	if (URogueSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<URogueSpawnSubsystem>())
	{
        if (StageView.Phase == ERogueBattleStage::Night)
        {
            // 黑夜按独立阶段时钟推进；白天耗时、剩余敌人及 Boss 都不延迟下一波。
            const double Duration = StageSettings.NightWaveDuration;
            NightWaveElapsedSeconds += FMath::Max(0.f, DeltaSeconds);
            const double CompletedPeriods = FMath::FloorToDouble(NightWaveElapsedSeconds / Duration);
            if (CompletedPeriods >= 1)
            {
                // 卡帧跨过多个完整周期时补上波数，保留本周期余量，不集中补刷跳过的名额。
                const int32 Advance = static_cast<int32>(FMath::Min(CompletedPeriods,
                    static_cast<double>(MAX_int32 - RunState.CurrentWave)));
                RunState.CurrentWave += Advance;
                NightWaveElapsedSeconds = FMath::Fmod(NightWaveElapsedSeconds, Duration);
                RunState.SpawnTimer = 0.f;
                UE_LOG(LogTemp, Log, TEXT("[RogueWave] Night timer; starting wave %d."), RunState.CurrentWave);
            }
        }
        else if (SpawnSubsystem->IsWaveComplete(RunState))
        {
            ++RunState.CurrentWave;
            RunState.SpawnTimer = 0.f;
            UE_LOG(LogTemp, Log, TEXT("[RogueWave] Cleared; starting wave %d."), RunState.CurrentWave);
        }
		SpawnSubsystem->TickSpawning(DeltaSeconds, RunState, GetSpawnReferenceCharacter(), LevelSpawnRegion.Get());
	}
}

void ARogueGameMode::TickPawnOwnershipRepair()
{
	if (UWorld* World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		if (LastPawnRepairTime < 0.0 || Now < LastPawnRepairTime || Now - LastPawnRepairTime >= 0.5)
		{
			LastPawnRepairTime = Now;
			RepairPlayerPawnOwnership();
		}
	}
}
