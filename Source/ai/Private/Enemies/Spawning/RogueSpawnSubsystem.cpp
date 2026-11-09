// 敌人生成子系统 —— 实现
#include "Enemies/Spawning/RogueSpawnSubsystem.h"

#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "World/RogueEnemySpawnRegion.h"
#include "World/RogueRangedSpawnPoint.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameModeRules.h"
#include "Player/RoguePlayerQuery.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	bool IsSpawnLocationClearOfEnemies(UWorld* World, const FVector& Location, const ARogueEnemy* Enemy)
	{
		const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
		constexpr float ExtraSpacing = 28.f;
		TArray<FOverlapResult> Overlaps;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(RogueEnemySpawnSpacing), false, Enemy);
		World->OverlapMultiByObjectType(Overlaps, Location, FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() + ExtraSpacing,
				Capsule->GetScaledCapsuleHalfHeight() + ExtraSpacing), Query);
		for (const FOverlapResult& Result : Overlaps)
		{
			const ARogueEnemy* Other = Cast<ARogueEnemy>(Result.GetActor());
			if (IsValid(Other) && !Other->IsDead() && !Other->IsAvailableInPool() &&
				Result.GetComponent() == Other->GetCapsuleComponent())
				return false;
		}
		return true;
	}

	FCollisionQueryParams MakeGroundQuery(UWorld* World, const ARogueEnemy* Enemy)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(RogueEnemySpawnGround), false, Enemy);
		// 角色胶囊、尸体和角色上的可查询网格均不能成为出生地面。
		for (TActorIterator<APawn> It(World); It; ++It) { Query.AddIgnoredActor(*It); }
		return Query;
	}

	bool HasContinuousSpawnFloor(const UWorld& World, const FHitResult& Ground, const UCapsuleComponent& Capsule,
		const UCharacterMovementComponent& Movement, ECollisionChannel TraceChannel, const FCollisionQueryParams& Query)
	{
		const FVector Normal = Ground.ImpactNormal;
		const double Tolerance = FMath::Clamp(static_cast<double>(Movement.MaxStepHeight), 20.0, 45.0);
		const auto HasSupport = [&](const FVector& Offset)
		{
			// 按中心坡面推算相邻支撑的高度，保留正常斜坡，拒绝石顶和地面之间的断层。
			const FVector Sample = Ground.ImpactPoint + Offset -
				FVector(0, 0, FVector::DotProduct(Normal, Offset) / Normal.Z);
			const FVector Vertical(0, 0, Tolerance);
			FHitResult Support;
			if (!World.LineTraceSingleByChannel(Support, Sample + Vertical, Sample - Vertical, TraceChannel, Query) ||
				Support.bStartPenetrating || !Movement.IsWalkable(Support))
			{
				return false;
			}
			const UPrimitiveComponent* Surface = Support.GetComponent();
			return Surface && Surface->GetCollisionResponseToChannel(Capsule.GetCollisionObjectType()) == ECR_Block &&
				Capsule.GetCollisionResponseToChannel(Surface->GetCollisionObjectType()) == ECR_Block;
		};

		const double Footprint = Capsule.GetScaledCapsuleRadius() + 12.0;
		for (const FVector& Axis : {FVector::ForwardVector, FVector::RightVector})
		{
			if (!HasSupport(Axis * Footprint) || !HasSupport(-Axis * Footprint)) { return false; }
		}

		// 需要一条可离开出生点的通道。允许窄桥与斜向通道，孤立岩顶即使中心可站也会被排除。
		const double LaneProbeDistance = FMath::Max(280.0, Footprint * 3.0);
		const FVector Axes[] = {FVector::ForwardVector, FVector::RightVector,
			FVector(1, 1, 0).GetSafeNormal2D(), FVector(1, -1, 0).GetSafeNormal2D()};
		for (const FVector& Axis : Axes)
		{
			if (HasSupport(Axis * LaneProbeDistance) && HasSupport(-Axis * LaneProbeDistance)) { return true; }
		}
		return false;
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
    float CountScale(const UWorld* World)
    { const auto* Mode=World?World->GetAuthGameMode<ARogueGameMode>():nullptr;return Mode?Mode->GetEnemyCountMultiplier():1.f; }
    float RateScale(const UWorld* World)
    { const auto* Mode=World?World->GetAuthGameMode<ARogueGameMode>():nullptr;return Mode?Mode->GetEnemySpawnRateMultiplier():1.f; }
	struct FEnemySpawnGate
	{
		const UDataTable* ArchetypeTable = nullptr;
        float CountMultiplier=1.f;
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
				[this](ERogueEnemyType Type) { const int32 Base=RogueEnemyArchetypes::GetMaxAlive(ArchetypeTable, Type); return int32(FMath::Min(double(MAX_int32),FMath::RoundToDouble(double(Base)*CountMultiplier))); });

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
				[this](ERogueEnemyType Type) { const int32 Base=RogueEnemyArchetypes::GetMaxSpawnPerWave(ArchetypeTable, Type); return int32(FMath::Min(double(MAX_int32),FMath::RoundToDouble(double(Base)*CountMultiplier))); });

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
    WaveEnemyBudget = RogueGameModeRules::BuildWaveEnemyCount(Wave, BalanceAsset, CountScale(GetWorld()));
    TotalSpawnedThisWave = 0;
    SpawnedMeleeThisWave = 0;
    NightMeleeTypes.Reset();
    const auto* Mode=GetWorld()->GetAuthGameMode<ARogueGameMode>();
    bNightComposition=Mode&&Mode->IsNightBattleStage();
    const auto& Stage=BalanceAsset?BalanceAsset->BattleStage:GetDefault<URogueGameBalanceAsset>()->BattleStage;
    const float Percent=FMath::IsFinite(Stage.NightMeleePercent)?FMath::Clamp(Stage.NightMeleePercent,0.f,100.f):70.f;
    WaveMeleeTarget=bNightComposition?int32(FMath::RoundToDouble(double(WaveEnemyBudget)*double(Percent)/100.)):0;
    auto Scale=[](float V){return FMath::IsFinite(V)?FMath::Clamp(V,.1f,10.f):2.f;};
    NightMeleeSpeedScale=Scale(Stage.NightMeleeMoveSpeedMultiplier);
    NightMeleeDamageScale=Scale(Stage.NightMeleeDamageMultiplier);
    if(bNightComposition&&EnemyArchetypeTable)
    {
        for(FName Name:EnemyArchetypeTable->GetRowNames())
        {
            const auto* Row=EnemyArchetypeTable->FindRow<FRogueEnemyArchetypeRow>(Name,TEXT("Night melee classification"));
            if(Row&&Row->bContactOnlyNormal&&!RogueEnemyArchetypes::IsHealerType(Row->EnemyType)&&!RogueEnemyArchetypes::IsFlyingType(Row->EnemyType))
                NightMeleeTypes.Add(Row->EnemyType);
        }
    }
    if(bNightComposition)
        UE_LOG(LogTemp,Log,TEXT("[RogueSpawn] Night wave=%d total=%d meleeTarget=%d percent=%.1f speed=%.2f damage=%.2f"),Wave,WaveEnemyBudget,WaveMeleeTarget,Percent,NightMeleeSpeedScale,NightMeleeDamageScale);
	BossSpawnRetryTimer = 0.0f;
}

int32 URogueSpawnSubsystem::ResolveNightMeleeTarget(int32 Wave) const
{
    if(!bNightComposition)return 0;
    FEnemySpawnGate Quota;
    Quota.ArchetypeTable=EnemyArchetypeTable;Quota.CountMultiplier=CountScale(GetWorld());
    Quota.SpawnedCountThisWave=&SpawnedCountThisWave;
    // Only permanent per-wave exhaustion permits redistribution. An alive cap is
    // temporary: wait for its slot instead of replacing the melee share with shooters.
    const bool HasMelee=RogueGameModeRules::HasEligibleEnemyTypeForWave(Wave,BalanceAsset,[&](ERogueEnemyType T){return IsNightMeleeType(T)&&Quota(T);});
    const bool HasOther=RogueGameModeRules::HasEligibleEnemyTypeForWave(Wave,BalanceAsset,[&](ERogueEnemyType T){return !IsNightMeleeType(T)&&Quota(T);});
    if(!HasMelee)return SpawnedMeleeThisWave;
    if(!HasOther)return WaveEnemyBudget-(TotalSpawnedThisWave-SpawnedMeleeThisWave);
    return FMath::Clamp(WaveMeleeTarget,SpawnedMeleeThisWave,WaveEnemyBudget-(TotalSpawnedThisWave-SpawnedMeleeThisWave));
}

bool URogueSpawnSubsystem::HasRemainingNormalWaveBudget(int32 Wave) const
{
    if (TotalSpawnedThisWave >= WaveEnemyBudget) return false;
    FEnemySpawnGate QuotaGate;
    QuotaGate.ArchetypeTable = EnemyArchetypeTable;
    QuotaGate.CountMultiplier=CountScale(GetWorld());
    QuotaGate.SpawnedCountThisWave = &SpawnedCountThisWave;
    // 不传 Tracker：同时存活名额会随击杀释放，不能误认为本波累计名额耗尽。
    const int32 MeleeTarget=ResolveNightMeleeTarget(Wave);
    return RogueGameModeRules::HasEligibleEnemyTypeForWave(Wave, BalanceAsset, [&](ERogueEnemyType Type)
    {
        if(bNightComposition)
        {
            const bool NeedsBucket=IsNightMeleeType(Type)?SpawnedMeleeThisWave<MeleeTarget
                :TotalSpawnedThisWave-SpawnedMeleeThisWave<WaveEnemyBudget-MeleeTarget;
            if(!NeedsBucket)return false;
        }
        return QuotaGate(Type);
    });
}

namespace
{
bool IsBossEncounterWave(int32 Wave,int32 Interval)
{
    return Wave==ARogueGameMode::VictoryWave || (Interval>0 && Wave>0 && Wave%Interval==0);
}
}

bool URogueSpawnSubsystem::HasFinishedWaveSpawning(const FRogueRunState& RunState) const
{
    if (CountedWave != RunState.CurrentWave || WaveEnemyBudget <= 0) return false;
    const bool bBossWave = IsBossEncounterWave(RunState.CurrentWave,BossSettings.BossWaveInterval);
    if (bBossWave && RunState.LastBossWaveSpawned != RunState.CurrentWave) return false;
    return !HasRemainingNormalWaveBudget(RunState.CurrentWave);
}

bool URogueSpawnSubsystem::IsWaveComplete(const FRogueRunState& RunState) const
{
    const URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
    return HasFinishedWaveSpawning(RunState) && !RunState.bBossAlive
        && Tracker && Tracker->GetActiveEnemyCount() == 0;
}

void URogueSpawnSubsystem::TickSpawning(float DeltaSeconds, FRogueRunState& RunState, ARogueCharacter* Character, const ARogueEnemySpawnRegion* Region)
{
    if (bSpawningSuspended || RunState.bGameOver) { return; }
	RunState.SpawnTimer -= DeltaSeconds;

	// 波次一变就把"本波已刷数量"清零。放在这里而不是 SpawnEnemy 里，
	// 是为了让"跨波"这件事只有一个入口，避免每个生成点都要判一次。
	if (CountedWave != RunState.CurrentWave)
	{
		ResetWaveSpawnCounts(RunState.CurrentWave);
	}

	BossSpawnRetryTimer = FMath::Max(0.0f, BossSpawnRetryTimer - DeltaSeconds);
	if (IsBossEncounterWave(RunState.CurrentWave,BossSettings.BossWaveInterval) &&
		BossSpawnRetryTimer <= 0.0f && !RunState.bBossAlive && RunState.LastBossWaveSpawned != RunState.CurrentWave)
	{
		SpawnBoss(RunState, Character, Region);
		// 无有效地面时避免每帧重试整套射线；不消耗本波 Boss 名额。
		if (!RunState.bBossAlive) { BossSpawnRetryTimer = FMath::Max(0.1f, SpawnSettings.SpawnInterval); }
	}

    // 达到本波总量后不再补刷；昼夜均由 GameMode 等待清场后切波。Boss 失败重试仍在上方调度。
    if (!HasRemainingNormalWaveBudget(RunState.CurrentWave)) return;

	const FRogueSpawnCadence SpawnCadence = RogueGameModeRules::BuildSpawnCadence(
        RunState.CurrentWave, SpawnSettings.SpawnInterval, BalanceAsset, RateScale(GetWorld()));
	int32 Batches=0;
    while (RunState.SpawnTimer <= 0.0f && HasRemainingNormalWaveBudget(RunState.CurrentWave) && Batches++<4)
	{
        // 不以全场存活数量阻止生成；SpawnEnemy 仍检查累计预算与每类怪的限制。
        const int32 Before=TotalSpawnedThisWave;
        const int32 Attempts = FMath::Min(SpawnCadence.SpawnBatchSize, WaveEnemyBudget - TotalSpawnedThisWave);
		for (int32 SpawnIndex = 0; SpawnIndex < Attempts; ++SpawnIndex)
		{
            SpawnEnemy(RunState, Character, Region);
		}
		RunState.SpawnTimer += SpawnCadence.NextSpawnDelay;
        if (TotalSpawnedThisWave==Before) { RunState.SpawnTimer=SpawnCadence.NextSpawnDelay;break; }
	}
}

bool URogueSpawnSubsystem::SpawnEnemy(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueEnemySpawnRegion* Region)
{
    if (bSpawningSuspended || RunState.bGameOver) { return false; }
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

    if (!HasRemainingNormalWaveBudget(RunState.CurrentWave)) return false;

	FEnemySpawnGate SpawnGate;
	SpawnGate.ArchetypeTable = EnemyArchetypeTable;
    SpawnGate.CountMultiplier=CountScale(GetWorld());
	SpawnGate.Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	SpawnGate.SpawnedCountThisWave = &SpawnedCountThisWave;
	SpawnGate.AliveCapLoggedTypes = &AliveCapLoggedTypes;
	SpawnGate.WaveBudgetLoggedTypes = &WaveBudgetLoggedTypes;

    bool bSpawnMelee=false;
    if(bNightComposition)
    {
        const int32 Target=ResolveNightMeleeTarget(RunState.CurrentWave);
        const int32 MeleeRemaining=Target-SpawnedMeleeThisWave;
        const int32 OtherRemaining=WaveEnemyBudget-Target-(TotalSpawnedThisWave-SpawnedMeleeThisWave);
        const bool MeleeReady=MeleeRemaining>0&&RogueGameModeRules::HasEligibleEnemyTypeForWave(RunState.CurrentWave,BalanceAsset,[&](ERogueEnemyType T){return IsNightMeleeType(T)&&SpawnGate(T);});
        const bool OtherReady=OtherRemaining>0&&RogueGameModeRules::HasEligibleEnemyTypeForWave(RunState.CurrentWave,BalanceAsset,[&](ERogueEnemyType T){return !IsNightMeleeType(T)&&SpawnGate(T);});
        if(!MeleeReady&&!OtherReady)return false;
        bSpawnMelee=MeleeReady&&(!OtherReady||FMath::FRand()<double(MeleeRemaining)/double(MeleeRemaining+OtherRemaining));
    }
    auto Eligible=[&](ERogueEnemyType Type){return (!bNightComposition||IsNightMeleeType(Type)==bSpawnMelee)&&SpawnGate(Type);};
    if(!RogueGameModeRules::HasEligibleEnemyTypeForWave(RunState.CurrentWave,BalanceAsset,Eligible))return false;
    ERogueEnemyType EnemyType=RogueGameModeRules::PickWeightedEnemyTypeForWave(RunState.CurrentWave,BalanceAsset,Eligible);
    if (BalanceAsset)
    {
        for (const auto& Entry:BalanceAsset->EnemyWaveEntries)
        {
            if (Entry.bGuaranteeIntroduction && Entry.Weight>0 && Entry.MinWave==RunState.CurrentWave
                && SpawnedCountThisWave.FindRef(Entry.EnemyType)==0 && Eligible(Entry.EnemyType))
            { EnemyType=Entry.EnemyType; break; }
        }
    }

	// 复核：所有类型都被拦下时，抽签函数只能兜底返回 Rifleman，这里必须把它拦下来，
	// 否则两道限制都会被兜底路径绕过。
	if (!Eligible(EnemyType))
	{
		return false;
	}

    FRogueEnemyProfile Profile=RogueGameModeRules::BuildEnemyProfile(RunState.CurrentWave,EnemyType,BalanceAsset);
    if(bNightComposition&&bSpawnMelee)
    {
        Profile.Speed*=NightMeleeSpeedScale;
        Profile.Damage*=NightMeleeDamageScale;
    }
    ARogueEnemy* SpawnedEnemy=SpawnConfiguredEnemy(Profile,SpawnSettings.SpawnRadius,SpawnSettings.EnemySpawnMinimumDistance,Region,Character);
	if (SpawnedEnemy == nullptr)
	{
		return false;   // 池子拿不到/生成失败 → 不占名额
	}

	// 只有真正刷出来了才记账：失败还计数会让"名额"被空气吃掉。
	++SpawnedCountThisWave.FindOrAdd(EnemyType);
    ++TotalSpawnedThisWave;
    if(bNightComposition&&bSpawnMelee)++SpawnedMeleeThisWave;
	return true;
}

void URogueSpawnSubsystem::SpawnBoss(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueEnemySpawnRegion* Region)
{
    if (bSpawningSuspended || RunState.bGameOver || RunState.bBossAlive || RunState.LastBossWaveSpawned == RunState.CurrentWave) { return; }
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
    // 与 InitializeEnemy 使用同一份原型表；普通接触怪不会因历史类型名称而误走远程点。
    const auto Archetype = RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(
        Enemy->GetEnemyArchetypeTable(), EnemyProfile.EnemyType, EnemyProfile.bIsBoss);
    // 不动的炮台优先在正常安全距离外的近圈出现，避免留在地图远端等待玩家收尾。
    if (Archetype.Movement.Model==ERogueEnemyMovementModel::Stationary)
        DesiredDistance=FMath::Max(MinimumDistance+100.f,FMath::Min(DesiredDistance,MinimumDistance*1.35f));
	FVector SpawnLocation;
	if (!FindSpawnLocation(DesiredDistance, MinimumDistance, Region, Character, Enemy,
		RogueEnemyArchetypes::IsFlyingType(EnemyProfile.EnemyType), Archetype.Ranged.bUsesRangedAttack, SpawnLocation))
	{
		return nullptr; // 保持回池状态，不登记、不占用本波刷新名额。
	}
	const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
	const float InitialHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float InitialRadius = Capsule->GetScaledCapsuleRadius();
	Enemy->ActivatePooledEnemy(nullptr, SpawnLocation, FRotator::ZeroRotator);
	Enemy->InitializeEnemy(EnemyProfile);
	// 飞行怪只抬高 Z。蓝图如果改了胶囊或水平位置，用最终数据重新验证落点。
	const FVector InitializedAt = Enemy->GetActorLocation();
	if (FVector::DistSquared2D(InitializedAt, SpawnLocation) > 1.0
		|| !FMath::IsNearlyEqual(InitialHalfHeight, Capsule->GetScaledCapsuleHalfHeight())
		|| !FMath::IsNearlyEqual(InitialRadius, Capsule->GetScaledCapsuleRadius()))
	{
		FVector Adjusted;
		const FVector Candidate(InitializedAt.X, InitializedAt.Y, Character->GetActorLocation().Z);
		if (!TryGroundSpawnLocation(Candidate, Enemy, MakeGroundQuery(GetWorld(), Enemy), Region, Adjusted)
			|| !IsSpawnLocationClearOfPlayers(Adjusted, MinimumDistance))
		{
			Enemy->DeactivateToPool();
			return nullptr;
		}
		Enemy->SetSpawnGroundReference(Adjusted);
	}

	// 原型初始化可能改胶囊或飞行高度，用真实位置再确认一次。
	if (!IsSpawnLocationClearOfEnemies(GetWorld(), Enemy->GetActorLocation(), Enemy))
	{
		Enemy->DeactivateToPool();
		return nullptr;
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
	if (!HasContinuousSpawnFloor(*World, Hit, *Capsule, *Movement, SpawnSettings.GroundTraceChannel, GroundQuery)) { return false; }
	OutLocation = Location;
	return true;
}

bool URogueSpawnSubsystem::FindSpawnLocation(float DesiredDistance, float MinimumDistance,
    const ARogueEnemySpawnRegion* Region, ARogueCharacter* Character, const ARogueEnemy* Enemy,
    bool bFlying, bool bFixedRanged, FVector& OutLocation)
{
    if (!IsValid(Character) || !IsValid(Enemy) || !IsValid(Region)) { return false; }
    const FVector PlayerLocation = Character->GetActorLocation();
    const FCollisionQueryParams GroundQuery = MakeGroundQuery(GetWorld(), Enemy);
    auto TryCandidate = [&](const FVector& Candidate)
    {
        return IsSpawnLocationClearOfPlayers(Candidate, MinimumDistance)
            && TryGroundSpawnLocation(Candidate, Enemy, GroundQuery, Region, OutLocation)
            && IsSpawnLocationClearOfPlayers(OutLocation, MinimumDistance)
            && (bFlying || IsSpawnLocationClearOfEnemies(GetWorld(), OutLocation, Enemy));
    };
    bool bNearbyFallback=false;
    const float ActiveDistance=FMath::Max(DesiredDistance*1.25f,MinimumDistance+300.f);
    if (bFixedRanged)
    {
        struct FNearbyPoint {int32 Index;double DistanceSquared;};
        TArray<FNearbyPoint,TInlineAllocator<16>> Nearby;
        bool bHasAssignedPoints=false,bHasEnabledPoints=false;
        for(int32 Index=0;Index<RangedSpawnPoints.Num();++Index)
        {
            const auto* Point=RangedSpawnPoints[Index].Get();
            if(!IsValid(Point)||Point->SpawnRegion!=Region)continue;
            bHasAssignedPoints=true;
            if(!Point->bEnabled)continue;
            bHasEnabledPoints=true;
            const double DistanceSquared=FVector::DistSquared2D(Point->GetActorLocation(),PlayerLocation);
            if(DistanceSquared<=FMath::Square(ActiveDistance))Nearby.Add({Index,DistanceSquared});
        }
        // 只轮换距离玩家最近的三个点；不再轮流使用地图另一端的入口。
        Nearby.Sort([](const FNearbyPoint& A,const FNearbyPoint& B)
        {return A.DistanceSquared==B.DistanceSquared?A.Index<B.Index:A.DistanceSquared<B.DistanceSquared;});
        const int32 ActiveCount=FMath::Min(3,Nearby.Num());
        for(int32 Offset=0;Offset<ActiveCount;++Offset)
        {
            const int32 Index=(NextRangedSpawnPoint+Offset)%ActiveCount;
            if(TryCandidate(RangedSpawnPoints[Nearby[Index].Index]->GetActorLocation()))
            {NextRangedSpawnPoint=(Index+1)%ActiveCount;return true;}
        }
        // 全部手动禁用仍尊重关卡设置；远处或暂时堵塞的点则改用玩家身边的合法地面。
        if(bHasAssignedPoints&&!bHasEnabledPoints)return false;
        bNearbyFallback=bHasAssignedPoints;
    }
    // 未摆固定点的自定义关卡仍沿用区域随机生成；现有战斗地图全部配置固定点。
    // 优先保持玩家周围的刷怪节奏，所有候选点必须落在关卡框内。
    for (int32 Attempt = 0; Attempt < 16; ++Attempt)
    {
        const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
        const float Distance = DesiredDistance * FMath::FRandRange(0.82f, 1.15f);
        if (TryCandidate(PlayerLocation + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f))) { return true; }
    }
    // 固定点回退继续限制在玩家周边；未摆点的自定义关卡仍可在整个区域内兜底。
    for (int32 Attempt = 0; Attempt < 32; ++Attempt)
    {
        FVector Candidate;
        if(bNearbyFallback)
        {
            const float Angle=FMath::FRandRange(0.f,2.f*PI);
            const float Distance=FMath::FRandRange(MinimumDistance,ActiveDistance);
            Candidate=PlayerLocation+FVector(FMath::Cos(Angle)*Distance,FMath::Sin(Angle)*Distance,0);
        }
        else {Candidate=Region->GetRandomCandidate();Candidate.Z=PlayerLocation.Z;}
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
