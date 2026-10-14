// 敌人生成子系统 —— 实现
#include "Enemies/Spawning/RogueSpawnSubsystem.h"

#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "World/RogueArena.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameModeRules.h"
#include "Player/RoguePlayerQuery.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float EnemySpawnGroundZ = 60.0f;

	/**
	 * "这一种敌人现在还能不能刷"的判据（供加权抽取做过滤）。
	 *
	 * 两道闸门，都配在 `DT_FRogueEnemyArchetypeRow` 里，**没填 / 填 0 = 不限制**：
	 *   ① `MaxAlive`        —— 同时存活上限：场上同时最多 N 只，**死掉就腾出名额**（动态）；
	 *   ② `MaxSpawnPerWave` —— 每波刷新上限：本波累计刷出 N 只就没了，**死了也不补**（静态预算）。
	 *   两个都填时谁先到谁生效。
	 *
	 * 为什么包成仿函数而不是就地写 lambda：
	 *  1) 加权抽取会对"每一行波次配置"各问一次，同一类型的上限/存活数会被反复查询，
	 *     所以在一次抽取内缓存结果；
	 *  2) 抽完之后调用方还要用同一套判据复核结果（所有类型都刷满时抽签会兜底成 Hunter），
	 *     复用同一个对象才能保证"过滤"和"复核"是同一份快照。
	 */
	struct FEnemySpawnGate
	{
		const UDataTable* ArchetypeTable = nullptr;
		const URogueEnemyTrackerSubsystem* Tracker = nullptr;
		const TMap<ERogueEnemyType, int32>* SpawnedCountThisWave = nullptr;

		/** 日志去重：同一波里每种怪的每种"被拦原因"只打一条，方便验证配置有没有生效 */
		TSet<ERogueEnemyType>* AliveCapLoggedTypes = nullptr;
		TSet<ERogueEnemyType>* WaveBudgetLoggedTypes = nullptr;

		// 以下三个缓存只在"一次抽取"内有效。
		// 注意 0 是合法的上限值，所以判空只能用 Find，不能拿值判空。
		TMap<ERogueEnemyType, int32> MaxAliveCache;
		TMap<ERogueEnemyType, int32> MaxSpawnPerWaveCache;
		TMap<ERogueEnemyType, int32> AliveCountCache;

		template <typename LookupType>
		int32 LookupCached(TMap<ERogueEnemyType, int32>& Cache, ERogueEnemyType EnemyType, LookupType&& Lookup)
		{
			if (const int32* Cached = Cache.Find(EnemyType))
			{
				return *Cached;
			}

			const int32 Value = Lookup(EnemyType);
			Cache.Add(EnemyType, Value);
			return Value;
		}

		bool operator()(ERogueEnemyType EnemyType)
		{
			// ---- 闸门①：同时存活上限 ----
			const int32 MaxAlive = LookupCached(MaxAliveCache, EnemyType,
				[this](ERogueEnemyType Type) { return RogueEnemyArchetypes::GetMaxAlive(ArchetypeTable, Type); });

			if (MaxAlive > 0 && Tracker != nullptr)
			{
				const int32 Alive = LookupCached(AliveCountCache, EnemyType,
					[this](ERogueEnemyType Type) { return Tracker->GetActiveEnemyCountOfType(Type); });

				if (Alive >= MaxAlive)
				{
					if (ShouldLogOnce(AliveCapLoggedTypes, EnemyType))
					{
						UE_LOG(LogTemp, Log, TEXT("[RogueSpawn] 敌人类型 %d 已达同时存活上限（%d / %d），暂不刷新"),
							static_cast<int32>(EnemyType), Alive, MaxAlive);
					}
					return false;
				}
			}

			// ---- 闸门②：每波累计刷新上限 ----
			const int32 MaxSpawn = LookupCached(MaxSpawnPerWaveCache, EnemyType,
				[this](ERogueEnemyType Type) { return RogueEnemyArchetypes::GetMaxSpawnPerWave(ArchetypeTable, Type); });

			if (MaxSpawn > 0)
			{
				const int32 Spawned = SpawnedCountThisWave != nullptr ? SpawnedCountThisWave->FindRef(EnemyType) : 0;
				if (Spawned >= MaxSpawn)
				{
					if (ShouldLogOnce(WaveBudgetLoggedTypes, EnemyType))
					{
						UE_LOG(LogTemp, Log, TEXT("[RogueSpawn] 敌人类型 %d 本波名额已用完（%d / %d），本波内不再刷新"),
							static_cast<int32>(EnemyType), Spawned, MaxSpawn);
					}
					return false;
				}
			}

			return true;
		}

		/** 每种怪每波每原因只打一条日志。
		 *  这里只回答"该不该打"，UE_LOG 留给调用点写 —— 因为 UE5.5 的格式串必须是编译期常量，
		 *  不能当参数传进来（传进来会触发 FormatStringSan 的 static_assert）。 */
		static bool ShouldLogOnce(TSet<ERogueEnemyType>* LoggedTypes, ERogueEnemyType EnemyType)
		{
			if (LoggedTypes == nullptr || LoggedTypes->Contains(EnemyType))
			{
				return false;
			}

			LoggedTypes->Add(EnemyType);
			return true;
		}
	};
}

void URogueSpawnSubsystem::Configure(
	TSubclassOf<ARogueEnemy> InDefaultEnemyClass,
	const TMap<ERogueEnemyType, TSubclassOf<ARogueEnemy>>& InEnemyClassMap,
	const FRogueSpawnSettings& InSpawnSettings,
	const FRogueBossSettings& InBossSettings,
	URogueGameBalanceAsset* InBalanceAsset,
	UDataTable* InEnemyArchetypeTable)
{
	DefaultEnemyClass = InDefaultEnemyClass;
	EnemyClassMap = InEnemyClassMap;
	SpawnSettings = InSpawnSettings;
	BossSettings = InBossSettings;
	BalanceAsset = InBalanceAsset;
	EnemyArchetypeTable = InEnemyArchetypeTable;
}

void URogueSpawnSubsystem::ResetWaveSpawnCounts(int32 Wave)
{
	// 波次切换 = 每类怪的刷新名额全部回满
	// （同时存活上限不需要重置：它是实时统计场上数量，本来就随死亡自然回落）
	SpawnedCountThisWave.Reset();
	AliveCapLoggedTypes.Reset();
	WaveBudgetLoggedTypes.Reset();
	CountedWave = Wave;
}

void URogueSpawnSubsystem::TickSpawning(float DeltaSeconds, FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena)
{
	RunState.SpawnTimer -= DeltaSeconds;

	const int32 TargetWave = FMath::FloorToInt(RunState.RunTimeSeconds / SpawnSettings.WaveDuration) + 1;
	if (TargetWave != RunState.CurrentWave)
	{
		RunState.CurrentWave = TargetWave;
	}

	// 波次一变就把"本波已刷数量"清零。放在这里而不是 SpawnEnemy 里，
	// 是为了让"跨波"这件事只有一个入口，避免每个生成点都要判一次。
	if (CountedWave != RunState.CurrentWave)
	{
		ResetWaveSpawnCounts(RunState.CurrentWave);
	}

	if (BossSettings.BossWaveInterval > 0 && RunState.CurrentWave > 0 && RunState.CurrentWave % BossSettings.BossWaveInterval == 0 &&
		!RunState.bBossAlive && RunState.LastBossWaveSpawned != RunState.CurrentWave)
	{
		SpawnBoss(RunState, Character, Arena);
	}

	URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	if (Tracker == nullptr)
	{
		return;
	}

	int32 AliveEnemies = Tracker->GetActiveEnemyCount();
	const FRogueSpawnCadence SpawnCadence = RogueGameModeRules::BuildSpawnCadence(RunState.CurrentWave, AliveEnemies, 
		SpawnSettings.MaxAliveEnemies, RunState.bBossAlive, SpawnSettings.SpawnInterval, BalanceAsset);
	if (RunState.SpawnTimer <= 0.0f && AliveEnemies < SpawnCadence.EffectiveMaxAliveEnemies)
	{
		for (int32 SpawnIndex = 0; SpawnIndex < SpawnCadence.SpawnBatchSize && AliveEnemies < SpawnCadence.EffectiveMaxAliveEnemies; ++SpawnIndex)
		{
			if (SpawnEnemy(RunState, Character, Arena))
			{
				++AliveEnemies;
			}
		}

		RunState.SpawnTimer = SpawnCadence.NextSpawnDelay;
	}
}

bool URogueSpawnSubsystem::SpawnEnemy(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena)
{
	if (!IsValid(Character) || Character->IsDead())
	{
		return false;
	}

	// 防御：SpawnEnemy 也可能被别处直接调用（不经过 TickSpawning），
	// 这里保证"跨波清零"一定发生过，不会把上一波的名额带到这一波。
	if (CountedWave != RunState.CurrentWave)
	{
		ResetWaveSpawnCounts(RunState.CurrentWave);
	}

	FEnemySpawnGate SpawnGate;
	SpawnGate.ArchetypeTable = EnemyArchetypeTable;
	SpawnGate.Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	SpawnGate.SpawnedCountThisWave = &SpawnedCountThisWave;
	SpawnGate.AliveCapLoggedTypes = &AliveCapLoggedTypes;
	SpawnGate.WaveBudgetLoggedTypes = &WaveBudgetLoggedTypes;

	const ERogueEnemyType EnemyType = RogueGameModeRules::PickWeightedEnemyTypeForWave(RunState.CurrentWave, BalanceAsset, SpawnGate);

	// 复核：所有类型都被拦下时，抽签函数只能兜底返回 Hunter，这里必须把它拦下来，
	// 否则两道限制都会被兜底路径绕过。
	if (!SpawnGate(EnemyType))
	{
		return false;
	}

	ARogueEnemy* SpawnedEnemy = SpawnConfiguredEnemy(RogueGameModeRules::BuildEnemyProfile(RunState.CurrentWave, EnemyType,
		BalanceAsset), SpawnSettings.SpawnRadius, SpawnSettings.EnemySpawnMinimumDistance, Arena, Character);
	if (SpawnedEnemy == nullptr)
	{
		return false;   // 池子拿不到/生成失败 → 不占名额
	}

	// 只有真正刷出来了才记账：失败还计数会让"名额"被空气吃掉。
	++SpawnedCountThisWave.FindOrAdd(EnemyType);
	return true;
}

void URogueSpawnSubsystem::SpawnBoss(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena)
{
	if (!IsValid(Character) || Character->IsDead())
	{
		return;
	}

	if (SpawnConfiguredEnemy(RogueGameModeRules::BuildBossProfile(RunState.CurrentWave, PickEnemyTypeForCurrentWave(RunState.CurrentWave), 
		BalanceAsset), BossSettings.BossSpawnDistance, BossSettings.BossSpawnMinimumDistance, Arena, Character) != nullptr)
	{
		RunState.bBossAlive = true;
		RunState.LastBossWaveSpawned = RunState.CurrentWave;
	}
}

ARogueEnemy* URogueSpawnSubsystem::SpawnConfiguredEnemy(const FRogueEnemyProfile& EnemyProfile, float DesiredDistance, float MinimumDistance,
	const ARogueArena* Arena, ARogueCharacter* Character)
{
	URogueCombatPoolSubsystem* Pools = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>();
	URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	if (Pools == nullptr || Tracker == nullptr)
	{
		return nullptr;
	}

	const FVector SpawnLocation = FindSpawnLocation(DesiredDistance, MinimumDistance, Arena, Character);
	TSubclassOf<ARogueEnemy> ResolvedClass = ResolveEnemyClass(EnemyProfile.EnemyType);
	UClass* EnemyToSpawn = ResolvedClass ? ResolvedClass.Get() : ARogueEnemy::StaticClass();
	ARogueEnemy* Enemy = Pools->AcquireEnemy(EnemyToSpawn, nullptr, SpawnLocation, FRotator::ZeroRotator);
	if (Enemy == nullptr)
	{
		return nullptr;
	}

	Enemy->ActivatePooledEnemy(nullptr, SpawnLocation, FRotator::ZeroRotator);
	Enemy->InitializeEnemy(EnemyProfile);
	Tracker->RegisterEnemy(Enemy);
	return Enemy;
}

bool URogueSpawnSubsystem::IsSpawnLocationClearOfPlayers(const FVector& CandidateLocation, float MinimumDistance) const
{
	// 死掉的玩家不占出生点：把他算进去会让"没人的那半边场地"也刷不出怪。
	// （口径统一在 RoguePlayerQuery，见那里的说明。）
	return !RoguePlayerQuery::IsAnyAlivePlayerWithin(GetWorld(), CandidateLocation, MinimumDistance);
}

FVector URogueSpawnSubsystem::FindSpawnLocation(float DesiredDistance, //希望离玩家多远（普通怪 1100 / Boss 1800）
	float MinimumDistance,  // 最低容忍距离（这才是"不刷脸上"的硬约束）
	const ARogueArena* Arena, 
	ARogueCharacter* Character) const
{
	if (!IsValid(Character))
	{
		return FVector(0.0f, 0.0f, EnemySpawnGroundZ);
	}

	const FVector PlayerLocation = Character->GetActorLocation();
	const FVector PlayerGroundLocation(PlayerLocation.X, PlayerLocation.Y, EnemySpawnGroundZ);
	const float ArenaHalfExtent = IsValid(Arena) ? Arena->GetArenaHalfExtent() - SpawnSettings.ArenaSpawnPadding : FMath::Max(
		SpawnSettings.SpawnRadius, DesiredDistance) + 500.0f; 

	for (int32 Attempt = 0; Attempt < 16; ++Attempt)
	{
		const FVector2D SpawnDirection = FVector2D(FMath::FRandRange(-1.0f, 1.0f), 
			FMath::FRandRange(-1.0f, 1.0f)).GetSafeNormal();
		
		const float DistanceScale = FMath::FRandRange(0.82f, 1.15f);
		
		const FVector CandidateLocation = PlayerGroundLocation + FVector(
			SpawnDirection.X * DesiredDistance * DistanceScale, SpawnDirection.Y * DesiredDistance * DistanceScale, 0.0f);
		
		if (FMath::Abs(CandidateLocation.X) <= ArenaHalfExtent && FMath::Abs(CandidateLocation.Y) <= ArenaHalfExtent
			&& IsSpawnLocationClearOfPlayers(CandidateLocation, MinimumDistance))
		{
			return CandidateLocation;
		}
	}

	// 兜底：**仍然围绕玩家取样**，只是把距离抖动放宽（0.7~1.6 倍）并把结果钳进竞技场。
	//
	// ⚠️ 这里原本是"在整个竞技场里均匀随机"——那是个真实的 bug，不是保守回退：
	//    · 玩家贴近边界时第一轮 16 次几乎必然失败（方向一多半指向场外），于是频繁落到这个循环；
	//    · 均匀随机会把怪撒到 ±ArenaHalfExtent 的任意角落（最远上万单位），
	//      表现是"怪刷在地图另一端、玩家永远遇不到"；
	//    · **Boss 也走这条路径**（SpawnBoss → SpawnConfiguredEnemy），所以 Boss 同样会刷丢。
	//   钳进竞技场 + 保留最小距离检查，既保证落点在可见范围内，也不会贴着玩家刷。
	for (int32 Attempt = 0; Attempt < 20; ++Attempt)
	{
		const FVector2D SpawnDirection = FVector2D(FMath::FRandRange(-1.0f, 1.0f),
			FMath::FRandRange(-1.0f, 1.0f)).GetSafeNormal();

		const float DistanceScale = FMath::FRandRange(0.7f, 1.6f);

		const FVector CandidateLocation = PlayerGroundLocation + FVector(
			SpawnDirection.X * DesiredDistance * DistanceScale, SpawnDirection.Y * DesiredDistance * DistanceScale, 0.0f);

		const FVector ClampedLocation(
			FMath::Clamp(CandidateLocation.X, -ArenaHalfExtent, ArenaHalfExtent),
			FMath::Clamp(CandidateLocation.Y, -ArenaHalfExtent, ArenaHalfExtent),
			EnemySpawnGroundZ);

		if (IsSpawnLocationClearOfPlayers(ClampedLocation, MinimumDistance))
		{
			return ClampedLocation;
		}
	}

	return FVector(
		FMath::Clamp(PlayerGroundLocation.X + DesiredDistance * 0.5f, -ArenaHalfExtent, ArenaHalfExtent),
		FMath::Clamp(PlayerGroundLocation.Y + DesiredDistance * 0.5f, -ArenaHalfExtent, ArenaHalfExtent),
		EnemySpawnGroundZ);
}

ERogueEnemyType URogueSpawnSubsystem::PickEnemyTypeForCurrentWave(int32 Wave) const
{
	return RogueGameModeRules::PickWeightedEnemyTypeForWave(Wave, BalanceAsset);
}

TSubclassOf<ARogueEnemy> URogueSpawnSubsystem::ResolveEnemyClass(ERogueEnemyType EnemyType) const
{
	// 优先查找类型映射表
	if (const TSubclassOf<ARogueEnemy>* FoundClass = EnemyClassMap.Find(EnemyType))
	{
		if (*FoundClass != nullptr)
		{
			return *FoundClass;
		}
	}

	// 回退到默认类
	return DefaultEnemyClass;
}
