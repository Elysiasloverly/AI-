#include "Player/RogueCharacter.h"
#include "HAL/IConsoleManager.h"
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

		Row.DisplayName = TEXT("阴阳玉");
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
	// 通知、接管和 BeginPlay 的先后不固定，但一个 Pawn 只创建一组武器 Actor。
	if (Weapons.Num() > 0)
	{
		return;
	}
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
			// 蓝图/数据表继续决定模型、弹道和伤害；初始装备统一为一张御札。
			// 其它武器保留数量 0 的实例，抽到获取卡后由 GAS 数量加成启用。
			// 读取子类的实际数量类型，避免旧数据表中错配的数量枚举把其它武器当作御札。
			Weapon->GetConfig().Count = Weapon->GetCountUpgradeType() == ERogueUpgradeType::ProjectileCount ? 1 : 0;
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
	// 常规离散开火由 GA/Task 驱动；NeedsCosmeticTick 武器在此维护持续实体。
	// 当前镰刀会在各端创建和配置环绕刀片，客户端用于表现，服务端刀片仍可结算命中。
	// 因此整个函数不能概括为无伤害的纯表现路径。
	for (ARogueWeaponBase* Weapon : Weapons)
	{
		if (IsValid(Weapon) && Weapon->NeedsCosmeticTick())
		{
			Weapon->WeaponTick(DeltaSeconds);
		}
	}

	// 合并截至当前已排队的武器表现（见 BroadcastWeaponCosmetic）；Tick 次序可能使后续入队延至下次。
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

	// 档案握手前不在服务端授予/激活开火能力，避免按空属性打一轮后再补永久基值。
	if (IsAuthoritySide())
	{
		const ARoguePlayerState* State = GetPlayerState<ARoguePlayerState>();
		if (State == nullptr || !State->IsProgressionReady() || !State->IsBaseAttributesInitialized())
		{
			return;
		}
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
	// 以 ASC 内已存在的能力类查重，避免初始化重试时重复授予。
	// 当前复活保留同 Pawn/ASC；查重并不负责替换 Pawn 后的能力 SourceObject 或武器引用更新。
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

	// 尚未发现带自动攻击标签的能力时继续重试；可能是复制未到，也可能是武器未配置能力。
	if (WeaponAbilityCount > 0 && !bAnyInactive)
	{
		bWeaponAbilitiesSettled = true;
		return;
	}

	// ---- ③ 激活 ----
	if (bLocalController)
	{
		// 本人：本地激活（LocalPredicted 会顺带通知服务端在自己那份 ASC 上激活）。
		// 按公共资产标签尝试激活；是否成功仍取决于各能力的激活条件。
		ASC->TryActivateAbilitiesByTag(AutoAttackTag);
		return;
	}

	// ---- ④ 服务端上的远端玩家：等本人客户端的激活请求，超时再兜底 ----
	//
	// 先等所属客户端激活，避免服务端抢先激活与本地预测请求发生冲突。
	// 具体拒绝行为取决于能力实例和重触发条件，不能认定任何服务端提前激活都必然失败。
	// 初始化、所有权或激活失败可能让能力迟迟未运行；五秒后服务端尝试兜底，
	// 这不是对 Reliable RPC 单次丢包的补偿，也不保证所有失败状态都能恢复。
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
	// 仅在非客户端世界排队；此处按执行环境限定服务端表现广播。
	// HasAuthority 是 Actor Role 检查，本地控制不等于 Authority；
	// 客户端调用 NetMulticast 只会本地执行，不会自动上行再向全体广播。
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
#if !UE_BUILD_SHIPPING
	if (const auto* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue.Audio.Debug")); Debug && Debug->GetInt())
		UE_LOG(LogTemp, Display, TEXT("[AttackAudio] WeaponRPC Player=%s Alive=%d AuthoritySide=%d Local=%d Entries=%d"), *GetName(), IsAlive(), IsAuthoritySide(), IsLocallyControlled(), Fires.Num());
#endif
	if (!IsAlive())
	{
		return;
	}

	// 服务端已在真实发射时播过音效。客户端先接收确认音效，只有远端角色重放弹体；
	// 所属客户端保留自己的预测弹体，也能在服务端接管开火时听到确认音效。
	if (IsAuthoritySide())
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
			// 音效统一回放服务器确认的这一轮，包含所属客户端；本地预测不另播一份。
			if (Entry.Fire.bPlayAttackSound) { Weapon->PlayAttackAudio(Entry.Fire.Start); }
			if (!IsLocallyControlled()) { Weapon->PlayCosmeticFire(Entry.Fire); }
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

void ARogueCharacter::RefreshWeaponBonusSnapshotFromAttributes()
{
	if (!IsAuthoritySide())
	{
		return;
	}

	const URogueAttributeSet* AttributeSet = GetRogueAttributeSet();
	if (AttributeSet == nullptr)
	{
		return;
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
	Values.ScytheSize = AttributeSet->GetScytheSizeBonus();
	Values.ProjectileBurst = AttributeSet->GetProjectileBurstBonus();
	Values.RocketBlastRadius = AttributeSet->GetRocketBlastRadiusBonus();
	Values.HellTowerDamageRamp = AttributeSet->GetHellTowerDamageRampBonus();

	ApplyWeaponBonusSnapshot(Values);
}

void ARogueCharacter::ApplyWeaponBonusSnapshot(const FRogueWeaponBonusValues& InValues)
{
	// 权威字段：只有服务端能写（客户端写会被下一次复制覆盖）
	if (!IsAuthoritySide())
	{
		return;
	}

	// 脏检查：16 个值都没变就直接返回（调用方 GameMode 每帧都会推一次）
	if (WeaponBonusSnapshot.Equals(InValues))
	{
		return;
	}

	WeaponBonusSnapshot = InValues;
}
