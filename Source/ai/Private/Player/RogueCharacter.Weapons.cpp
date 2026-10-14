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

namespace
{
	const FName MortarWeaponRowName(TEXT("Mortar"));
	FRogueWeaponTableRow BuildDefaultMortarWeaponConfig(TSubclassOf<ARogueRocketProjectile> MortarProjectileClass)
	{
		FRogueWeaponTableRow Row;
		if (MortarProjectileClass == nullptr)
		{
			MortarProjectileClass = ARogueMortarProjectile::StaticClass();
		}

		Row.DisplayName = TEXT("迫击炮");
		Row.WeaponClass = ARogueWeapon_Mortar::StaticClass();
		Row.Count = 0;
		Row.Damage = 12.0f;
		Row.Cooldown = 2.35f;
		Row.Range = 2600.0f;
		Row.CountUpgradeType = ERogueUpgradeType::MortarCount;
		Row.MortarProjectileClass = Cast<UClass>(MortarProjectileClass.Get());
		Row.MortarExplosionRadius = 470.0f;
		Row.MortarLaunchSpeed = 760.0f;
		Row.RocketClass = MortarProjectileClass;
		Row.ExplosionRadius = 470.0f;
		Row.RocketSpeed = 760.0f;
		Row.bEnableTrajectoryDeviation = true;
		Row.TrajectoryDeviationMaxAngle = 2.0f;
		Row.bEnableSpeedRandomization = false;
		Row.SpeedRandomizationRatio = 0.0f;
		return Row;
	}

}

void ARogueCharacter::InitializeWeapons()
{
	if (WeaponDataTable == nullptr)
	{
		return;
	}

	auto SpawnWeaponInstance = [this](TSubclassOf<ARogueWeaponBase> WeaponClass, const FRogueWeaponTableRow& Config)
	{
		if (WeaponClass == nullptr)
		{
			return static_cast<ARogueWeaponBase*>(nullptr);
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		ARogueWeaponBase* Weapon = GetWorld()->SpawnActor<ARogueWeaponBase>(WeaponClass, GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
		if (Weapon != nullptr)
		{
			Weapon->InitializeWeapon(this, Config);
			Weapons.Add(Weapon);
		}
		return Weapon;
	};

	auto FindRowByCountUpgradeType = [this](ERogueUpgradeType CountUpgradeType, FName PreferredRowName, FRogueWeaponTableRow& OutRow) -> bool
	{
		if (WeaponDataTable == nullptr)
		{
			return false;
		}

		if (!PreferredRowName.IsNone())
		{
			if (const FRogueWeaponTableRow* PreferredRow = WeaponDataTable->FindRow<FRogueWeaponTableRow>(PreferredRowName, TEXT("FindRowByCountUpgradeType")))
			{
				OutRow = *PreferredRow;
				return true;
			}
		}

		const TArray<FName> TableRowNames = WeaponDataTable->GetRowNames();
		for (const FName RowName : TableRowNames)
		{
			if (const FRogueWeaponTableRow* Row = WeaponDataTable->FindRow<FRogueWeaponTableRow>(RowName, TEXT("FindRowByCountUpgradeType")))
			{
				if (Row->CountUpgradeType == CountUpgradeType)
				{
					OutRow = *Row;
					return true;
				}
			}
		}

		return false;
	};

	bool bHasMortarWeapon = false;
	const TArray<FName> RowNames = WeaponDataTable->GetRowNames();
	for (int32 Index = 0; Index < RowNames.Num(); ++Index)
	{
		const FRogueWeaponTableRow* Row = WeaponDataTable->FindRow<FRogueWeaponTableRow>(RowNames[Index], TEXT("InitializeWeapons"));
		if (Row == nullptr)
		{
			continue;
		}

		TSubclassOf<ARogueWeaponBase> WeaponClass = Row->WeaponClass;
		if (WeaponClass == nullptr && WeaponClasses.IsValidIndex(Index))
		{
			WeaponClass = WeaponClasses[Index];
		}

		if (WeaponClass == nullptr)
		{
			continue;
		}

		ARogueWeaponBase* Weapon = SpawnWeaponInstance(WeaponClass, *Row);
		bHasMortarWeapon = bHasMortarWeapon || Cast<ARogueWeapon_Mortar>(Weapon) != nullptr || Row->CountUpgradeType == ERogueUpgradeType::MortarCount;
	}

	if (!bHasMortarWeapon)
	{
		FRogueWeaponTableRow MortarConfig;
		if (!FindRowByCountUpgradeType(ERogueUpgradeType::MortarCount, MortarWeaponRowName, MortarConfig))
		{
			MortarConfig = BuildDefaultMortarWeaponConfig(DefaultMortarProjectileClass);
		}

		TSubclassOf<ARogueWeaponBase> MortarWeaponClass = DefaultMortarWeaponClass;
		if (MortarWeaponClass == nullptr)
		{
			MortarWeaponClass = ARogueWeapon_Mortar::StaticClass();
		}

		if (MortarConfig.RocketClass == nullptr)
		{
			TSubclassOf<ARogueRocketProjectile> MortarProjectileClass = DefaultMortarProjectileClass;
			if (MortarProjectileClass == nullptr)
			{
				MortarProjectileClass = ARogueMortarProjectile::StaticClass();
			}
			MortarConfig.RocketClass = MortarProjectileClass;
		}

		if (MortarConfig.MortarProjectileClass == nullptr)
		{
			MortarConfig.MortarProjectileClass = Cast<UClass>(MortarConfig.RocketClass.Get());
		}
		if (MortarConfig.MortarExplosionRadius <= 0.0f)
		{
			MortarConfig.MortarExplosionRadius = MortarConfig.ExplosionRadius;
		}
		if (MortarConfig.MortarLaunchSpeed <= 0.0f)
		{
			MortarConfig.MortarLaunchSpeed = MortarConfig.RocketSpeed;
		}

		SpawnWeaponInstance(MortarWeaponClass, MortarConfig);
	}
}

void ARogueCharacter::TickWeapons(float DeltaSeconds)
{
	// 这里只剩"纯表现"了 —— 武器开火已经全部搬进 GameplayAbility
	// （URogueWeaponAbility + URogueAbilityTask_WeaponFireLoop，见 EnsureWeaponAbilities）。
	//
	// 为什么还留着这条路径：环绕刀片（镰刀）的表现是一条**连续轨道**，没有"某一发"
	// 可以让服务端广播，所以在谁那儿都得自己画；而技能列表只复制给本人
	// （ASC 的 ActivatableAbilities 是 COND_OwnerOnly），队友客户端根本没有那份能力。
	// 所以这类武器由 NeedsCosmeticTick() 标记，继续在这里跑。
	//
	// 联机分工（改造前后一致）：
	//   · 服务端 / 本人客户端：开火（现在在能力里）；
	//   · 其它客户端：只画服务端广播过来的那一发（见 BroadcastCosmeticFire）。
	//
	// 注意这里**不**加 `HasAuthority() || IsLocallyControlled()`：
	// 改造前那条判据是 `bAuthoritative || bLocallyControlled || NeedsCosmeticTick()`，
	// 对"持续型表现武器"来说恒为真（它们在任何机器上都要画自己的轨道），
	// 所以循环里只留 NeedsCosmeticTick() 才与改造前逐台机器等价。
	for (ARogueWeaponBase* Weapon : Weapons)
	{
		if (IsValid(Weapon) && Weapon->NeedsCosmeticTick())
		{
			Weapon->WeaponTick(DeltaSeconds);
		}
	}

	// 本帧所有武器表现合成一条 RPC 发出（见 BroadcastWeaponCosmetics 的说明）。
	// 放在这里而不是每发即发：一次齐射（5 连发）从 5 条 RPC 降成 1 条。
	FlushWeaponCosmetics();
}

void ARogueCharacter::EnsureWeaponAbilities()
{
	// 每帧调一次，但只有在"还没全部就绪"时才真的干活（见头文件）。
	if (bWeaponAbilitiesSettled || Weapons.Num() == 0)
	{
		return;
	}

	URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
	if (ASC == nullptr)
	{
		// ASC 还没就位（PlayerState 复制中）。下一帧再来。
		return;
	}

	const bool bAuthority = ASC->IsOwnerActorAuthoritative();

	// 别人的武器不由本机模拟：客户端只跑**自己**那份（这一点与改造前 TickWeapons 的判据一致）。
	// 服务端除外 —— 它要权威地跑所有人的武器（伤害结算只能在这里）。
	const bool bLocalController = IsLocallyControlled();
	if (!bAuthority && !bLocalController)
	{
		bWeaponAbilitiesSettled = true;
		return;
	}

	const FGameplayTagContainer AutoAttackTag = FGameplayTagContainer(RogueGameplayTags::Ability_WeaponAutoAttack);

	// ---- ① 授予：只在权威端，且幂等 ----
	//
	// 幂等判据不是"本角色是否授予过"，而是"这个 ASC 上有没有同类能力"：
	// 角色重生时 Pawn 会换、但 PlayerState 与它上面的 ASC 是保留的，
	// 用角色上的 bool 会导致重复授予 —— 表现就是**每把武器开两枪**。
	if (bAuthority)
	{
		for (const ARogueWeaponBase* Weapon : Weapons)
		{
			if (!IsValid(Weapon))
			{
				continue;
			}

			const TSubclassOf<URogueWeaponAbility> AbilityClass = Weapon->GetFireAbilityClass();
			if (AbilityClass == nullptr)
			{
				// 不参与能力驱动的武器（镰刀：纯表现轨道，见它的头文件）。
				continue;
			}

			bool bAlreadyGranted = false;
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.Ability != nullptr && Spec.Ability->GetClass() == AbilityClass.Get())
				{
					bAlreadyGranted = true;
					break;
				}
			}

			if (!bAlreadyGranted)
			{
				ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, /*Level=*/1, /*InputID=*/INDEX_NONE, /*SourceObject=*/this));
			}
		}
	}

	// ---- ② 是否已经全部在跑 ----
	bool bAnyInactive = false;
	int32 WeaponAbilityCount = 0;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability == nullptr || !Spec.Ability->GetAssetTags().HasTag(RogueGameplayTags::Ability_WeaponAutoAttack))
		{
			continue;
		}
		++WeaponAbilityCount;
		if (!Spec.IsActive())
		{
			bAnyInactive = true;
			break;
		}
	}

	// 一个都没有 = 能力还没复制到本机（客户端常见的前几帧）；不算失败，继续等。
	if (WeaponAbilityCount > 0 && !bAnyInactive)
	{
		bWeaponAbilitiesSettled = true;
		return;
	}

	// ---- ③ 激活 ----
	if (bLocalController)
	{
		// 本人：本地激活（LocalPredicted 会顺带通知服务端在自己那份 ASC 上激活）。
		// 一次调用把全部武器能力都激活（它们共用 Ability.Weapon.AutoAttack 资产标签）。
		ASC->TryActivateAbilitiesByTag(AutoAttackTag);
		return;
	}

	// ---- ④ 服务端上的远端玩家：等本人客户端的激活请求，超时再兜底 ----
	//
	// 为什么不能由服务端直接激活：LocalPredicted 的能力如果服务端已经激活，
	// 客户端那条激活请求会被判失败（ClientActivateAbilityFailed），
	// 于是**客户端本地那份会被结束** —— 本人玩家就失去了零延迟的开火表现。
	// 但也不能干等：请求万一丢了，这个玩家的武器就永远不会权威开火（伤害全没），
	// 而且这种失败是静默的。所以加一个看门狗，超时后由服务端接管。
	const UWorld* World = GetWorld();
	const float NowSeconds = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	if (WeaponAbilityWaitStartTime < 0.0f)
	{
		WeaponAbilityWaitStartTime = NowSeconds;
		return;
	}

	if (NowSeconds - WeaponAbilityWaitStartTime > 5.0f)
	{
		if (ASC->TryActivateAbilitiesByTag(AutoAttackTag))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[武器技能] 玩家 %s 的客户端在 5 秒内没有激活武器能力，服务端已接管激活（本人端将失去本地预测）"),
				*GetName());
		}
	}
}

void ARogueCharacter::BroadcastWeaponCosmetic(const ARogueWeaponBase* Weapon, const FRogueWeaponCosmeticFire& Fire)
{
	// ⚠️ 判据必须是 IsAuthoritySide()（GetNetMode）而不是 HasAuthority()：
	// 客户端上**本机拥有**的角色 Role 也是 ROLE_Authority，HasAuthority() 会返回 true，
	// 于是客户端的开火表现会被塞进队列，最后在 FlushWeaponCosmetics 里
	// 从客户端发出一条 Multicast RPC（本地会被 IsLocallyControlled 兜住、看不出异常，
	// 但那是一次无意义的上行广播，而且闸门本身已经失效）。
	if (!IsAuthoritySide() || Weapon == nullptr)
	{
		return;
	}

	const int32 WeaponIndex = Weapons.IndexOfByKey(Weapon);
	if (WeaponIndex == INDEX_NONE)
	{
		return;
	}

	// 只入队不发送：本帧所有武器（含一次齐射的多发）会在 TickWeapons 末尾合成一条 RPC。
	FRogueWeaponCosmeticBatchEntry& Entry = PendingWeaponCosmetics.AddDefaulted_GetRef();
	Entry.WeaponIndex = WeaponIndex;
	Entry.Fire = Fire;
}

void ARogueCharacter::FlushWeaponCosmetics()
{
	// 同 BroadcastWeaponCosmetic：判据用 IsAuthoritySide()，理由见那里的注释。
	if (!IsAuthoritySide() || PendingWeaponCosmetics.Num() == 0)
	{
		return;
	}

	// 一条 RPC 承载整帧的表现。Unreliable 是有意的：这是纯表现，
	// 丢一帧只是"少看见一两发"，不影响任何结算。
	Multicast_WeaponCosmetics(PendingWeaponCosmetics);
	PendingWeaponCosmetics.Reset();
}

void ARogueCharacter::Multicast_WeaponCosmetics_Implementation(const TArray<FRogueWeaponCosmeticBatchEntry>& Fires)
{
	if (!IsAlive())
	{
		return;
	}

	// 两类机器跳过（它们各自已经有"真实/预测"的那一份了）：
	//   · 服务端：权威那一发就是它自己生成的；
	//   · 本人：自己那一发是本地预测。
	// 这里用 IsAuthoritySide() 而不是 HasAuthority()：语义是"我是不是权威端"，
	// 与广播侧保持同一判据（客户端上本机拥有的角色 HasAuthority() 也是 true）。
	if (IsAuthoritySide() || IsLocallyControlled())
	{
		return;
	}

	for (const FRogueWeaponCosmeticBatchEntry& Entry : Fires)
	{
		if (!Weapons.IsValidIndex(Entry.WeaponIndex))
		{
			continue;
		}

		if (ARogueWeaponBase* Weapon = Weapons[Entry.WeaponIndex])
		{
			Weapon->PlayCosmeticFire(Entry.Fire);
		}
	}
}

// ---- 武器系统方法 ----
// 注：ApplySharedWeapon*Bonus 与 DispatchWeaponUpgrade 已删除。
// 武器升级不再改武器 Actor 上的配置，而是改玩家 ASC 上的 URogueAttributeSet
// （见 ApplyUpgrade 里武器分支的说明），所以这里没有"把加成推给每把武器"这一步了。

// ---- 向后兼容的武器查询 ----

float ARogueCharacter::GetAttackDamage() const
{
	const ARogueWeapon_Projectile* PW = FindWeapon<ARogueWeapon_Projectile>();
	return PW != nullptr ? PW->GetDamage() : 0.0f;
}

float ARogueCharacter::GetAttackInterval() const
{
	const ARogueWeapon_Projectile* PW = FindWeapon<ARogueWeapon_Projectile>();
	return PW != nullptr ? PW->GetInterval() : 1.0f;
}

int32 ARogueCharacter::GetProjectileCount() const
{
	const ARogueWeapon_Projectile* PW = FindWeapon<ARogueWeapon_Projectile>();
	return PW != nullptr ? PW->GetCount() : 1;
}

int32 ARogueCharacter::GetEffectiveProjectileCount() const
{
	const ARogueWeapon_Projectile* PW = FindWeapon<ARogueWeapon_Projectile>();
	return PW != nullptr ? PW->GetEffectiveProjectileCount() : 1;
}

int32 ARogueCharacter::GetScytheCount() const
{
	const ARogueWeapon_Scythe* SW = FindWeapon<ARogueWeapon_Scythe>();
	return SW != nullptr ? SW->GetCount() : 0;
}

int32 ARogueCharacter::GetEffectiveScytheCount() const
{
	const ARogueWeapon_Scythe* SW = FindWeapon<ARogueWeapon_Scythe>();
	return SW != nullptr ? SW->GetEffectiveCount() : 0;
}

int32 ARogueCharacter::GetRocketLauncherCount() const
{
	const ARogueWeapon_Rocket* RW = FindWeapon<ARogueWeapon_Rocket>();
	return RW != nullptr ? RW->GetCount() : 0;
}

int32 ARogueCharacter::GetEffectiveRocketLauncherCount() const
{
	const ARogueWeapon_Rocket* RW = FindWeapon<ARogueWeapon_Rocket>();
	return RW != nullptr ? RW->GetEffectiveCount() : 0;
}

int32 ARogueCharacter::GetLaserCannonCount() const
{
	const ARogueWeapon_Laser* LW = FindWeapon<ARogueWeapon_Laser>();
	return LW != nullptr ? LW->GetCount() : 0;
}

int32 ARogueCharacter::GetEffectiveLaserCannonCount() const
{
	const ARogueWeapon_Laser* LW = FindWeapon<ARogueWeapon_Laser>();
	return LW != nullptr ? LW->GetEffectiveCount() : 0;
}

int32 ARogueCharacter::GetHellTowerCount() const
{
	const ARogueWeapon_HellTower* HW = FindWeapon<ARogueWeapon_HellTower>();
	return HW != nullptr ? HW->GetCount() : 0;
}

int32 ARogueCharacter::GetEffectiveHellTowerCount() const
{
	const ARogueWeapon_HellTower* HW = FindWeapon<ARogueWeapon_HellTower>();
	return HW != nullptr ? HW->GetEffectiveCount() : 0;
}

int32 ARogueCharacter::GetMortarCount() const
{
	const ARogueWeapon_Mortar* MW = FindWeapon<ARogueWeapon_Mortar>();
	return MW != nullptr ? MW->GetCount() : 0;
}

int32 ARogueCharacter::GetEffectiveMortarCount() const
{
	const ARogueWeapon_Mortar* MW = FindWeapon<ARogueWeapon_Mortar>();
	return MW != nullptr ? MW->GetEffectiveCount() : 0;
}

int32 ARogueCharacter::GetLaserRefractionCount() const
{
	const ARogueWeapon_Laser* LW = FindWeapon<ARogueWeapon_Laser>();
	return LW != nullptr ? LW->GetRefractionCount() : 0;
}

void ARogueCharacter::ApplyWeaponBonusSnapshot(const FRogueWeaponBonusValues& InValues)
{
	// 权威字段：只有服务端能写（客户端写会被下一次复制覆盖）
	if (!IsAuthoritySide())
	{
		return;
	}

	// 脏检查：12 个值都没变就直接返回（调用方 GameMode 每帧都会推一次）
	if (WeaponBonusSnapshot.Equals(InValues))
	{
		return;
	}

	WeaponBonusSnapshot = InValues;
}