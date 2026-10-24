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
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
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
#include "Combat/Visuals/RogueCombatFeedback.h"
#include "Components/CapsuleComponent.h"
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
	const TCHAR* DefaultPlayerBalanceAssetPath = TEXT("/Game/Data/Player/DA_PlayerBalance.DA_PlayerBalance");

	URoguePlayerBalanceAsset* LoadConfiguredOrDefaultPlayerBalanceAsset(const TSoftObjectPtr<URoguePlayerBalanceAsset>& ConfiguredAsset)
	{
		if (!ConfiguredAsset.IsNull())
		{
			if (URoguePlayerBalanceAsset* LoadedAsset = ConfiguredAsset.LoadSynchronous())
			{
				return LoadedAsset;
			}
		}

		const FSoftObjectPath DefaultObjectPath(DefaultPlayerBalanceAssetPath);
		const TSoftObjectPtr<URoguePlayerBalanceAsset> DefaultAsset(DefaultObjectPath);
		return DefaultAsset.IsNull() ? nullptr : DefaultAsset.LoadSynchronous();
	}

}

void ARogueCharacter::BindAbilitySystemEvents()
{
	URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
	if (ASC == nullptr || bAbilitySystemEventsBound)
	{
		return;
	}
	bAbilitySystemEventsBound = true;

	// 死亡标签事件在权威端安排下一 Tick 的善后处理；
	// 显示镜像由 SyncAttributeMirrors 轮询，上限增量补偿则由下面两个属性委托完成。
	ASC->RegisterGameplayTagEvent(RogueGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ARogueCharacter::HandleDeathTagChanged);

	// ---- 上限提升时"补满那部分增量"（原本住在 URogueAttributeSet::PostGameplayEffectExecute）----
	//
	// 非周期 Infinite 升级通过聚合修饰器改变 CurrentValue，不执行 PostGameplayEffectExecute。
	// 属性委托可捕获这类上限变化；服务端处理函数只在增量为正时补当前值。
	// PostGameplayEffectExecute 也会用于周期执行，不能泛化为只对 Instant GE 有效。
	ASC->GetGameplayAttributeValueChangeDelegate(URogueAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &ARogueCharacter::HandleMaxHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(URogueAttributeSet::GetMaxArmorAttribute())
		.AddUObject(this, &ARogueCharacter::HandleMaxArmorChanged);
}

void ARogueCharacter::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	// 增量补偿只在非客户端世界执行。当前属性为普通 Replicated；
	// 收到复制不会自动调用 GAS 的 SetBaseAttributeValueFromReplication 或属性委托，
	// 若引入预测通知，应增加相应 RepNotify/GAMEPLAYATTRIBUTE_REPNOTIFY 并重新验证。
	if (!IsAuthoritySide())
	{
		return;
	}

	const float Delta = Data.NewValue - Data.OldValue;
	if (Delta > 0.0f)
	{
		SetCurrentHealth(GetCurrentHealth() + Delta);
	}
}

void ARogueCharacter::HandleMaxArmorChanged(const FOnAttributeChangeData& Data)
{
	// 同 HandleMaxHealthChanged：只在权威侧补，理由见那里。
	if (!IsAuthoritySide())
	{
		return;
	}

	const float Delta = Data.NewValue - Data.OldValue;
	if (Delta > 0.0f)
	{
		SetCurrentArmor(GetCurrentArmor() + Delta);
	}
}

void ARogueCharacter::SyncAttributeMirrors()
{
	const URogueAttributeSet* AttrSet = GetRogueAttributeSet();
	if (AttrSet == nullptr)
	{
		return;
	}

	// 为 HUD、移动与现有查询接口维护属性镜像；属性集就绪后按帧刷新。
	// BeginPlay 等初始化路径仍会写默认缓存，所以这里不是所有生命周期阶段的唯一写入点。
	// 运行时数值变更应先作用于属性集，需要即时观察时显式调用本函数。
	const float PreviousHealth = CurrentHealth;
	MaxHealth = AttrSet->GetMaxHealth();
	MaxArmor = AttrSet->GetMaxArmor();
	CurrentHealth = AttrSet->GetHealth();
	CurrentArmor = AttrSet->GetArmor();
	MoveSpeed = AttrSet->GetMoveSpeed();
	PickupRadius = AttrSet->GetPickupRadius();
	ExperienceMultiplier = AttrSet->GetExperienceMultiplier();
	DashCooldownDuration = AttrSet->GetDashCooldownDuration();
	DashMinCooldown = AttrSet->GetMinDashCooldown();
	DashDuration = AttrSet->GetDashDuration();
	DashSpeed = AttrSet->GetDashSpeed();

	// 回血飘字：生命上涨时弹绿色数字。两类"不是治疗"的情况要排除：
	//   1. 首次同步（出生/进场，缓存还是默认值）；
	//   2. 0 → 满血的整段补满（复活/重连初始化，弹 +100 会像血条出错）。
	// 持续回复与吸血走同一条路径，由 ShowHealingNumber 的窗口合并成一条。
	if (bHealthFeedbackInitialized)
	{
		const bool bLooksLikeInitialFill =
			PreviousHealth <= KINDA_SMALL_NUMBER && CurrentHealth >= MaxHealth - KINDA_SMALL_NUMBER;
		if (!bLooksLikeInitialFill && CurrentHealth > PreviousHealth + KINDA_SMALL_NUMBER)
		{
			ShowHealingNumber(CurrentHealth - PreviousHealth);
		}
	}
	bHealthFeedbackInitialized = true;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = MoveSpeed;
	}
}

void ARogueCharacter::ShowHealingNumber(float AppliedHealing)
{
	if (AppliedHealing <= 0.0f || GetWorld() == nullptr)
	{
		return;
	}

	// 持续回复（0.25s 一跳）与吸血会频繁触发，先累计再按窗口弹出，避免刷屏。
	PendingHealingNumber += AppliedHealing;
	TryFlushHealingNumber();
}

void ARogueCharacter::TryFlushHealingNumber()
{
	if (PendingHealingNumber <= KINDA_SMALL_NUMBER || GetWorld() == nullptr)
	{
		return;
	}

	constexpr float HealingNumberInterval = 0.35f;
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastHealingNumberTime < HealingNumberInterval)
	{
		return;
	}

	LastHealingNumberTime = CurrentTime;
	const float ShownHealing = PendingHealingNumber;
	PendingHealingNumber = 0.0f;

	// 头顶偏上弹出，和敌人血条飘字区分开；水平加一点随机避免连跳重叠成一条。
	FVector HealingLocation = GetActorLocation();
	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		HealingLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
	}
	HealingLocation.Z += 34.0f;
	HealingLocation.X += FMath::FRandRange(-16.0f, 16.0f);
	HealingLocation.Y += FMath::FRandRange(-16.0f, 16.0f);

	RogueCombatFeedback::AddHealingNumber(this, HealingLocation, ShownHealing);
}

void ARogueCharacter::SetCurrentHealth(float NewHealth)
{
	// 写属性集基值；CurrentHealth 由 SyncAttributeMirrors 刷新，不由属性回调刷新。
	if (URogueAttributeSet* AttrSet = GetRogueAttributeSet())
	{
		AttrSet->SetHealth(FMath::Max(0.0f, NewHealth));
	}
}

void ARogueCharacter::SetCurrentArmor(float NewArmor)
{
	// 与生命值同理：写属性集基值，再由 SyncAttributeMirrors 刷新护甲镜像。
	if (URogueAttributeSet* AttrSet = GetRogueAttributeSet())
	{
		AttrSet->SetArmor(FMath::Max(0.0f, NewArmor));
	}
}

void ARogueCharacter::ClearRecentlyDamagedState()
{
	URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	FGameplayTagContainer RecentlyDamagedTag;
	RecentlyDamagedTag.AddTag(RogueGameplayTags::State_RecentlyDamaged);
	ASC->RemoveActiveEffectsWithGrantedTags(RecentlyDamagedTag);
}

void ARogueCharacter::ClearDashCooldown()
{
	URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	FGameplayTagContainer DashCooldownTag;
	DashCooldownTag.AddTag(RogueGameplayTags::Cooldown_Dash);
	ASC->RemoveActiveEffectsWithGrantedTags(DashCooldownTag);
}

bool ARogueCharacter::ApplyUpgradeEffectViaGAS(ERogueUpgradeType UpgradeType, float Magnitude)
{
	// 升级是服务端权威操作：客户端只收属性同步结果
	URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
	if (ASC == nullptr || !ASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	const TSubclassOf<UGameplayEffect> EffectClass = RogueUpgradeEffectMap::GetEffectClass(UpgradeType);
	if (EffectClass == nullptr)
	{
		// 枚举加了但没在 RogueUpgradeEffectMap 里映射。
		// 静默忽略会表现成"卡牌能抽到但没效果"（最难归因的一类），所以打 Warning。
		UE_LOG(LogTemp, Warning, TEXT("[Rogue] 升级类型 %d 没有对应的 GE 类，本次升级被忽略（检查 RogueUpgradeEffectMap::GetEffectClass）"),
			static_cast<int32>(UpgradeType));
		return false;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	// 每种升级使用独立的 Infinite GE 类，固定目标属性并按目标叠层。
	// 引擎叠层按相同 Def 匹配（按来源叠层时还会检查 SourceASC），不会按动态标签分栈。
	// 当前线性修饰器在每层量级相同、未触及上限时按层数累积；
	// 不同量级的重复购买、层数上限和属性钳制需单独验证，不能等同于任意逐次加法。
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	// 数值。冲刺冷却是负向升级，调用方传负数（符号语义由数据决定，不写死在 GE 里）。
	SpecHandle.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Magnitude, Magnitude);

	const bool bApplied = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()).WasSuccessfullyApplied();

	if (bApplied)
	{
		// 连续升级会在同一调用栈生成下一轮卡牌，不能等 GameMode 下一帧才刷新数量。
		RefreshWeaponBonusSnapshotFromAttributes();
		UE_LOG(LogTemp, Verbose, TEXT("[GAS] 升级已施加：类型=%d 数值=%.2f GE=%s（同类型再买会叠加层数）"),
			static_cast<int32>(UpgradeType), Magnitude, *EffectClass->GetName());
	}

	return bApplied;
}

void ARogueCharacter::HandleDeathTagChanged(FGameplayTag Tag, int32 NewCount)
{
	(void)Tag;
	// 客户端的迟到 GAS 标签不能启动本地死亡/复活；只信角色生命周期快照。
	if (!IsAuthoritySide() || NewCount <= 0 || LifeState.Phase != ERoguePlayerLifePhase::Alive || bPendingDeath)
	{
		return;
	}
	bPendingDeath = true;
	// 立刻退出存活查询，善后延后一帧，避开正在执行的 GameplayEffect 重入。
	if (UWorld* World = GetWorld())
	{
		PendingDeathTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ARogueCharacter::ProcessDeath);
	}
}

URogueAbilitySystemComponent* ARogueCharacter::GetRogueAbilitySystemComponent() const
{
	const ARoguePlayerState* RoguePS = GetPlayerState<ARoguePlayerState>();
	return RoguePS != nullptr ? RoguePS->GetRogueAbilitySystemComponent() : nullptr;
}

URogueAttributeSet* ARogueCharacter::GetRogueAttributeSet() const
{
	const ARoguePlayerState* RoguePS = GetPlayerState<ARoguePlayerState>();
	return RoguePS != nullptr ? RoguePS->GetAttributeSet() : nullptr;
}

void ARogueCharacter::NotifyProgressionReady()
{
	if (!IsAuthoritySide())
	{
		return;
	}
	ARoguePlayerState* State = GetPlayerState<ARoguePlayerState>();
	if (State == nullptr || !State->IsProgressionReady())
	{
		return;
	}

	// 快照只在本局开始时接收一次。属性初始化也由 PlayerState 守卫，复活和接管不会再加一份。
	State->RefreshAbilityActorInfo(this);
	InitializeGASAttributes();
	State->RestoreStageTravelGrowth(this);
	BindAbilitySystemEvents();
	ApplyLifeStateEffects();
	ApplyDownedState();
	SyncAttributeMirrors();
	EnsureWeaponAbilities();
}

void ARogueCharacter::InitializeGASAttributes()
{
	// 此初始化仅允许在非客户端世界运行；IsAuthoritySide 明确检查 NetMode。
	// HasAuthority 判断 Actor Role，本地控制的复制 Pawn 在客户端通常为 AutonomousProxy。
	if (!IsAuthoritySide())
	{
		return;
	}

	// 每个 PlayerState 仅初始化一次，避免重复接管时重写基值、补满生命或重置元属性。
	// 当前升级是 Infinite 修饰器，保留在 ASC 上；重写 BaseValue 并不等于删除全部升级。
	// 当前复活复用 Pawn/ASC，这个标志也不提供跨 PlayerState 重建或断线重连的数据持久化。
	ARoguePlayerState* RoguePS = GetPlayerState<ARoguePlayerState>();
	if (RoguePS == nullptr || !RoguePS->IsProgressionReady() || RoguePS->IsBaseAttributesInitialized())
	{
		return;
	}
	URogueAttributeSet* AttributeSet = GetRogueAttributeSet();
	if (AttributeSet == nullptr)
	{
		return;
	}

	// 接管可能先于 BeginPlay；这里确保已解析配置，且只在属性集就绪后提交初始化守卫。
	if (LoadedPlayerBalanceAsset == nullptr)
	{
		LoadedPlayerBalanceAsset = LoadConfiguredOrDefaultPlayerBalanceAsset(PlayerBalanceAsset);
	}
	RoguePS->MarkBaseAttributesInitialized();

	// 数据源和旧系统保持同一份：URoguePlayerBalanceAsset::BaseStats。
	// 加载失败时用结构体默认值兜底 —— 旧系统在这里踩过坑（兜底缺失会导致玩家数值全 0，无法移动）。
	const FRoguePlayerBaseStatConfig DefaultBaseStats;
	const FRoguePlayerBaseStatConfig& BaseStats =
		LoadedPlayerBalanceAsset ? LoadedPlayerBalanceAsset->BaseStats : DefaultBaseStats;

	const URogueUpgradeTreeDefinition* Tree = URogueUpgradeTreeDefinition::GetDefaultDefinition();
	const FRogueProgressionSnapshot& Profile = RoguePS->GetProgressionSnapshot();
	auto PermanentBonus = [Tree, &Profile](ERogueMetaStat Stat)
	{
		return Tree->GetStatBonus(Profile, Stat);
	};
	const bool bArmorUnlocked = Tree->IsArmorUnlocked(Profile);

	// 永久成长只加进本局 GAS 基值，不施加局内叠层 GE。后续同步或复活不重放加法。
	// 武器加成属性的上限（**-1 = 不设上限**，资产里默认全 -1 → 行为与"没有上限"完全一致）。
	//
	// 先提供加成上限，再初始化属性；ASC 对属性当前值的更新会进入 PreAttributeChange。
	// 初始武器加成可以非零，必须先安装上限，避免开局值绕过配置限制。
	AttributeSet->SetWeaponBonusCaps(LoadedPlayerBalanceAsset
		? LoadedPlayerBalanceAsset->WeaponBonusCaps
		: FRogueWeaponBonusCaps());

	// 先写上限，再写当前值。
	// 原因：PreAttributeChange 里 Health 的上界取的是 GetMaxHealth()，
	// 顺序反了会被旧上限钳住（例如 MaxHealth 从 100 提到 200 时 Health 只能到 100）。
	AttributeSet->SetMaxHealth(BaseStats.MaxHealth + PermanentBonus(ERogueMetaStat::MaxHealth));
	AttributeSet->SetMaxArmor(bArmorUnlocked ? BaseStats.MaxArmor + PermanentBonus(ERogueMetaStat::ArmorCapacity) : 0.0f);
	AttributeSet->SetHealthRegenPerSecond(BaseStats.HealthRegenPerSecond + PermanentBonus(ERogueMetaStat::HealthRegen));
	AttributeSet->SetArmorRechargeDelay(BaseStats.ArmorRechargeDelay);
	AttributeSet->SetArmorRechargeRate(BaseStats.ArmorRechargeRate);
	AttributeSet->SetDamageReductionPercent(BaseStats.DamageReductionPercent);
	AttributeSet->SetMoveSpeed(BaseStats.MoveSpeed + PermanentBonus(ERogueMetaStat::MoveSpeed));
	AttributeSet->SetPickupRadius(BaseStats.PickupRadius + PermanentBonus(ERogueMetaStat::PickupRadius));
	AttributeSet->SetExperienceMultiplier(BaseStats.ExperienceMultiplier + PermanentBonus(ERogueMetaStat::ExperienceGain));
	AttributeSet->SetMinDashCooldown(BaseStats.DashMinCooldown);
	AttributeSet->SetDashCooldownDuration(BaseStats.DashCooldownDuration);
	AttributeSet->SetDashDuration(BaseStats.DashDuration);
	AttributeSet->SetDashSpeed(BaseStats.DashSpeed);

	const FRoguePlayerSpawnStateConfig SpawnState = LoadedPlayerBalanceAsset
		? LoadedPlayerBalanceAsset->SpawnState : FRoguePlayerSpawnStateConfig();
	AttributeSet->SetHealth(FMath::Min(AttributeSet->GetMaxHealth(), FMath::Max(1.0f,
		AttributeSet->GetMaxHealth() * FMath::Clamp(SpawnState.HealthRatio, 0.0f, 1.0f))));
	AttributeSet->SetArmor(AttributeSet->GetMaxArmor() * FMath::Clamp(SpawnState.ArmorRatio, 0.0f, 1.0f));

	const FRogueInitialWeaponBonusConfig InitialBonuses = LoadedPlayerBalanceAsset
		? LoadedPlayerBalanceAsset->InitialWeaponBonuses : FRogueInitialWeaponBonusConfig();
	AttributeSet->SetAttackPowerBonus(InitialBonuses.AttackPower + PermanentBonus(ERogueMetaStat::AttackPower));
	AttributeSet->SetAttackSpeedBonus(InitialBonuses.AttackSpeed + PermanentBonus(ERogueMetaStat::AttackSpeed));
	AttributeSet->SetAttackRangeBonus(InitialBonuses.AttackRange);
	AttributeSet->SetProjectileSpeedBonus(InitialBonuses.ProjectileSpeed);
	// 每局只有一张御札作为起始武器（其基础数量由 InitializeWeapons 固定为 1）。
	// 永久解锁只让卡片入池，不直接装备；旧平衡资产的初始数量和专属加成不再跳过这条规则。
	AttributeSet->SetProjectileCountBonus(0.0f);
	AttributeSet->SetScytheCountBonus(0.0f);
	AttributeSet->SetRocketCountBonus(0.0f);
	AttributeSet->SetLaserCountBonus(0.0f);
	AttributeSet->SetHellTowerCountBonus(0.0f);
	AttributeSet->SetLaserRefractionBonus(0.0f);
	AttributeSet->SetMortarCountBonus(0.0f);
	AttributeSet->SetMortarBlastRadiusBonus(0.0f);
	AttributeSet->SetScytheSizeBonus(0.0f);
	AttributeSet->SetScytheLifestealBonus(0.0f);
	AttributeSet->SetProjectileBurstBonus(0.0f);
	AttributeSet->SetRocketBlastRadiusBonus(0.0f);
	AttributeSet->SetHellTowerDamageRampBonus(0.0f);

	RefreshWeaponBonusSnapshotFromAttributes();

	// 成长运行值只在服务器首次初始化时赋值，复活或再次接管不重置钱包和升级进度。
	ExperienceToNextLevel = FMath::Max(1, BaseStats.ExperienceToNextLevel);
	Money = FMath::Max(0, LoadedPlayerBalanceAsset ? LoadedPlayerBalanceAsset->Progression.StartingMoney : 0);

	// 元属性必须是 0，否则第一次结算会带上一个凭空多出来的伤害
	AttributeSet->SetDamage(0.0f);

	UE_LOG(LogTemp, Log,
		TEXT("[GAS] 玩家属性初始化完成：Health=%.1f/%.1f  Armor=%.1f/%.1f  MoveSpeed=%.1f  DashCD=%.2f"),
		AttributeSet->GetHealth(), AttributeSet->GetMaxHealth(),
		AttributeSet->GetArmor(), AttributeSet->GetMaxArmor(),
		AttributeSet->GetMoveSpeed(), AttributeSet->GetDashCooldownDuration());
	
	// 基础值写入后挂载两种常驻周期回复效果；速率捕获及结算条件见 RogueStateEffects。
	EnsureRegenEffects();
}

void ARogueCharacter::EnsureRegenEffects()
{
	URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	// 按效果定义查重，避免重复施加不叠层的常驻回复效果。
	// 当前初始化已有 PlayerState 守卫，同 Pawn 复活不重新挂载这些效果。
	const TSubclassOf<UGameplayEffect> RegenEffectClasses[] =
	{
		URogueHealthRegenEffect::StaticClass(),
		URogueArmorRechargeEffect::StaticClass()
	};

	for (const TSubclassOf<UGameplayEffect>& RegenClass : RegenEffectClasses)
	{
		if (RegenClass == nullptr)
		{
			continue;
		}

		FGameplayEffectQuery AlreadyActiveQuery;
		AlreadyActiveQuery.EffectDefinition = RegenClass;
		if (ASC->GetActiveEffects(AlreadyActiveQuery).Num() > 0)
		{
			continue;
		}

		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		ASC->ApplyGameplayEffectToSelf(RegenClass->GetDefaultObject<UGameplayEffect>(), /*Level=*/1.0f, Context);

		UE_LOG(LogTemp, Log, TEXT("[GAS] 自然回复 GE 已挂载：%s"), *RegenClass->GetName());
	}
}

void ARogueCharacter::ApplyBalanceAsset()
{
	LoadedPlayerBalanceAsset = LoadConfiguredOrDefaultPlayerBalanceAsset(PlayerBalanceAsset);

	// 平衡资产加载失败时使用基础配置默认值；本函数只直接更新非 GAS 配置。
	// GAS 基值由 InitializeGASAttributes 的服务端路径写入。
	const FRoguePlayerBaseStatConfig DefaultBaseStats;
	const FRoguePlayerBaseStatConfig& BaseStats =
		LoadedPlayerBalanceAsset ? LoadedPlayerBalanceAsset->BaseStats : DefaultBaseStats;

	// 非属性管理的字段仍然直接赋值
	if (IsAuthoritySide())
	{
		const ARoguePlayerState* State = GetPlayerState<ARoguePlayerState>();
		if (State == nullptr || !State->IsBaseAttributesInitialized())
		{
			ExperienceToNextLevel = FMath::Max(1, BaseStats.ExperienceToNextLevel);
		}
	}
	// DashMinCooldown 不再在这里赋值：它是 MinDashCooldown 属性的镜像
	//（冲刺冷却的下限现在由属性集在 PreAttributeChange 里执行，见 URogueAttributeSet）。
	JumpZVelocity = BaseStats.JumpZVelocity;
	AirControl = BaseStats.AirControl;
	GravityScale = BaseStats.GravityScale;
	FallGravityMultiplier = FMath::Max(1.0f, BaseStats.FallGravityMultiplier);
	JumpCutMultiplier = FMath::Clamp(BaseStats.JumpCutMultiplier, 0.0f, 1.0f);
	JumpBufferSeconds = FMath::Max(0.0f, BaseStats.JumpBufferSeconds);
	CoyoteTimeSeconds = FMath::Max(0.0f, BaseStats.CoyoteTimeSeconds);
	JumpMaxCountConfig = BaseStats.JumpMaxCount;
	GetCharacterMovement()->JumpZVelocity = JumpZVelocity;
	GetCharacterMovement()->AirControl = AirControl;
	GetCharacterMovement()->GravityScale = GravityScale;
	JumpMaxCount = FMath::Max(0, JumpMaxCountConfig);

	// 属性类字段不再在这里赋值。
	// 它们的唯一真相源是 GAS 属性集，由 InitializeGASAttributes()（PossessedBy 时）
	// 从 URoguePlayerBalanceAsset 与本玩家永久成长快照灌入，之后由局内升级 GE 修改，
	// 最后经 SyncAttributeMirrors() 每帧缓存回运行时字段。
	//
	// GAS 区分基值与持续修饰器，但重复施加效果仍可能重复累加；初始化和效果查重各有守卫。
}

float ARogueCharacter::GetScytheLifestealPercent() const
{
	const URogueAttributeSet* Attributes = GetRogueAttributeSet();
	return Attributes != nullptr
		? FMath::Clamp(Attributes->GetScytheLifestealBonus(), 0.0f, 1.0f)
		: 0.0f;
}

void ARogueCharacter::ApplyScytheLifesteal(float DamageDealt)
{
	if (DamageDealt <= 0.0f || !IsAlive() || !IsAuthoritySide())
	{
		return;
	}

	URogueAttributeSet* Attributes = GetRogueAttributeSet();
	if (Attributes == nullptr)
	{
		return;
	}

	const float Lifesteal = FMath::Clamp(Attributes->GetScytheLifestealBonus(), 0.0f, 1.0f);
	if (Lifesteal <= 0.0f)
	{
		return;
	}

	// 只补到最大生命；SetHealth 的最终钳制仍由属性集负责。
	const float HealAmount = DamageDealt * Lifesteal;
	Attributes->SetHealth(FMath::Min(Attributes->GetHealth() + HealAmount, Attributes->GetMaxHealth()));
}

float ARogueCharacter::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

UAbilitySystemComponent* ARogueCharacter::GetAbilitySystemComponent() const
{
	// 本项目的 ASC 挂在 PlayerState 上（玩家侧），这里转发过去。
	// 实现这个接口之后，"通过 Actor 找 ASC"的所有引擎/项目代码（含 ApplyDamageToTarget）
	// 可在 PlayerState 就绪时取得其 ASC；就绪前仍返回 nullptr，调用方应处理空值。
	const ARoguePlayerState* RoguePlayerState = GetPlayerState<ARoguePlayerState>();
	return RoguePlayerState != nullptr ? RoguePlayerState->GetAbilitySystemComponent() : nullptr;
}
