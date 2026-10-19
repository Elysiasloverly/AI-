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


void ARogueCharacter::AddExperience(int32 ExperienceAmount)
{
	if (!IsAlive())
	{
		return;
	}

	// 成长数据是**服务端权威**：经验球可能在客户端本地存在，
	// 但"加经验"必须只由服务端做，否则客户端会自己涨经验（服务端不知道，
	// 而成长字段是复制的 → 客户端本地值会被下一帧的复制覆盖回去，表现为"经验乱跳"）。
	if (!HasAuthority())
	{
		return;
	}

	const float TotalExperience = static_cast<float>(ExperienceAmount) * ExperienceMultiplier + ExperienceRemainder;
	const int32 GrantedExperience = FMath::FloorToInt(TotalExperience);
	ExperienceRemainder = TotalExperience - static_cast<float>(GrantedExperience);
	CurrentExperience += GrantedExperience;

	int32 LevelUpGuard = 0;
	while (ExperienceToNextLevel > 0 && CurrentExperience >= ExperienceToNextLevel && LevelUpGuard < 20)
	{
		CurrentExperience -= ExperienceToNextLevel;
		LevelUp();
		++LevelUpGuard;
	}
}

void ARogueCharacter::AddMoney(int32 Amount)
{
	// 同样只在服务端加钱（金额是复制的，客户端本地加会被覆盖）
	if (!HasAuthority())
	{
		return;
	}

	Money += FMath::Max(0, Amount);
}

void ARogueCharacter::SetNearbyShopTerminal(ARogueShopTerminal* ShopTerminal)
{
	NearbyShopTerminal = ShopTerminal;
}

bool ARogueCharacter::TrySpendMoney(int32 Amount)
{
	// 扣钱是权威操作：客户端不能自己扣（会被复制覆盖），更不能自己"少扣"
	if (!HasAuthority())
	{
		return false;
	}

	if (Amount < 0 || Money < Amount)
	{
		return false;
	}

	Money -= Amount;
	return true;
}

void ARogueCharacter::ClearNearbyShopTerminal(const ARogueShopTerminal* ShopTerminal)
{
	if (NearbyShopTerminal.Get() == ShopTerminal)
	{
		NearbyShopTerminal = nullptr;
	}
}

bool ARogueCharacter::GetShopPromptWorldLocation(FVector& OutLocation) const
{
	const ARogueShopTerminal* ShopTerminal = NearbyShopTerminal.Get();
	if (!IsValid(ShopTerminal))
	{
		return false;
	}

	OutLocation = ShopTerminal->GetPromptWorldLocation();
	return true;
}

void ARogueCharacter::ApplyUpgrade(const FRogueUpgradeOption& Upgrade)
{
	// 升级落地只走这一条路（原来是 ApplyUpgrade → FRogueUpgradeEffectApplier::ApplyToCharacter
	// 再转一手，纯粹多一层跳转，已合并进来）。
	//   · 玩家属性与武器加成 → 各类型的 Infinite GameplayEffect，按目标叠层
	//   · 武器表现 → 读取复制后的属性加成与角色快照
	//   · 联动效果 → 护甲上限提升后立刻开始回复、冲刺冷却缩短后不必等旧冷却

	// 记录升级前的上限，用于护甲上限提升后的"立即开始回复"联动
	const float PreviousMaxArmor = MaxArmor;

	switch (Upgrade.Type)
	{
	// ---- 属性类升级：统一通过 GAS 的 GameplayEffect 施加 ----
	// RogueUpgradeEffectMap 把枚举映射到具体的升级 GE 子类；
	// 每个子类绑定一个目标属性，数值走 SetByCaller(Data.Magnitude)，重复购买增加层数。
	//
	// 迁移前的做法是把 Magnitude 塞进自研的 UpgradeGroup 再整表重算，
	// 那条路径已经整体删除 —— 属性现在只有一个真相源（GAS 属性集），
	// 而且升级结果会随属性复制自动同步到客户端。
	//
	// 注意"等量回血/回甲"这条联动**没有丢**：它搬到了
	// HandleMaxHealthChanged / HandleMaxArmorChanged（监听上限变化，按增量补当前值），
	// 所以这里不再需要 PreviousMaxHealth 那套前后差值计算。
	case ERogueUpgradeType::MaxHealth:
	case ERogueUpgradeType::MoveSpeed:
	case ERogueUpgradeType::ArmorCapacity:
	case ERogueUpgradeType::PickupRadius:
	case ERogueUpgradeType::Recovery:
	case ERogueUpgradeType::Armor:
	case ERogueUpgradeType::ExperienceGain:
		ApplyUpgradeEffectViaGAS(Upgrade.Type, Upgrade.Magnitude);
		break;

	case ERogueUpgradeType::DashCooldown:
		// 冲刺冷却是**负向**升级：数值取负表达"减少冷却"。
		// 属性的下限钳制在 URogueAttributeSet::PreAttributeChange（读 MinDashCooldown 属性）。
		ApplyUpgradeEffectViaGAS(Upgrade.Type, -Upgrade.Magnitude);
		// 让玩家不必继续等旧的更长冷却
		ClearDashCooldown();
		break;

	// ---- 武器类升级：与属性升级**同一条路** ----
	// 升级 GE 改玩家属性集中的加成，施加成功后立即刷新武器读取的角色快照。
	// 客户端不重放升级加法；复制仍有延迟，不能保证每一瞬间与服务器数值一致。
	case ERogueUpgradeType::AttackPower:
	case ERogueUpgradeType::AttackSpeed:
	case ERogueUpgradeType::AttackRange:
	case ERogueUpgradeType::ProjectileSpeed:
	case ERogueUpgradeType::ProjectileCount:
	case ERogueUpgradeType::ScytheCount:
	case ERogueUpgradeType::RocketCount:
	case ERogueUpgradeType::LaserCount:
	case ERogueUpgradeType::HellTowerCount:
	case ERogueUpgradeType::LaserRefraction:
	case ERogueUpgradeType::MortarCount:
	case ERogueUpgradeType::MortarBlastRadius:
	case ERogueUpgradeType::ScytheSize:
		ApplyUpgradeEffectViaGAS(Upgrade.Type, Upgrade.Magnitude);
		break;

	default:
		break;
	}

	// 属性镜像平时是每帧在 Tick 里同步的，但这里必须**立刻**同步一次：
	// 升级刚改完属性集，镜像还停留在上一帧的值 ——
	// 下面的联动判断（MaxArmor 是否变大）如果读旧镜像就永远为假，联动会静默失效。
	// 顺带也让 HUD 在升级的当帧就显示新数值，而不是等下一帧。
	SyncAttributeMirrors();

	// 护甲上限提升后立即开始回复（清掉"刚受伤"状态）。
	// 其余联动（回血/回甲、冲刺冷却收缩）由属性变化委托与 ClearDashCooldown 处理。
	if (Upgrade.Type == ERogueUpgradeType::ArmorCapacity && MaxArmor > PreviousMaxArmor)
	{
		ClearRecentlyDamagedState();
	}
}

float ARogueCharacter::GetExperiencePercent() const
{
	return ExperienceToNextLevel > 0 ? static_cast<float>(CurrentExperience) / static_cast<float>(ExperienceToNextLevel) : 0.0f;
}

void ARogueCharacter::RequestUpgradeSelection(int32 UpgradeIndex)
{
	// 联机正确路径：请求发到 PlayerState 的 ServerRPC，由服务端校验并施加。
	// 旧写法是直接调 `GetWorld()->GetAuthGameMode()->TrySelectUpgrade(...)`，
	// 而 GetAuthGameMode() 在客户端返回 nullptr —— 客户端玩家选了也没用。
	//
	// 注意这个函数在**服务端与客户端都可用**：
	//   · 客户端调用 → 走 RPC 到服务端；
	//   · 服务端（含 Listen Server 的主机玩家）调用 → RPC 在本机直接执行。
	if (ARoguePlayerState* RoguePlayerState = GetPlayerState<ARoguePlayerState>())
	{
		RoguePlayerState->Server_RequestUpgradeSelection(UpgradeIndex);
	}
}

void ARogueCharacter::ChooseUpgradeOne()
{
	RequestUpgradeSelection(0);
}

void ARogueCharacter::ChooseUpgradeTwo()
{
	RequestUpgradeSelection(1);
}

void ARogueCharacter::ChooseUpgradeThree()
{
	RequestUpgradeSelection(2);
}

void ARogueCharacter::LevelUp()
{
	++PlayerLevel;
	const FRoguePlayerProgressionConfig Progression = LoadedPlayerBalanceAsset
		? LoadedPlayerBalanceAsset->Progression : FRoguePlayerProgressionConfig();
	ExperienceToNextLevel = Progression.GetNextExperienceRequirement(ExperienceToNextLevel);

	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->StartUpgradeSelection(this);
	}
}
