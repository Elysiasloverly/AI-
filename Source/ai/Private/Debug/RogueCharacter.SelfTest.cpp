#include "Player/RogueCharacter.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Debug/RogueSelfTest.h"

#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Combat/Weapons/RogueWeapon_Scythe.h"
#include "Combat/Weapons/RogueWeapon_Rocket.h"
#include "Combat/Weapons/RogueWeapon_Laser.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Combat/Weapons/RogueWeapon_Mortar.h"
#include "Combat/Projectiles/RogueMortarProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "Player/RoguePlayerBalanceAsset.h"
#include "Player/RoguePlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/Abilities/RogueWeaponAbility.h"
#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"
#include "AbilitySystem/Abilities/RogueEnemyAttackAbility.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "AbilitySystem/GameplayEffects/RogueAttributeUpgradeEffect.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "World/RogueShopTerminal.h"
#include "UI/RogueHUD.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
// Enhanced Input：绑定与必需资产校验见 RogueCharacter.Input.cpp；缺失时不绑定本角色动作。
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/SoftObjectPath.h"


void ARogueCharacter::TickWeaponFireReport(float DeltaSeconds)
{
	if (!RogueSelfTest::IsWeaponUpgradeChainEnabled() || WeaponFireReportCount >= 6)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float NowSeconds = World->GetTimeSeconds();

	// 帧时采样：每帧累加，报告时取平均（报告本身每 10 秒一次）。
	// 放在"还没到报告时间"的判断**之前**，否则采样窗口会不完整。
	if (WeaponFireReportNextTime > 0.0f)
	{
		FireReportAccumulatedTime += DeltaSeconds;
		++FireReportFrameCount;
	}

	if (WeaponFireReportNextTime <= 0.0f)
	{
		// 第一次调用只建立时间基准（这时候武器与技能通常都还没就绪）
		WeaponFireReportNextTime = NowSeconds + 10.0f;
		return;
	}
	if (NowSeconds < WeaponFireReportNextTime)
	{
		return;
	}

	WeaponFireReportNextTime = NowSeconds + 10.0f;
	++WeaponFireReportCount;

	URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	// 本局战况：击杀数只在服务端增长（伤害在服务端结算），所以客户端读到的这份
	// 恰好能当"复制是否也正常"的旁证。
	int32 Kills = -1;
	int32 Wave = -1;
	if (const AGameStateBase* WorldGameState = World->GetGameState())
	{
		if (const ARogueGameState* RogueGS = Cast<ARogueGameState>(WorldGameState))
		{
			Kills = RogueGS->GetEnemiesDefeated();
			Wave = RogueGS->GetCurrentWave();
		}
	}

	const APlayerState* OwningPlayerState = GetPlayerState();

	// 场上敌人数量 + 最近 10 秒的平均帧时：
	//   · 敌人数量是"这次采样可不可比"的前提（波次推进时它会一直变）；
	//   · 这里平均的是传入 Tick 的 DeltaSeconds；帧率限制、固定步长与场景负载都会影响它，
	//     不等于该函数的 CPU 耗时，性能归因需配合 Insights 等计时工具。
	int32 ActiveEnemies = -1;
	if (const URogueEnemyTrackerSubsystem* Tracker = World->GetSubsystem<URogueEnemyTrackerSubsystem>())
	{
		ActiveEnemies = Tracker->GetActiveEnemyCount();
	}

	const int32 SampledFrames = FMath::Max(1, FireReportFrameCount);
	const float AverageFrameMs = FireReportAccumulatedTime * 1000.0f / static_cast<float>(SampledFrames);
	FireReportAccumulatedTime = 0.0f;
	FireReportFrameCount = 0;

	const RogueEnemyAttackStats::FCounters& AttackCounters = RogueEnemyAttackStats::Get();

	UE_LOG(LogTemp, Log, TEXT("[SelfTest] 武器技能报告#%d | 本机(%s) 玩家=%s | 波次=%d 击杀=%d 敌人=%d 倒地=%d 复活剩余=%.2f"),
		WeaponFireReportCount,
		IsAuthoritySide() ? TEXT("服务端") : TEXT("客户端"),
		OwningPlayerState != nullptr ? *OwningPlayerState->GetPlayerName() : TEXT("?"),
		Wave, Kills, ActiveEnemies,
		IsDead() ? 1 : 0,
		GetReviveSecondsRemaining());

	if (IsAuthoritySide())
	{
		// 三种攻击的尝试/执行计数用于缩小调查范围，计数按本进程累计。
		// 尝试为零可能是未进入距离、目标无效或路径被跳过；尝试有值而执行为零
		// 也可能由冷却、标签或其他合法激活条件拒绝，不能直接认定 GAS 故障。
		UE_LOG(LogTemp, Log, TEXT("[SelfTest]   敌人攻击(能力执行)：接触=%d 远程=%d 地面波=%d | 尝试：接触=%d 远程=%d 地面波=%d | 最近 10 秒平均帧时=%.2f ms（%d 帧）"),
			AttackCounters.Contact, AttackCounters.Ranged, AttackCounters.GroundWave,
			AttackCounters.ContactAttempts, AttackCounters.RangedAttempts, AttackCounters.GroundWaveAttempts,
			AverageFrameMs, SampledFrames);
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const URogueWeaponAbility* Ability = Cast<URogueWeaponAbility>(Spec.GetPrimaryInstance());
		if (Ability == nullptr)
		{
			continue;
		}

		// 目标数也要打：否则"这把武器一次没开火"分不清是"没目标"还是"被冷却卡住"。
		int32 TargetCount = -1;
		if (const ARogueWeaponBase* Weapon = Ability->GetDrivenWeapon())
		{
			TArray<ARogueEnemy*> Targets;
			Weapon->CollectFireTargets(Targets);
			TargetCount = Targets.Num();
		}

		UE_LOG(LogTemp, Log, TEXT("[SelfTest]   能力 %-32s 激活=%d 开火=%d 持续心跳=%d 冷却中=%d 剩余=%.2f 目标=%d 该武器=%s"),
			*Ability->GetClass()->GetName(),
			Spec.IsActive() ? 1 : 0,
			Ability->GetFireCount(),
			Ability->GetContinuousTickCount(),
			Ability->IsWeaponCoolingDown() ? 1 : 0,
			Ability->GetCooldownRemainingForSelfTest(),
			TargetCount,
			*GetNameSafe(Ability->GetDrivenWeapon()));
	}

	for (const ARogueWeaponBase* Weapon : Weapons)
	{
		if (!IsValid(Weapon))
		{
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("[SelfTest]   武器 %-32s 生效冷却=%.2f 数量=%d 伤害=%.2f"),
			*Weapon->GetClass()->GetName(),
			Weapon->GetAbilityCooldown(), Weapon->GetEffectiveCount(), Weapon->GetEffectiveDamage());
	}
}