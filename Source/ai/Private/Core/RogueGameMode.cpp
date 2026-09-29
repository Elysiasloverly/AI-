#include "Core/RogueGameMode.h"
#include "Core/RogueSelfTest.h"
#include "Core/RogueGameplayTags.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "Core/RogueTypes.h"
#include "AbilitySystem/RogueAttributeSet.h"
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
#include "Core/RogueGameBalanceAsset.h"
#include "Core/RogueUpgradeDefinitionAsset.h"
#include "Core/RogueUpgradeRuleAsset.h"
#include "Core/RogueGameModeRules.h"
#include "Core/RogueGameState.h"
#include "Core/RoguePlayerQuery.h"
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
	RunWeaponUpgradeSelfTest();

	// 倒地等待 → 复活的**兜底驱动**。
	//
	// 为什么 GameMode 也要管这件事：复活原本只靠两处（角色 Tick 的截止时刻轮询 + 角色身上的定时器），
	// 实测这两处都不生效（日志里 "进入倒地等待" 有、"已复活" 一次都没有），
	// 而"倒下的角色自己不会再动"恰恰是最容易让 Tick 轮询失效的场景。
	// GameMode 是服务端唯一确定每帧都在跑的东西（连暂停看门狗都靠它），
	// 所以这里再放一条独立的路：谁到点了就复活谁。
	//
	// ⚠️ 必须放在 `if (RunState.bGameOver) return;` **之前**：
	// 按设计（规则 A）全员同时倒地只意味着"本局结束、不再刷怪"，
	// 每个人的 10 秒倒地等待仍然要照常结束 —— 否则最后倒下的人会永远躺着。
	TickReviveQueue();

	if (RunState.bGameOver)
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

void ARogueGameMode::RunWeaponUpgradeSelfTest()
{
	if (!RogueSelfTest::IsWeaponUpgradeChainEnabled() || SelfTestPhase >= 5)
	{
		return;
	}

	TArray<ARogueCharacter*, TInlineAllocator<4>> Players;
	CollectAlivePlayers(Players);
	if (Players.Num() == 0)
	{
		// 等玩家就绪；下一帧再试
		return;
	}

	const UWorld* World = GetWorld();
	const float NowSeconds = World != nullptr ? World->GetTimeSeconds() : 0.0f;

	// ⚠️⚠️ 这个自检踩过一个很贵的坑，务必保持"每个玩家都施加、且带玩家名"：
	//
	//   最初只对 Players[0] 施加升级，而 Players[0] 是**主机**玩家；
	//   客户端自检打印的是**它自己**那个玩家的属性 —— 那个玩家从来没被升级过，
	//   于是客户端永远是 0.00。两边比的根本不是同一个玩家，结论"属性没复制到客户端"
	//   就是这么得出来的（错的）。现在：①全员施加；②每行都带玩家名，
	//   服务端/客户端的同一玩家可以直接对齐比较。
	auto PlayerName = [](const ARogueCharacter* InCharacter) -> FString
	{
		if (InCharacter == nullptr)
		{
			return TEXT("<null>");
		}
		const APlayerState* PS = InCharacter->GetPlayerState();
		return PS != nullptr ? PS->GetPlayerName() : TEXT("?");
	};

	auto DumpSet = [](const TCHAR* Stage, const ARogueCharacter* InCharacter)
	{
		if (InCharacter == nullptr)
		{
			return;
		}
		const ARoguePlayerState* PS = InCharacter->GetPlayerState<ARoguePlayerState>();
		const URogueAttributeSet* Set = PS != nullptr ? PS->GetAttributeSet() : nullptr;
		const FRogueWeaponBonusValues& Snap = InCharacter->GetWeaponBonusSnapshot();
		const FString Name = InCharacter->GetPlayerState() != nullptr
			? InCharacter->GetPlayerState()->GetPlayerName() : FString(TEXT("?"));

		UE_LOG(LogTemp, Log,
			TEXT("[SelfTest] %s | 玩家=%s 本机主控=%d | 生命=%.1f/%.1f 护甲=%.1f/%.1f"),
			Stage, *Name, InCharacter->IsLocallyControlled() ? 1 : 0,
			Set != nullptr ? Set->GetHealth() : -1.0f,
			Set != nullptr ? Set->GetMaxHealth() : -1.0f,
			Set != nullptr ? Set->GetArmor() : -1.0f,
			Set != nullptr ? Set->GetMaxArmor() : -1.0f);

		UE_LOG(LogTemp, Log,
			TEXT("[SelfTest] %s | 玩家=%s | 属性集加成: 伤害=%.2f 攻速=%.2f 范围=%.2f 弹速=%.2f 数量=%.2f"),
			Stage, *Name,
			Set != nullptr ? Set->GetAttackPowerBonus() : -1.0f,
			Set != nullptr ? Set->GetAttackSpeedBonus() : -1.0f,
			Set != nullptr ? Set->GetAttackRangeBonus() : -1.0f,
			Set != nullptr ? Set->GetProjectileSpeedBonus() : -1.0f,
			Set != nullptr ? Set->GetProjectileCountBonus() : -1.0f);

		UE_LOG(LogTemp, Log,
			TEXT("[SelfTest] %s | 玩家=%s | 角色快照: 伤害=%.2f 攻速=%.2f 范围=%.2f 弹速=%.2f 数量=%.2f"),
			Stage, *Name,
			Snap.AttackPower, Snap.AttackSpeed, Snap.AttackRange, Snap.ProjectileSpeed, Snap.ProjectileCount);
	};

	auto DumpWeapons = [](const TCHAR* Stage, const ARogueCharacter* InCharacter)
	{
		if (InCharacter == nullptr)
		{
			return;
		}
		const FString Name = InCharacter->GetPlayerState() != nullptr
			? InCharacter->GetPlayerState()->GetPlayerName() : FString(TEXT("?"));
		for (const ARogueWeaponBase* Weapon : InCharacter->GetWeapons())
		{
			if (!IsValid(Weapon))
			{
				continue;
			}
			UE_LOG(LogTemp, Log, TEXT("[SelfTest] %s | 玩家=%s | %-26s 数量=%d 伤害=%.2f 冷却=%.2f 范围=%.1f"),
				Stage, *Name,
				*Weapon->GetClass()->GetName(),
				Weapon->GetEffectiveCount(), Weapon->GetEffectiveDamage(),
				Weapon->GetEffectiveCooldown(), Weapon->GetEffectiveRange());
		}
	};

	auto DumpAll = [&Players, &DumpSet, &DumpWeapons](const TCHAR* Stage)
	{
		for (const ARogueCharacter* Player : Players)
		{
			DumpSet(Stage, Player);
			DumpWeapons(Stage, Player);
		}
	};

	// 把属性集里的 12 个加成立刻推成角色快照（不然本帧日志看到的快照还是旧值，要等下一帧 Tick）
	auto PushSnapshots = [&Players]()
	{
		for (ARogueCharacter* Player : Players)
		{
			const ARoguePlayerState* PS = Player->GetPlayerState<ARoguePlayerState>();
			const URogueAttributeSet* Set = PS != nullptr ? PS->GetAttributeSet() : nullptr;
			if (Set == nullptr)
			{
				continue;
			}
			FRogueWeaponBonusValues Values;
			Values.AttackPower = Set->GetAttackPowerBonus();
			Values.AttackSpeed = Set->GetAttackSpeedBonus();
			Values.AttackRange = Set->GetAttackRangeBonus();
			Values.ProjectileSpeed = Set->GetProjectileSpeedBonus();
			Values.ProjectileCount = Set->GetProjectileCountBonus();
			Values.ScytheCount = Set->GetScytheCountBonus();
			Values.RocketCount = Set->GetRocketCountBonus();
			Values.LaserCount = Set->GetLaserCountBonus();
			Values.HellTowerCount = Set->GetHellTowerCountBonus();
			Values.MortarCount = Set->GetMortarCountBonus();
			Values.LaserRefraction = Set->GetLaserRefractionBonus();
			Values.MortarBlastRadius = Set->GetMortarBlastRadiusBonus();
			Player->ApplyWeaponBonusSnapshot(Values);
		}
	};

	const int32 PlayerCount = Players.Num();

	// ---- 自检要施加的升级内容 ----
	//
	// 每一项都走真实的 ApplyUpgrade → GE → ExecutionCalculation → 属性集。
	//
	// ⚠️ 为什么连 6 种"数量+1"也要施加：武器的"是否开火"判据是生效数量 > 0，
	//    而 DT_WeaponConfig 里多数行的基础数量就是 0（= 未解锁，要靠数量卡解锁）。
	//    只加主武器数量的话，另外 5 把武器全程是"锁着"的状态 —— 日志看起来一切正常，
	//    实际上它们的开火路径（能力 → 目标选择 → 池化弹体）一次都没被验证过。
	//    所以这里把 6 类数量全 +1，让整个武器表都真的跑起来。
	struct FSelfTestCase
	{
		ERogueUpgradeType Type;
		float Magnitude;
		const TCHAR* Label;
	};
	const FSelfTestCase Cases[] =
	{
		{ ERogueUpgradeType::AttackPower,      10.0f, TEXT("伤害+10") },
		{ ERogueUpgradeType::AttackSpeed,       0.1f, TEXT("攻速+0.1") },
		{ ERogueUpgradeType::AttackRange,     100.0f, TEXT("范围+100") },
		{ ERogueUpgradeType::ProjectileSpeed, 200.0f, TEXT("弹速+200") },
		{ ERogueUpgradeType::ProjectileCount,   1.0f, TEXT("主武器数量+1") },
		{ ERogueUpgradeType::ScytheCount,       1.0f, TEXT("镰刀数量+1") },
		{ ERogueUpgradeType::RocketCount,       1.0f, TEXT("火箭数量+1") },
		{ ERogueUpgradeType::LaserCount,        1.0f, TEXT("激光数量+1") },
		{ ERogueUpgradeType::HellTowerCount,    1.0f, TEXT("地狱塔数量+1") },
		{ ERogueUpgradeType::MortarCount,       1.0f, TEXT("迫击炮数量+1") },
	};

	auto ApplyStandardUpgrades = [&Cases, &PlayerName](ARogueCharacter* InCharacter, const TCHAR* Stage, bool bLogEach)
	{
		if (InCharacter == nullptr)
		{
			return;
		}

		const ARoguePlayerState* PS = InCharacter->GetPlayerState<ARoguePlayerState>();
		const URogueAttributeSet* Set = PS != nullptr ? PS->GetAttributeSet() : nullptr;

		for (const FSelfTestCase& TestCase : Cases)
		{
			FRogueUpgradeOption Option;
			Option.Type = TestCase.Type;
			Option.Magnitude = TestCase.Magnitude;
			Option.Title = TestCase.Label;
			InCharacter->ApplyUpgrade(Option);

			if (bLogEach && Set != nullptr)
			{
				UE_LOG(LogTemp, Log, TEXT("[SelfTest] %s | 施加【%s】后 | 玩家=%s | 属性集：伤害=%.2f 攻速=%.2f 范围=%.2f 弹速=%.2f 主武器数量=%.2f"),
					Stage, TestCase.Label, *PlayerName(InCharacter),
					Set->GetAttackPowerBonus(), Set->GetAttackSpeedBonus(), Set->GetAttackRangeBonus(),
					Set->GetProjectileSpeedBonus(), Set->GetProjectileCountBonus());
			}
		}
	};

	// 武器的"是否开火"完全由生效数量决定（0 = 锁着），所以这个摘要必须每次都打：
	// 没有它，"某把武器一次都没开火"会被误读成"能力没跑起来"。
	auto DumpWeaponCounts = [&PlayerName](const TCHAR* Stage, const ARogueCharacter* InCharacter)
	{
		if (InCharacter == nullptr)
		{
			return;
		}

		FString Summary;
		for (const ARogueWeaponBase* Weapon : InCharacter->GetWeapons())
		{
			if (!IsValid(Weapon))
			{
				continue;
			}
			Summary += FString::Printf(TEXT("%s=%d "),
				*Weapon->GetClass()->GetName().Replace(TEXT("RogueWeapon_"), TEXT("")),
				Weapon->GetEffectiveCount());
		}

		UE_LOG(LogTemp, Log, TEXT("[SelfTest] %s | 玩家=%s | 各武器生效数量：%s"),
			Stage, *PlayerName(InCharacter), *Summary);
	};

	switch (SelfTestPhase)
	{
	// ---- 阶段 2/4：在"所有人都已经连上"之后改属性 ----
	//
	// 为什么必须有这一步：客户端总是在第一阶段之后才连上，所以第一阶段只能证明
	// "初始同步带了这些属性"。只有在一端已经连上、看着数值**变化**，才能证明后续变化能到。
	case 1:
	case 3:
	{
		const bool bSecondPhase = (SelfTestPhase == 1);
		const float WaitSeconds = bSecondPhase ? 20.0f : 8.0f;
		if (PlayerCount < 2 || NowSeconds < SelfTestPhaseTime + WaitSeconds)
		{
			return;
		}

		SelfTestPhase += 1;
		SelfTestPhaseTime = NowSeconds;

		FRogueUpgradeOption LateOption;
		LateOption.Type = ERogueUpgradeType::AttackPower;
		LateOption.Magnitude = 5.0f;
		LateOption.Title = TEXT("在线补一刀");

		UE_LOG(LogTemp, Log, TEXT("[SelfTest] ==== 第%s阶段：%d 个玩家**都在线** → 全员【伤害加成+5】%s ===="),
			bSecondPhase ? TEXT("二") : TEXT("三"), PlayerCount,
			bSecondPhase ? TEXT("并各挨 7 点伤害 + 全套升级（含 6 类数量解锁）") : TEXT("（只改加成，不动生命）"));

		for (ARogueCharacter* Player : Players)
		{
			// 第二阶段补一次"全套升级"：后连上的玩家在第一阶段时还不存在，
			// 所以它的武器全是锁着的（生效数量 0），那样就验证不到它的开火路径。
			if (bSecondPhase)
			{
				ApplyStandardUpgrades(Player, TEXT("第二阶段"), /*bLogEach=*/false);
			}

			Player->ApplyUpgrade(LateOption);
			if (bSecondPhase)
			{
				// 同时改一个**老属性**（生命），作对照组：一次运行就能区分
				//   · "新属性（武器加成）不复制" —— 生命跟着变、加成不变；
				//   · "这套属性集的任何变化都到不了客户端" —— 生命也不变。
				RogueAbilitySystem::ApplyDamageToTarget(Player, Player, 7.0f, RogueGameplayTags::Damage_Source_Contact);
			}
		}

		PushSnapshots();
		for (const ARogueCharacter* Player : Players)
		{
			DumpWeaponCounts(bSecondPhase ? TEXT("第二阶段后") : TEXT("第三阶段后"), Player);
		}
		DumpAll(bSecondPhase ? TEXT("第二阶段后") : TEXT("第三阶段后"));
		return;
	}

	// ---- 阶段 3/5：等复制落定后打印"服务端地面真值"，供与各客户端日志逐行对比 ----
	case 2:
	case 4:
	{
		if (NowSeconds < SelfTestPhaseTime + 3.0f)
		{
			return;
		}

		SelfTestPhase += 1;
		SelfTestPhaseTime = NowSeconds;

		DumpAll(SelfTestPhase == 3 ? TEXT("第二阶段落定") : TEXT("第三阶段落定"));
		if (SelfTestPhase >= 5)
		{
			UE_LOG(LogTemp, Log, TEXT("[SelfTest] ==== 自检全部完成 ===="));
			UE_LOG(LogTemp, Log, TEXT("[SelfTest] 判据：客户端日志里**同名玩家**的『属性集加成』『角色快照』『武器生效值』"));
			UE_LOG(LogTemp, Log, TEXT("[SelfTest]       必须与服务端此处逐字一致（伤害加成 15→20、武器伤害随之 +5）。"));

			// 敌人攻击能力的验证探针放在**自检全部完成之后**：
			// 它们会故意改变场上状态（瞬移敌人、临时刷一只很硬的震荡柱），
			// 混在前面会干扰"加成数值逐行对比"那部分自检。
			//
			// 为什么非要有探针：这三条攻击路径的触发条件（贴脸 / 进射程 / 起波时机）
			// 在无头测试里几乎撞不上 —— 武器会在敌人靠近前把它打死。
			// 只统计次数的话，"没实现"和"没机会触发"看起来完全一样（实测两者都是 0）。
			TArray<ARogueCharacter*, TInlineAllocator<4>> ProbeTargets;
			CollectAlivePlayers(ProbeTargets);
			if (ProbeTargets.Num() > 0 && !bSelfTestProbesDone)
			{
				bSelfTestProbesDone = true;
				ProbeContactAttack(ProbeTargets[0]);
				ProbeGroundWaveAttack(ProbeTargets[0]);
				ProbeDeathAndRevive(ProbeTargets[0]);
			}
		}
		return;
	}
	}

	// ---- 阶段 0→1：首次运行，逐项施加升级 ----
	SelfTestPhase = 1;
	SelfTestPhaseTime = NowSeconds;

	UE_LOG(LogTemp, Log, TEXT("[SelfTest] ==== 武器升级链路自检开始（服务端玩家数=%d）===="), PlayerCount);
	for (const ARogueCharacter* Player : Players)
	{
		DumpWeaponCounts(TEXT("升级前"), Player);
	}
	DumpAll(TEXT("升级前"));

	for (ARogueCharacter* Player : Players)
	{
		const ARoguePlayerState* PS = Player->GetPlayerState<ARoguePlayerState>();
		if (PS == nullptr || PS->GetAttributeSet() == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("[SelfTest] 玩家 %s 的 ASC 上没有属性集 —— 注册失败（见 RoguePlayerState 构造）"), *PlayerName(Player));
			continue;
		}

		ApplyStandardUpgrades(Player, TEXT("第一阶段"), /*bLogEach=*/true);
	}

	PushSnapshots();
	for (const ARogueCharacter* Player : Players)
	{
		DumpWeaponCounts(TEXT("升级后"), Player);
	}
	DumpAll(TEXT("升级后"));
	UE_LOG(LogTemp, Log, TEXT("[SelfTest] ==== 第一阶段完成（等客户端连上后会自动进入第二阶段）===="));
}

/** 探针专用血量：高到足以活过"下一次决策 + 地面波初始冷却"。
 *  为什么必须调高：两条攻击路径的触发都需要敌人活到那个时候，而武器一轮齐射就能秒掉普通怪 ——
 *  这个坑实测踩过两次（接触 / 地面波计数都是 0，看起来像"能力没实现"）。 */
static constexpr float ProbeEnemyHealth = 20000.0f;

void ARogueGameMode::ProbeDeathAndRevive(ARogueCharacter* Target)
{
	if (Target == nullptr || Target->IsDead())
	{
		UE_LOG(LogTemp, Log, TEXT("[SelfTest] 死亡/复活验证：目标不可用（已倒地或为空），跳过"));
		return;
	}

	// 为什么要主动打死一个玩家：现在的无头自检里武器太强，敌人根本贴不上来，
	// 整局 0 次倒地 —— 于是"倒地等待 GE → 复活 → 复活无敌"这条链路一次都不会被执行，
	// 改了也验证不到。这里直接施加一次致命伤害，让它走真实的死亡流程
	// （伤害 → 血量归零 → 死亡 GE → Die() → ScheduleRevive）。
	RogueAbilitySystem::ApplyDamageToTarget(Target, Target, 1000000.0f, RogueGameplayTags::Damage_Source_Contact);

	const float DownedRemaining = Target->GetReviveSecondsRemaining();
	UE_LOG(LogTemp, Log,
		TEXT("[SelfTest] 死亡/复活验证：已对 %s 施加致命伤害 | 倒地=%d 待复活=%d 倒地等待剩余=%.2f 秒（预期后续看到『已复活』）"),
		Target->GetPlayerState() != nullptr ? *Target->GetPlayerState()->GetPlayerName() : *Target->GetName(),
		Target->IsDead() ? 1 : 0,
		Target->IsRevivePending() ? 1 : 0,
		DownedRemaining);
}

void ARogueGameMode::ProbeContactAttack(ARogueCharacter* Target)
{
	if (Target == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	const URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	if (Tracker == nullptr)
	{
		return;
	}

	// 找离目标玩家最近的一只活着的敌人
	ARogueEnemy* Nearest = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	const FVector TargetLocation = Target->GetActorLocation();
	for (const TWeakObjectPtr<ARogueEnemy>& WeakEnemy : Tracker->GetActiveEnemies())
	{
		ARogueEnemy* Enemy = WeakEnemy.Get();
		if (!IsValid(Enemy) || Enemy->IsDead() || Enemy->IsAvailableInPool())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Enemy->GetActorLocation(), TargetLocation);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Nearest = Enemy;
		}
	}

	if (Nearest == nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[SelfTest] 接触攻击验证：场上一只可用敌人都没有，跳过（这条路径本次没被验证）"));
		return;
	}

	const FString TargetName = Target->GetPlayerState() != nullptr ? Target->GetPlayerState()->GetPlayerName() : Target->GetName();

	// 先把这只怪改硬：接触攻击要等它下一次 Tick 才判定，而武器一轮齐射就能把它秒了 ——
	// 实测这就是"瞬移过去了但『接触』计数还是 0"的真正原因（不是能力没跑）。
	// 直接写属性基值：这是自检探针，允许改数值，但不改任何玩法逻辑。
	if (UAbilitySystemComponent* EnemyASC = Nearest->GetAbilitySystemComponent())
	{
		EnemyASC->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(), ProbeEnemyHealth);
	}

	Nearest->SetActorLocation(TargetLocation, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	// 诊断一次到位：把"决策为什么可能不触发"的前置条件全打出来。
	// 踩过的坑：只看计数时，"瞬移过去了但计数还是 0"完全无法定位原因。
	const UFunction* BehaviorFunction = Nearest->GetClass()->FindFunctionByName(TEXT("OnBehaviorTick"));
	const bool bBlueprintOverridesBehavior =
		BehaviorFunction != nullptr && BehaviorFunction->GetOuter() != ARogueEnemy::StaticClass();

	UE_LOG(LogTemp, Log,
		TEXT("[SelfTest] 接触攻击验证：已把敌人 %s（类=%s）瞬移到玩家 %s 身上 | 距离=%.1f 判定HasTouching=%d 接触能力=%s 蓝图重写了行为Tick=%d"),
		*Nearest->GetName(), *Nearest->GetClass()->GetName(), *TargetName,
		FVector::Dist2D(Nearest->GetActorLocation(), TargetLocation),
		Nearest->HasTouchingPlayer() ? 1 : 0,
		Nearest->ContactAttackAbilityHandle.IsValid() ? TEXT("已授予") : TEXT("未授予"),
		bBlueprintOverridesBehavior ? 1 : 0);
}

void ARogueGameMode::ProbeGroundWaveAttack(ARogueCharacter* Target)
{
	if (Target == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	URogueCombatPoolSubsystem* Pools = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>();
	if (Pools == nullptr)
	{
		return;
	}

	// 直接用池生成一只震荡柱放在玩家附近。
	// 为什么不用常规刷怪流程：刷怪按波次/权重选类型，短时间内不一定会出震荡柱，
	// 于是"地面波攻击能力"这条路径可能整场都没被验证过（实测跑了 2 分钟一次都没出）。
	const FVector SpawnLocation = Target->GetActorLocation() + FVector(600.0f, 0.0f, 0.0f);
	ARogueEnemy* Enemy = Pools->AcquireEnemy(ARogueEnemy::StaticClass(), nullptr, SpawnLocation, FRotator::ZeroRotator);
	if (Enemy == nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[SelfTest] 地面波验证：池里取不到敌人，跳过"));
		return;
	}

	FRogueEnemyProfile Profile;
	Profile.EnemyType = ERogueEnemyType::ShockPillar;
	// 够硬才能活到"起波"那一刻：地面波的初始冷却是随机的（实测这次 2.24 秒），
	// 而 500 血在六把武器面前只够活 2 秒左右 —— 于是永远等不到起波，计数一直是 0。
	Profile.Health = ProbeEnemyHealth;
	Profile.Speed = 0.0f;
	Profile.Damage = 5.0f;
	Profile.ExperienceReward = 0;
	Profile.bIsBoss = false;

	Enemy->ActivatePooledEnemy(nullptr, SpawnLocation, FRotator::ZeroRotator);
	Enemy->InitializeEnemy(Profile);

	if (URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>())
	{
		Tracker->RegisterEnemy(Enemy);
	}

	UE_LOG(LogTemp, Log, TEXT("[SelfTest] 地面波验证：已在玩家 %s 附近生成一只震荡柱（%s），等它起波"),
		Target->GetPlayerState() != nullptr ? *Target->GetPlayerState()->GetPlayerName() : *Target->GetName(),
		*Enemy->GetName());

	// 诊断：能力是否真的授予了、原型里地面波开关是不是开的、决策用的初始冷却还剩多久。
	UE_LOG(LogTemp, Log,
		TEXT("[SelfTest]   震荡柱诊断：地面波开关=%d 攻击能力=%s 初始冷却=%.2f 距离=%.1f 扩展半径参数=%.1f 可命中跳跃=%d"),
		Enemy->GetCurrentArchetype().GroundWave.bUsesGroundWave ? 1 : 0,
		Enemy->GroundWaveAttackAbilityHandle.IsValid() ? TEXT("已授予") : TEXT("未授予"),
		Enemy->GetGroundWaveAttackCooldownRemaining(),
		FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation()),
		Enemy->GetCurrentArchetype().GroundWave.MaxRadius,
		Enemy->GetCurrentArchetype().GroundWave.bJumpCanDodge ? 1 : 0);
}

void ARogueGameMode::TickReviveQueue()
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 兜底驱动：遍历控制器 → Pawn → 角色，让每个还在倒地等待的角色自己判到期没。
	//
	// 迁移前这里读的是"复制在 PlayerState 上的复活截止时刻"。现在"还剩多久"是
	// 玩家 ASC 上那个带时长的 State.Downed GE，判据统一收在 ARogueCharacter::TryReviveIfExpired()：
	// 三个驱动（角色 Tick / 本函数 / 角色上的定时器）走同一个闸门，不会再出现
	// "某个驱动用了另一套判据、于是行为不一致"。
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController == nullptr)
		{
			continue;
		}

		ARogueCharacter* Character = Cast<ARogueCharacter>(PlayerController->GetPawn());
		if (Character == nullptr || !Character->IsRevivePending())
		{
			continue;
		}

		Character->TryReviveIfExpired();
	}
}

ARogueCharacter* ARogueGameMode::FindNearestAlivePlayer(const FVector& Location) const
{
	TArray<ARogueCharacter*, TInlineAllocator<4>> AlivePlayers;
	CollectAlivePlayers(AlivePlayers);

	ARogueCharacter* Nearest = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (ARogueCharacter* Candidate : AlivePlayers)
	{
		const float DistanceSquared = FVector::DistSquared2D(Location, Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			Nearest = Candidate;
		}
	}

	return Nearest;
}

ARogueCharacter* ARogueGameMode::GetSpawnReferenceCharacter() const
{
	// 随机取一个活着的玩家当刷怪参照点（为什么不固定用玩家 0，见头文件里的说明）。
	TArray<ARogueCharacter*, TInlineAllocator<4>> AlivePlayers;
	CollectAlivePlayers(AlivePlayers);

	if (AlivePlayers.Num() == 0)
	{
		// 全员阵亡（或还没生成角色）：返回 nullptr。
		// SpawnEnemy 会因 !IsValid(Character) 直接拒绝 —— 这正是想要的"团灭就别刷了"。
		return nullptr;
	}

	return AlivePlayers[FMath::RandRange(0, AlivePlayers.Num() - 1)];
}

void ARogueGameMode::AdvanceShopAutoRefresh(float DeltaSeconds, bool bWorldPaused)
{
	// 世界暂停时商店计时冻结。
	// 这里原来还有一条"商店打开 + 世界暂停就用真实时间继续推进"的分支 ——
	// 那是为"开商店会暂停游戏"的旧设计准备的；现在商店不再暂停游戏，
	// 而暂停菜单又不能和商店同时打开（TogglePauseMenu 里挡了），所以那条分支已不可达。
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

void ARogueGameMode::TickRunSystems(float DeltaSeconds)
{
	RunState.RunTimeSeconds += DeltaSeconds;

	// 生成调度 —— 委托给 SpawnSubsystem
	if (URogueSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<URogueSpawnSubsystem>())
	{
		SpawnSubsystem->TickSpawning(DeltaSeconds, RunState, GetSpawnReferenceCharacter(), SpawnedArena.Get());
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
		AwardEnemyCurrency(Enemy, ResolveRewardCharacter(Killer, Enemy));
		SpawnExperienceReward(Enemy, Tracker, Pools);

		if (Enemy->IsBoss())
		{
			HandleBossKilledReward();
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

	// 3) 兜底：离现场最近的存活玩家。
	// 旧实现回退到 CachedCharacter（= 玩家 0 / 主机），玩家 2 打死的怪赏金会进主机口袋。
	// 单机时两者结果一致（场上只有一个人）。
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

void ARogueGameMode::HandleBossKilledReward()
{
	RunState.bBossAlive = false;

	// Boss 奖励是**每个玩家都有份**的。
	// 旧实现只发给 GetCachedCharacter()（联机下就等于主机），
	// 结果是打完 Boss 只有主机能选升级，其他玩家什么都没弹。
	// 阵亡玩家不弹选择：他已经出局，弹一个点不了的框只会让人以为是 bug。
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

	Character->SetMenuInteractionInput(true);

	// 立刻推一次快照：不然客户端要等到下一帧 Tick 才看到商店界面
	PushShopSnapshotForPlayer(PlayerState);

	// 注意：这里**不暂停游戏**。
	// 旧设计是"开商店 → SetGamePaused(true)"（单机写法）。联机下暂停是全局的，
	// 一个玩家开商店会把所有人的世界冻住。现在改成非模态：
	// 世界继续跑、敌人继续打，玩家可以边逛边躲（SetMenuInteractionInput 用的是
	// FInputModeGameAndUI，游戏输入仍然有效）。
}

void ARogueGameMode::CloseShop(ARogueCharacter* Requester)
{
	ARoguePlayerState* PlayerState = IsValid(Requester) ? Requester->GetPlayerState<ARoguePlayerState>() : nullptr;
	if (PlayerState == nullptr || !PlayerState->GetShopRuntime().IsOpen())
	{
		return;
	}

	PlayerState->GetShopRuntime().CloseShop();
	Requester->SetMenuInteractionInput(false);
	PushShopSnapshotForPlayer(PlayerState);

	// 同样不涉及暂停（见 TryOpenShop 的说明）
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

	// 复制的是**自动刷新的截止时刻**而不是倒计时：倒计时每帧都在变，
	// 会把 PlayerState 的复制频率打满；截止时刻只在刷新时变化。
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
	// 前者在服务端任何时刻都可靠，也不依赖 GameState 的复制时机。
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

void ARogueGameMode::NotifyPlayerDied(ARogueCharacter* DeadCharacter)
{
	// 强制纠偏：有人死亡**绝不能暂停世界**。
	//
	// C++ 侧早已清掉了所有 SetGamePaused(true)，但实测仍有"一个玩家死后整局停住"的现象 ——
	// 那是死亡界面的蓝图（为旧的单机设计写的）在调 Set Game Paused。
	// 这里每次有人死亡都无条件恢复运行：不管是谁暂停的，都不允许它影响队友。
	// （暂停菜单与商店本身也不暂停世界，见 TogglePauseMenu / TryOpenShop 的注释。）
	if (UWorld* World = GetWorld())
	{
		if (World->IsPaused())
		{
			UGameplayStatics::SetGamePaused(this, false);
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 检测到死亡时世界处于暂停：已强制恢复（暂停很可能来自死亡界面蓝图，建议改掉）"));
		}
	}
	// 先判断"这一局是不是所有人都死了"。
	// 时机很关键：ARogueCharacter::Die() 会先把自己标成 bDead 再调这里，
	// 所以 CollectAlivePlayers 不会把刚死的这个人算进去。
	TArray<ARogueCharacter*, TInlineAllocator<4>> AlivePlayers;
	CollectAlivePlayers(AlivePlayers);
	const bool bRunOver = AlivePlayers.Num() == 0;

	// 死者的收尾：他没选的升级卡作废（人已经出局），商店关掉，
	// 并给他 UI 输入模式，让他能点结算界面。
	//
	// 注意**只处理他自己**：旧实现顺手把所有人的升级/商店状态一起清了，
	// 于是队友还活着却平白丢掉排队的升级。
	if (IsValid(DeadCharacter))
	{
		if (ARoguePlayerState* DeadPlayerState = DeadCharacter->GetPlayerState<ARoguePlayerState>())
		{
			DeadPlayerState->ResetUpgradeState();
			DeadPlayerState->GetShopRuntime().CloseShop();
			DeadPlayerState->ApplyShopSnapshot(false, TArray<FRogueShopOffer>(), 0, 0.0f);
		}

		DeadCharacter->SetMenuInteractionInput(true);
	}

	if (!bRunOver)
	{
		// 还有队友活着 → 本局继续（波次、刷怪、商店、升级照常）。
		return;
	}

	// 全员阵亡：整局结束。
	RunState.bGameOver = true;
	RunState.bBossAlive = false;

	// 这时才清所有人的状态 —— 大家都出局了，没有"队友的进度"需要保。
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* PlayerController = It->Get())
		{
			if (ARoguePlayerState* RoguePlayerState = PlayerController->GetPlayerState<ARoguePlayerState>())
			{
				RoguePlayerState->ResetUpgradeState();
				RoguePlayerState->GetShopRuntime().CloseShop();
				RoguePlayerState->ApplyShopSnapshot(false, TArray<FRogueShopOffer>(), 0, 0.0f);
			}

			// 注意：这里**不再取消**待复活。
			// 设计上"倒下"是一个 10 秒的等待状态（无法控制、不被索敌、也会被队友看到躺在地上），
			// 到时一定回到正常状态；一旦取消，最后倒下的人就会永远躺在地上，
			// 观感正是之前反复报的"卡死/暂停"。
			// 本局"结束"只意味着不再刷怪 + 显示结算，玩家仍然可以各自复活，也可以点重开。
		}
	}
}

bool ARogueGameMode::SetPause(APlayerController* PC, FCanUnpause CanUnpauseDelegate)
{
	// 拒绝一切暂停请求（详见头文件）。打日志是为了把"谁在暂停"暴露出来 ——
	// 这条日志里的 PC 名（或为空）就是定位那个蓝图的线索。
	UE_LOG(LogTemp, Warning, TEXT("[Rogue] 收到暂停请求（PC=%s）：本作不支持暂停，已拒绝。若你看到这条日志，请在蓝图里搜索 Set Game Paused / SetPause 节点并删除"),
		PC != nullptr ? *PC->GetName() : TEXT("未知（可能来自控制台或关卡蓝图）"));
	return false;
}

void ARogueGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 见头文件说明：引擎登录流程会让在场 PC 短暂抢走新玩家的 Pawn，
	// 导致在场 PC 自己失去 Pawn（主机失控）。这里做一次权威校正。
	RepairPlayerPawnOwnership();
}

void ARogueGameMode::RepairPlayerPawnOwnership()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (PlayerController == nullptr || PlayerController->GetPawn() != nullptr)
		{
			// 已经有 Pawn 的不用管（正常情况）
			continue;
		}

		APlayerState* OwningPlayerState = PlayerController->PlayerState;

		// 首选：把"被抢走又被还回来"的那个 Pawn 直接认领回来。
		// 这比重新生成更对：既不留下没人控制的孤儿角色，也不会让玩家位置突变。
		if (const ARoguePlayerController* RoguePlayerController = Cast<ARoguePlayerController>(PlayerController))
		{
			APawn* ReclaimCandidate = RoguePlayerController->GetLastUnpossessedPawn();
			if (IsValid(ReclaimCandidate) && ReclaimCandidate->GetController() == nullptr)
			{
				PlayerController->Possess(ReclaimCandidate);
				UE_LOG(LogTemp, Warning, TEXT("[Rogue] 归属校正：%s 认领回原角色 %s"),
					*PlayerController->GetName(), *ReclaimCandidate->GetName());
				continue;
			}
		}

		if (OwningPlayerState == nullptr)
		{
			continue;
		}

		// 找"本命角色"：同一份 PlayerState、且当前没有控制器
		for (TActorIterator<ARogueCharacter> CharacterIt(World); CharacterIt; ++CharacterIt)
		{
			ARogueCharacter* Candidate = *CharacterIt;
			if (Candidate == nullptr || Candidate->GetController() != nullptr)
			{
				continue;
			}

			if (Candidate->GetPlayerState() != OwningPlayerState)
			{
				continue;
			}

			PlayerController->Possess(Candidate);
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 归属校正：%s 重新接管 %s"),
				*PlayerController->GetName(), *Candidate->GetName());
			break;
		}

		// ⚠️ 这里**故意不再调 RestartPlayer**。
		//
		// 实测教训：RestartPlayer 会**再生成一个新角色**，而那个 PC 原来的角色会变成
		// "没有控制器的孤儿"。于是服务端上所有"通过 PlayerController->GetPawn() 取玩家"的代码
		// —— 敌人的目标选择 GatherAlivePlayers、以及本类的 CollectAlivePlayers ——
		// 从此看到的是新角色，玩家实际操控/被瞄准的链条错位，表现为：
		//   · 客户端死亡后怪物仍锁着尸体（存活列表里找不到他）
		//   · 主机死亡后被误判成"全员阵亡"，整局停摆（看起来像暂停）
		// 而"某个 PC 暂时没有 Pawn"最坏只是暂时没有控制权（下次登录/接管会恢复），
		// 危害远小于制造孤儿角色。所以这里只记录，不动手。
		if (PlayerController->GetPawn() == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 归属校正：%s 目前没有 Pawn 且无可认领的原角色（按设计不做重生，避免制造孤儿角色）"),
				*PlayerController->GetName());
		}
	}
}

void ARogueGameMode::HandleRestartRequest(ARoguePlayerState* Requester)
{
	// 只有整局结束（全员阵亡）才允许重开。
	//
	// 为什么服务端必须挡这一下：客户端那个按钮是蓝图界面上摆着的，
	// 只靠 UI 约束等于没有约束 —— 一个已阵亡的玩家在队友还在打的时候点它，
	// 会把整局连人一起重开。
	if (!RunState.bGameOver)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rogue] 拒绝重开请求：本局尚未结束（请求者=%s）"),
			Requester != nullptr ? *Requester->GetPlayerName() : TEXT("未知"));
		return;
	}

	// 一次性闸门：OpenLevel/ServerTravel 是很重的操作，
	// 结算界面上的按钮如果被连点（或被改过的客户端反复调 RPC），
	// 服务端会不停切图/卡在加载。第一次请求之后就不再受理。
	if (bRestartRequested)
	{
		return;
	}
	bRestartRequested = true;

	// 由**服务端**执行 OpenLevel：listen server 会顺带把所有客户端一起带到新关卡
	// （server travel），所有人一起重开。
	//
	// 客户端绝不能自己 OpenLevel：那样只会加载出一份单机的关卡副本，
	// 表现为"点了重开之后队友全没了"—— 这正是旧实现（HUD 里直接 OpenLevel）的毛病。
	// 取"可安全 travel 的关卡名"。
	//
	// ⚠️ 这里踩过一个很隐蔽的坑：`GetCurrentLevelName()` 在**联网 PIE** 下会返回带主机前缀的
	// 字符串（实测形如 "127.0.0.1:17777/Game/GameAssets/.../Minimal_Default"）。
	// 直接丢给 OpenLevel，服务器就会去 **Browse 那个地址** —— 结果是整个会话被踢掉：
	// 客户端掉线后收不到任何更新（画面冻结、怪物不动），看起来像"整局暂停"。
	//
	// 注意要**保留包路径、只剥掉主机前缀**：本工程里存在两个都叫 Minimal_Default 的地图
	// （Content/StarterContent 与 Content/GameAssets/StarterContent 各一份），
	// 用最后一段短名会撞名、OpenLevel 可能找不到或找错图。
	FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	const int32 ColonIndex = LevelName.Find(TEXT(":"), ESearchCase::IgnoreCase);
	if (ColonIndex != INDEX_NONE)
	{
		// 形如 "127.0.0.1:17777/Game/..." → 从主机端口后的第一个 '/' 起截断（保留开头的 '/'）
		const int32 SlashAfterHostIndex = LevelName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ColonIndex);
		if (SlashAfterHostIndex != INDEX_NONE)
		{
			LevelName = LevelName.RightChop(SlashAfterHostIndex);
		}
	}

	if (LevelName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rogue] 重开失败：拿不到当前关卡名"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Rogue] 重开关卡：%s"), *LevelName);
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
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
