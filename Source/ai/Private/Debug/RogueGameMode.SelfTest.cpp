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

	// 把属性集里的 13 个加成立刻推成角色快照（不然本帧日志看到的快照还是旧值，要等下一帧 Tick）
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
			Values.ScytheSize = Set->GetScytheSizeBonus();
	Values.ProjectileBurst = Set->GetProjectileBurstBonus();
	Values.RocketBlastRadius = Set->GetRocketBlastRadiusBonus();
	Values.HellTowerDamageRamp = Set->GetHellTowerDamageRampBonus();

			Player->ApplyWeaponBonusSnapshot(Values);
		}
	};

	const int32 PlayerCount = Players.Num();

	// 压测与阶段机**无关**：它只关心"场上敌人有多少"，不需要等客户端连上
	//（阶段 2 之后的所有步骤都要求 ≥2 玩家，那是给跨端数值对比用的）。
	// 放在这里才能用单进程跑压测 —— 少一个客户端也少一份噪声。
	if (!bSelfTestStressSpawned && Players.Num() > 0)
	{
		const int32 StressCount = RogueSelfTest::GetEnemyStressCount();
		if (StressCount > 0)
		{
			bSelfTestStressSpawned = true;
			ProbeEnemyStress(Players[0], StressCount);
		}
	}

	// ---- 自检要施加的升级内容 ----
	//
	// 每一项都走真实的 ApplyUpgrade → Infinite GE 叠层 → 属性集。
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
		{ ERogueUpgradeType::ProjectileCount,   1.0f, TEXT("御札数量+1") },
		{ ERogueUpgradeType::ScytheCount,       1.0f, TEXT("银刃数量+1") },
		{ ERogueUpgradeType::RocketCount,       1.0f, TEXT("八卦炉数量+1") },
		{ ERogueUpgradeType::LaserCount,        1.0f, TEXT("极限火花数量+1") },
		{ ERogueUpgradeType::HellTowerCount,    1.0f, TEXT("御柱数量+1") },
		{ ERogueUpgradeType::MortarCount,       1.0f, TEXT("阴阳玉数量+1") },
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
	// ---- 阶段 2/4：至少两名玩家就绪后修改属性，观察后续同步 ----
	//
	// 此阶段等待至少两名玩家就绪，再改变数值，以补充连接后的增量同步观察；
	// 第一阶段不保证所有客户端都已连接，只有对齐同玩家的日志才可讨论
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
			// 为什么非要有探针：接触/远程两条攻击路径的触发条件（贴脸 / 进射程）
			// 在无头测试里几乎撞不上 —— 武器会在敌人靠近前把它打死；震荡柱弹幕也需要单独验证。
			// 只统计次数的话，"没实现"和"没机会触发"看起来完全一样（实测两者都是 0）。
			TArray<ARogueCharacter*, TInlineAllocator<4>> ProbeTargets;
			CollectAlivePlayers(ProbeTargets);
			if (ProbeTargets.Num() > 0 && !bSelfTestProbesDone)
			{
				bSelfTestProbesDone = true;
				ProbeContactAttack(ProbeTargets[0]);
				ProbeShockPillarBarrage(ProbeTargets[0]);
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

/** 探针专用血量：高到足以活过"下一次决策 + 首次攻击冷却"。
 *  为什么必须调高：攻击路径的触发都需要敌人活到那个时候，而武器一轮齐射就能秒掉普通怪 ——
 *  这个坑实测踩过两次（接触/远程计数都是 0，看起来像"能力没实现"）。 */
static constexpr float ProbeEnemyHealth = 20000.0f;

void ARogueGameMode::ProbeDeathAndRevive(ARogueCharacter* Target)
{
	if (Target == nullptr || Target->IsDead())
	{
		UE_LOG(LogTemp, Log, TEXT("[SelfTest] 死亡/复活验证：目标不可用（已倒地或为空），跳过"));
		return;
	}

	// 主动施加 GAS 致命伤害，覆盖标签回调 → 下一 Tick 的 ProcessDeath → Die → 服务端定时器。
	// 有存活队友且本局继续时才会复活；若触发团灭则转为 Eliminated。
	// 此探针只触发并记录，不代替专门的生命周期自动化/双进程自检。
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

void ARogueGameMode::ProbeShockPillarBarrage(ARogueCharacter* Target)
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
	// 为什么不用常规刷怪流程：刷怪按波次/权重选类型，短时间内不一定会出震荡柱。
	const FVector SpawnLocation = Target->GetActorLocation() + FVector(600.0f, 0.0f, 0.0f);
	ARogueEnemy* Enemy = Pools->AcquireEnemy(ARogueEnemy::StaticClass(), nullptr, SpawnLocation, FRotator::ZeroRotator);
	if (Enemy == nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[SelfTest] 震荡柱弹幕验证：池里取不到敌人，跳过"));
		return;
	}

	FRogueEnemyProfile Profile;
	Profile.EnemyType = ERogueEnemyType::ShockPillar;
	// 使用探针专用高生命，增加敌人存活到首次弹幕的机会；
	// 实际存活时长仍取决于当前武器伤害，需检查后续攻击计数。
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

	UE_LOG(LogTemp, Log, TEXT("[SelfTest] 震荡柱弹幕验证：已在玩家 %s 附近生成一只震荡柱（%s），等它发射整圈弹幕"),
		Target->GetPlayerState() != nullptr ? *Target->GetPlayerState()->GetPlayerName() : *Target->GetName(),
		*Enemy->GetName());

	const FRogueEnemyRangedArchetype& Ranged = Enemy->GetRangedArchetype();
	UE_LOG(LogTemp, Log,
		TEXT("[SelfTest]   震荡柱诊断：弹幕开关=%d 攻击能力=%s 初始冷却=%.2f 距离=%.1f 每圈=%d 速度=%.1f"),
		Ranged.ShotPattern == ERogueEnemyShotPattern::RadialRing ? 1 : 0,
		Enemy->RangedAttackAbilityHandle.IsValid() ? TEXT("已授予") : TEXT("未授予"),
		Enemy->GetRangedAttackCooldownRemaining(),
		FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation()),
		Ranged.BulletRing.ProjectileCount, Ranged.BulletRing.ProjectileSpeed);
}

void ARogueGameMode::ProbeEnemyStress(ARogueCharacter* Target, int32 Count)
{
	if (Count <= 0 || Target == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	URogueCombatPoolSubsystem* Pools = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>();
	URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	if (Pools == nullptr || Tracker == nullptr)
	{
		return;
	}

	// 压测配置使用高生命、零接触伤害和约 300–814 的初始半径，减少初始负载波动。
	// 初始位置位于当前 950 距离降频阈值内；玩家移动、击杀和池获取失败仍会改变实际负载。
	// 不以生成请求数量代替活跃敌人数，也不把该场景视作所有配置下的最坏情况。
	FRogueEnemyProfile Profile;
	Profile.EnemyType = ERogueEnemyType::Rifleman;
	Profile.Health = 100000.0f;
	Profile.Speed = 120.0f;
	Profile.Damage = 0.0f;
	Profile.ExperienceReward = 0;
	Profile.bIsBoss = false;

	const FVector Center = Target->GetActorLocation();
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = (2.0f * PI * static_cast<float>(Index)) / static_cast<float>(Count);
		const float Radius = 300.0f + 600.0f * static_cast<float>(Index % 7) / 7.0f;
		const FVector SpawnLocation = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 60.0f);

		ARogueEnemy* Enemy = Pools->AcquireEnemy(ARogueEnemy::StaticClass(), nullptr, SpawnLocation, FRotator::ZeroRotator);
		if (Enemy == nullptr)
		{
			break;
		}

		Enemy->ActivatePooledEnemy(nullptr, SpawnLocation, FRotator::ZeroRotator);
		Enemy->InitializeEnemy(Profile);
		Tracker->RegisterEnemy(Enemy);
		++Spawned;
	}

	UE_LOG(LogTemp, Log, TEXT("[SelfTest] 压测：已刷 %d/%d 只敌人（攻速很硬、伤害 0），后续报告里的『敌人=N』与『平均帧时』就是 200+ 场景的数据"),
		Spawned, Count);
}
