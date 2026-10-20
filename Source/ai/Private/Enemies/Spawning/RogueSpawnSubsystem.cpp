// 敌人生成子系统 —— 实现
#include "Enemies/Spawning/RogueSpawnSubsystem.h"

#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "World/RogueEnemySpawnRegion.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameModeRules.h"
#include "Player/RoguePlayerQuery.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	FCollisionQueryParams MakeGroundQuery(UWorld* World, const ARogueEnemy* Enemy)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(RogueEnemySpawnGround), false, Enemy);
		// 角色胶囊、尸体和角色上的可查询网格均不能成为出生地面。
		for (TActorIterator<APawn> It(World); It; ++It) { Query.AddIgnoredActor(*It); }
		return Query;
	}

	/**
	 * "这一种敌人现在还能不能刷"的判据（供加权抽取做过滤）。
	 *
	 * 两道闸门，都配在 `DT_EnemyArchetypes` 里，**没填 / 填 0 = 不限制**：
	 *   ① `MaxAlive`        —— 同时存活上限：场上同时最多 N 只，**死掉就腾出名额**（动态）；
	 *   ② `MaxSpawnPerWave` —— 每波刷新上限：本波累计刷出 N 只就没了，**死了也不补**（静态预算）。
	 *   两个都填时谁先到谁生效。
	 *
	 * 为什么包成仿函数而不是就地写 lambda：
	 *  1) 加权抽取会对"每一行波次配置"各问一次，同一类型的上限/存活数会被反复查询，
	 *     所以在一次抽取内缓存结果；
	 *  2) 抽完之后调用方还要用同一套判据复核结果（所有类型都刷满时抽签会兜底成 Rifleman），
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
	BossSpawnRetryTimer = 0.0f;
}

void URogueSpawnSubsystem::TickSpawning(float DeltaSeconds, FRogueRunState& RunState, ARogueCharacter* Character, const ARogueEnemySpawnRegion* Region)
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

	BossSpawnRetryTimer = FMath::Max(0.0f, BossSpawnRetryTimer - DeltaSeconds);
	if (BossSettings.BossWaveInterval > 0 && RunState.CurrentWave > 0 && RunState.CurrentWave % BossSettings.BossWaveInterval == 0 &&
		BossSpawnRetryTimer <= 0.0f && !RunState.bBossAlive && RunState.LastBossWaveSpawned != RunState.CurrentWave)
	{
		SpawnBoss(RunState, Character, Region);
		// 无有效地面时避免每帧重试整套射线；不消耗本波 Boss 名额。
		if (!RunState.bBossAlive) { BossSpawnRetryTimer = FMath::Max(0.1f, SpawnSettings.SpawnInterval); }
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
			if (SpawnEnemy(RunState, Character, Region))
			{
				++AliveEnemies;
			}
		}

		RunState.SpawnTimer = SpawnCadence.NextSpawnDelay;
	}
}

bool URogueSpawnSubsystem::SpawnEnemy(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueEnemySpawnRegion* Region)
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

	// 复核：所有类型都被拦下时，抽签函数只能兜底返回 Rifleman，这里必须把它拦下来，
	// 否则两道限制都会被兜底路径绕过。
	if (!SpawnGate(EnemyType))
	{
		return false;
	}

	ARogueEnemy* SpawnedEnemy = SpawnConfiguredEnemy(RogueGameModeRules::BuildEnemyProfile(RunState.CurrentWave, EnemyType,
		BalanceAsset), SpawnSettings.SpawnRadius, SpawnSettings.EnemySpawnMinimumDistance, Region, Character);
	if (SpawnedEnemy == nullptr)
	{
		return false;   // 池子拿不到/生成失败 → 不占名额
	}

	// 只有真正刷出来了才记账：失败还计数会让"名额"被空气吃掉。
	++SpawnedCountThisWave.FindOrAdd(EnemyType);
	return true;
}

void URogueSpawnSubsystem::SpawnBoss(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueEnemySpawnRegion* Region)
{
	if (!IsValid(Character) || Character->IsDead())
	{
		return;
	}

	if (SpawnConfiguredEnemy(RogueGameModeRules::BuildBossProfile(RunState.CurrentWave, PickEnemyTypeForCurrentWave(RunState.CurrentWave), 
		BalanceAsset), BossSettings.BossSpawnDistance, BossSettings.BossSpawnMinimumDistance, Region, Character) != nullptr)
	{
		RunState.bBossAlive = true;
		RunState.LastBossWaveSpawned = RunState.CurrentWave;
	}
}

ARogueEnemy* URogueSpawnSubsystem::SpawnConfiguredEnemy(const FRogueEnemyProfile& EnemyProfile, float DesiredDistance, float MinimumDistance,
	const ARogueEnemySpawnRegion* Region, ARogueCharacter* Character)
{
	URogueCombatPoolSubsystem* Pools = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>();
	URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	if (Pools == nullptr || Tracker == nullptr)
	{
		return nullptr;
	}

	if (!IsValid(Character) || !IsValid(Region)) { return nullptr; }
	TSubclassOf<ARogueEnemy> ResolvedClass = ResolveEnemyClass(EnemyProfile.EnemyType);
	UClass* EnemyToSpawn = ResolvedClass ? ResolvedClass.Get() : ARogueEnemy::StaticClass();
	// Acquire 只取用/定位，尚未激活；此时可读取蓝图构造完成后的真实胶囊尺寸。
	ARogueEnemy* Enemy = Pools->AcquireEnemy(EnemyToSpawn, nullptr, Character->GetActorLocation(), FRotator::ZeroRotator);
	if (Enemy == nullptr) { return nullptr; }
	FVector SpawnLocation;
	if (!FindSpawnLocation(DesiredDistance, MinimumDistance, Region, Character, Enemy, SpawnLocation))
	{
		return nullptr; // 保持回池状态，不登记、不占用本波刷新名额。
	}
	const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
	const float InitialHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float InitialRadius = Capsule->GetScaledCapsuleRadius();
	Enemy->ActivatePooledEnemy(nullptr, SpawnLocation, FRotator::ZeroRotator);
	Enemy->InitializeEnemy(EnemyProfile);
	// 蓝图激活/初始化事件若调整了胶囊大小，再以本次尺寸落地一次。
	if (!FMath::IsNearlyEqual(InitialHalfHeight, Capsule->GetScaledCapsuleHalfHeight())
		|| !FMath::IsNearlyEqual(InitialRadius, Capsule->GetScaledCapsuleRadius()))
	{
		FVector Adjusted;
		const FVector Candidate(SpawnLocation.X, SpawnLocation.Y, Character->GetActorLocation().Z);
		if (!TryGroundSpawnLocation(Candidate, Enemy, MakeGroundQuery(GetWorld(), Enemy), Region, Adjusted))
		{
			Enemy->DeactivateToPool();
			return nullptr;
		}
		Enemy->SetActorLocation(Adjusted, false, nullptr, ETeleportType::TeleportPhysics);
	}

	Tracker->RegisterEnemy(Enemy);
	return Enemy;
}

bool URogueSpawnSubsystem::IsSpawnLocationClearOfPlayers(const FVector& CandidateLocation, float MinimumDistance) const
{
	// 死掉的玩家不占出生点：把他算进去会让"没人的那半边场地"也刷不出怪。
	// （口径统一在 RoguePlayerQuery，见那里的说明。）
	return !RoguePlayerQuery::IsAnyAlivePlayerWithin(GetWorld(), CandidateLocation, MinimumDistance);
}

bool URogueSpawnSubsystem::TryGroundSpawnLocation(const FVector& Candidate, const ARogueEnemy* Enemy,
	const FCollisionQueryParams& GroundQuery, const ARogueEnemySpawnRegion* Region, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = IsValid(Enemy) ? Enemy->GetCapsuleComponent() : nullptr;
	const UCharacterMovementComponent* Movement = IsValid(Enemy) ? Enemy->GetCharacterMovement() : nullptr;
	if (World == nullptr || Capsule == nullptr || Movement == nullptr) { return false; }

	FHitResult Hit;
    FVector Start, End;
    if (!IsValid(Region) || !Region->ClipGroundTrace(Candidate, SpawnSettings.GroundTraceHeight, SpawnSettings.GroundTraceDepth, Start, End)) { return false; }
	if (!World->LineTraceSingleByChannel(Hit, Start, End, SpawnSettings.GroundTraceChannel, GroundQuery)
		|| Hit.bStartPenetrating || !Region->IsGroundHeightAllowed(Hit.ImpactPoint) || !Movement->IsWalkable(Hit)) { return false; }

	const UPrimitiveComponent* Ground = Hit.GetComponent();
	if (Ground == nullptr || Ground->GetCollisionResponseToChannel(Capsule->GetCollisionObjectType()) != ECR_Block
		|| Capsule->GetCollisionResponseToChannel(Ground->GetCollisionObjectType()) != ECR_Block) { return false; }

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	if (!FMath::IsFinite(HalfHeight) || !FMath::IsFinite(Radius) || Radius <= 0.0f || HalfHeight < Radius) { return false; }
	// 胶囊下端是球面；斜坡额外抬高 Radius * (1 / Normal.Z - 1)，避免侧面初始穿入坡面。
	const float SlopeLift = Radius * (1.0f / FMath::Max(Hit.ImpactNormal.Z, KINDA_SMALL_NUMBER) - 1.0f);
	const FVector Location(Candidate.X, Candidate.Y,
		Hit.ImpactPoint.Z + HalfHeight + SlopeLift + FMath::Max(0.0f, SpawnSettings.GroundClearance));
	if (!Region->ContainsCapsule(Location, Radius, HalfHeight)) { return false; }
	const FCollisionQueryParams ClearanceQuery(SCENE_QUERY_STAT(RogueEnemySpawnClearance), false, Enemy);
	if (World->OverlapBlockingTestByChannel(Location, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Radius, HalfHeight), ClearanceQuery,
		FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()))) { return false; }
	OutLocation = Location;
	return true;
}

bool URogueSpawnSubsystem::FindSpawnLocation(float DesiredDistance, float MinimumDistance,
    const ARogueEnemySpawnRegion* Region, ARogueCharacter* Character, const ARogueEnemy* Enemy, FVector& OutLocation) const
{
    if (!IsValid(Character) || !IsValid(Enemy) || !IsValid(Region)) { return false; }
    const FVector PlayerLocation = Character->GetActorLocation();
    const FCollisionQueryParams GroundQuery = MakeGroundQuery(GetWorld(), Enemy);
    auto TryCandidate = [&](const FVector& Candidate)
    {
        return IsSpawnLocationClearOfPlayers(Candidate, MinimumDistance)
            && TryGroundSpawnLocation(Candidate, Enemy, GroundQuery, Region, OutLocation)
            && IsSpawnLocationClearOfPlayers(OutLocation, MinimumDistance);
    };
    // 优先保持玩家周围的刷怪节奏，所有候选点必须落在关卡框内。
    for (int32 Attempt = 0; Attempt < 16; ++Attempt)
    {
        const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
        const float Distance = DesiredDistance * FMath::FRandRange(0.82f, 1.15f);
        if (TryCandidate(PlayerLocation + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f))) { return true; }
    }
    // 玩家在框外或框较小时，在整个框内重新选点，不回退到世界原点的隐式范围。
    for (int32 Attempt = 0; Attempt < 32; ++Attempt)
    {
        FVector Candidate = Region->GetRandomCandidate();
        Candidate.Z = PlayerLocation.Z;
        if (TryCandidate(Candidate)) { return true; }
    }
    return false;
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
