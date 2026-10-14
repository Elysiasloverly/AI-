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
// Enhanced Input（UE5 标准输入）。资产清单见 Docs/Enhanced-Input-Migration.md；
// 资产缺失时 SetupEnhancedInput 会返回 false，输入整体回退到 legacy 绑定。
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
	//   · 帧时用来对比 Phase 3 前后的性能（200+ 敌人不退化这条红线）。
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
		// 敌人攻击（Phase 3）：三种攻击各执行了多少次。
		// 这三个数字是"攻击真的改成能力驱动了"的直接证据 ——
		// 它们只在 URogueEnemyAttackAbility 的激活路径里自增。
		// "尝试"与"执行"必须一起看：两者都是 0 = 敌人根本没走到攻击距离；
		// 尝试 > 0 而执行 = 0 = GAS 激活这一步出了问题。
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