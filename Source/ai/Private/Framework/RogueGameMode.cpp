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

namespace
{
	const TCHAR* DefaultGameBalanceAssetPath = TEXT("/Game/DataTable/AS_RogueGameBalanceAsse.AS_RogueGameBalanceAsse");
	const TCHAR* DefaultUpgradeDefinitionAssetPath = TEXT("/Game/DataTable/AS_UpgradeDefinitionAsset.AS_UpgradeDefinitionAsset");
	const TCHAR* DefaultUpgradeRuleAssetPath = TEXT("/Game/DataTable/AS_RogueUpgradeRuleAsset.AS_RogueUpgradeRuleAsset");
	const TCHAR* DefaultEnemyArchetypeTablePath = TEXT("/Game/DataTable/DT_FRogueEnemyArchetypeRow.DT_FRogueEnemyArchetypeRow");

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

	// 整局运行状态走 GameState（会复制到所有客户端）。
	// GameMode 是服务端独占对象，客户端拿不到 —— 所以 HUD 要显示的东西不能只放在这里。
	GameStateClass = ARogueGameState::StaticClass();

	// ASC 挂在 PlayerState 上（联机 GAS 的标准做法，详见 ARoguePlayerState 的注释）。
	// 注意：这是在 C++ 里设的，如果 BP_RogueGameMode 覆盖过 PlayerStateClass，
	// 需要同步改成 BP 或清掉那个覆盖。
	PlayerStateClass = ARoguePlayerState::StaticClass();

	// 用项目自己的 PlayerController：它记录"曾经拥有的 Pawn"，供下面的归属自愈认领回原角色。
	// （详见 ARoguePlayerController 与 RepairPlayerPawnOwnership 的注释）
	PlayerControllerClass = ARoguePlayerController::StaticClass();

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
	//生成竞技场
	if (!SpawnedArena.IsValid())
	{
		UClass* ArenaToSpawn = ArenaClass ? ArenaClass.Get() : ARogueArena::StaticClass();
		SpawnedArena = GetWorld()->SpawnActor<ARogueArena>(ArenaToSpawn, FVector::ZeroVector, FRotator::ZeroRotator);
	}
	//生成商店终端
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
	LoadedEnemyArchetypeTable = LoadConfiguredOrDefaultAsset(EnemyArchetypeTable, DefaultEnemyArchetypeTablePath);
	UpgradeSystem.SetDefinitionAsset(LoadConfiguredOrDefaultAsset(UpgradeDefinitionAsset, DefaultUpgradeDefinitionAssetPath));
	UpgradeSystem.SetRuleAsset(LoadConfiguredOrDefaultAsset(UpgradeRuleAsset, DefaultUpgradeRuleAssetPath));

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

	// 把整局状态推进 GameState（服务端权威 → 自动复制给所有客户端）。
	// 刻意放在所有早退分支**之前**：游戏结束、暂停、正在选卡时客户端同样需要正确数据
	// （否则结算界面会停在"结束前一帧"的数值上）。
	//
	// ⚠️ 这一行曾经被"加看门狗"的编辑意外覆盖掉（只剩注释）：后果是整局快照（存活时间、
	// 击杀数、波次、是否结束）**从来没有推给 GameState** —— HUD 永远显示"存活 0 秒"，
	// 客户端也永远收不到 bGameOver。改这一带代码时务必确认它还在。
	PushRunStateToGameState();

	// 暂停看门狗：本作**任何时候都不该暂停世界** —— 暂停菜单、商店、选卡、死亡界面都不暂停
	// （见 TogglePauseMenu / TryOpenShop 的注释；C++ 里也没有任何 SetGamePaused(true)）。
	//
	// 关于"整局停住"的历史结论（别再重复排查一遍）：
	//   · 实测 `World->IsPaused()` 一直是 0、全局时间膨胀也没变，本看门狗与角色侧的冻结探针
	//     在整个测试期都没有触发过；
	//   · 真正的现象是"编辑器窗口失去焦点后，PIE 世界停止推进"（会话日志里出现过大段
	//     只属于编辑器的行、游戏侧一行都没有），来自编辑器设置 bThrottleCPUWhenNotForeground；
	//   · 所以这段看门狗只是兜底（防某个蓝图真的调 Set Game Paused），不是那个现象的解法。
	//     注意它必须靠 `PrimaryActorTick.bTickEvenWhenPaused`（构造函数里已设）才能在暂停时运行。
	{
		UWorld* WatchdogWorld = GetWorld();
		if (WatchdogWorld != nullptr && WatchdogWorld->IsPaused())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 世界被暂停（本作设计里不应发生）：已强制恢复。若你在用控制台 pause 调试，请临时注释掉这段看门狗"));
			UGameplayStatics::SetGamePaused(this, false);
		}
	}

	// 归属自愈（每 0.5 秒一次）：
	// 引擎登录流程会让在场 PC 抢走新玩家的 Pawn，随后才纠正 —— 代价是在场 PC 失去自己的 Pawn
	// （主机失控）。PostLogin 时机太早（那时抢还没发生），所以放在 Tick 里周期性检查。
	// 正常对局里这个检查什么也不会做（每个 PC 都有 Pawn），只有异常时才动手。
	{
		static double LastPawnRepairTime = 0.0;
		const double NowSeconds = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0;
		if (NowSeconds - LastPawnRepairTime > 0.5)
		{
			LastPawnRepairTime = NowSeconds;
			RepairPlayerPawnOwnership();
		}
	}
	PushShopStateToPlayers();

	// 武器加成快照：把每个玩家属性集里的 12 个加成，推到**他自己角色**身上的复制字段。
	//
	// 为什么需要这一层：武器在客户端也要按同样的加成算表现，而客户端读不到服务端算好的
	// 武器生效值，只能自己重算一遍 —— 所以需要一份跨端一致的"加成输入"。
	// 真相源仍然是属性集（升级 GE 写入），这里只做"变化才推"的同步：没变化时在
	// ApplyWeaponBonusSnapshot 的脏检查里直接返回，代价是每帧 O(玩家数) 次 12 浮点比较。
	//
	// 载体选角色而不是 PlayerState：角色自己的复制属性能到客户端（bDead 已验证），
	// 且这份数据本来就是"角色的战斗参数"，放在角色上分层更合理。
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

	// 一次性写整份快照，而不是逐个字段设 —— 避免客户端读到"波次已更新、击杀数还没更"的中间态。
	RogueGameState->SetRunStateSnapshot(
		RunState.CurrentWave,
		RunState.EnemiesDefeated,
		RunState.RunTimeSeconds,
		RunState.bGameOver,
		RunState.bBossAlive);
}

void ARogueGameMode::PushWeaponBonusSnapshots()
{
	TArray<ARogueCharacter*, TInlineAllocator<4>> Players;
	CollectAlivePlayers(Players);

	for (ARogueCharacter* Character : Players)
	{
		ARoguePlayerState* PlayerState = Character != nullptr ? Character->GetPlayerState<ARoguePlayerState>() : nullptr;
		const URogueAttributeSet* AttributeSet = PlayerState != nullptr ? PlayerState->GetAttributeSet() : nullptr;
		if (PlayerState == nullptr || AttributeSet == nullptr)
		{
			continue;
		}

		FRogueWeaponBonusValues Values;
		Values.AttackPower = AttributeSet->GetAttackPowerBonus();
		Values.AttackSpeed = AttributeSet->GetAttackSpeedBonus();
		Values.AttackRange = AttributeSet->GetAttackRangeBonus();
		Values.ProjectileSpeed = AttributeSet->GetProjectileSpeedBonus();
		Values.ProjectileCount = AttributeSet->GetProjectileCountBonus();
		Values.ScytheCount = AttributeSet->GetScytheCountBonus();
		Values.RocketCount = AttributeSet->GetRocketCountBonus();
		Values.LaserCount = AttributeSet->GetLaserCountBonus();
		Values.HellTowerCount = AttributeSet->GetHellTowerCountBonus();
		Values.MortarCount = AttributeSet->GetMortarCountBonus();
		Values.LaserRefraction = AttributeSet->GetLaserRefractionBonus();
		Values.MortarBlastRadius = AttributeSet->GetMortarBlastRadiusBonus();

		Character->ApplyWeaponBonusSnapshot(Values);
	}
}

void ARogueGameMode::CollectAlivePlayers(TArray<ARogueCharacter*, TInlineAllocator<4>>& OutPlayers) const
{
	// 实现统一在 RoguePlayerQuery 里（敌人索敌、刷怪点检查、经验球磁吸共用同一份口径）
	RoguePlayerQuery::CollectAlivePlayers(GetWorld(), OutPlayers);
}

void ARogueGameMode::TickRunSystems(float DeltaSeconds)
{
	RunState.RunTimeSeconds += DeltaSeconds;

	// 生成调度 —— 委托给 SpawnSubsystem
	if (URogueSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<URogueSpawnSubsystem>())
	{
		SpawnSubsystem->TickSpawning(DeltaSeconds, RunState, GetSpawnReferenceCharacter(), SpawnedArena.Get());
	}
}