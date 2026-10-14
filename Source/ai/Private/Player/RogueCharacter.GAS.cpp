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
	const TCHAR* DefaultPlayerBalanceAssetPath = TEXT("/Game/DataTable/AS_RoguePlayerBalanceAsset.AS_RoguePlayerBalanceAsset");

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

	// 只订阅死亡标签。
	// 属性数值不走委托而是每帧轮询（见 SyncAttributeMirrors）：
	//   · 属性有 14 个，逐个绑委托是纯样板；
	//   · 轮询读的是 FGameplayAttributeData 的属性值（直接属性读取，不是查表），成本可以忽略；
	//   · 只有"死亡"必须事件驱动 —— 它要在标签出现的当下被捕获，等下一帧就晚了。
	ASC->RegisterGameplayTagEvent(RogueGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ARogueCharacter::HandleDeathTagChanged);

	// ---- 上限提升时"补满那部分增量"（原本住在 URogueAttributeSet::PostGameplayEffectExecute）----
	//
	// 为什么搬到这里：升级 GE 现在是 **Infinite + Stacking**，而
	// `PostGameplayEffectExecute` **只对 Instant GE 触发** —— 留原处永远不会执行，
	// "升级最大生命/护甲上限时等量回复"会静默消失。
	//
	// 注意这与上面"属性数值走每帧轮询"的取舍并不冲突：轮询负责**镜像**（供 HUD/武器读），
	// 这里要的是"变化发生的那一刻做一次补偿动作"，那是事件驱动的典型场景。
	// 而且它顺带变得更通用：现在**任何来源**的上限变化都会补当前值，不再只认升级。
	ASC->GetGameplayAttributeValueChangeDelegate(URogueAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &ARogueCharacter::HandleMaxHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(URogueAttributeSet::GetMaxArmorAttribute())
		.AddUObject(this, &ARogueCharacter::HandleMaxArmorChanged);
}

void ARogueCharacter::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	// ⚠️ **只在权威侧补当前值**。
	// 属性变化会随复制到达客户端并**同样触发这个委托** —— 在客户端改生命只会与服务端
	// 下一次复制打架（本项目已经踩过同类坑：护甲回复的注释里记着"客户端自己回甲、
	// 护甲条在两个值之间来回跳"）。这与 InitializeGASAttributes 用 IsAuthoritySide() 同理。
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

	// 运行时字段是属性集的**只读缓存**（不是可以调的数值）。
	// 好处：HUD / 武器 / 移动组件这些高频读取点不必每次都走 GAS 的属性查找，
	// 而且调用点一行都不用改。
	// 约定：这些 float 只在这里被写；任何"改数值"都必须改属性集（初始化 / 升级 GE / SetCurrentXxx）。
	// 所以它们在头文件里是**不可编辑**的（曾经标了 EditDefaultsOnly，在蓝图里改毫无效果 —— 很坑）。
	//
	// 注意：这里只同步**真的有人读**的字段。HealthRegenPerSecond / ArmorRechargeDelay /
	// ArmorRechargeRate / DamageReductionPercent 这几项曾经也各留了一份镜像，
	// 但从来没有任何代码读过它们（属性集才是消费方），已删除。
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

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = MoveSpeed;
	}
}

void ARogueCharacter::SetCurrentHealth(float NewHealth)
{
	// 写属性集，再由上面的回调刷新 CurrentHealth —— 单一写入点
	if (URogueAttributeSet* AttrSet = GetRogueAttributeSet())
	{
		AttrSet->SetHealth(FMath::Max(0.0f, NewHealth));
	}
}

void ARogueCharacter::SetCurrentArmor(float NewArmor)
{
	// 与 SetCurrentHealth 同理：写属性集，再由属性变化回调刷新 CurrentArmor 镜像。
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

	// 每种升级有自己的 GE 子类（**属性烤在类上**），并且是 Infinity + Stacking。
	//
	// 为什么必须"一种升级一个类"：引擎的 FindStackableActiveGameplayEffect **只按 Spec.Def 匹配**
	// 可叠加实例 —— 改造前 20 种升级共用一个类，开 Stacking 会让它们共用一个栈、数值错乱。
	//
	// 于是同类型再买一次 = 同一个 GE 实例的**层数 +1**，而引擎把**量级乘以层数**
	//（ComputeStackedModifierMagnitude），所以"买 N 次 = N × 单次数值"，
	// 与改造前 Instant 连加 N 次逐项相等。
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

void ARogueCharacter::InitializeGASAttributes()
{
	// 判据用 IsAuthoritySide()（GetNetMode）而**不是** HasAuthority()：
	// 客户端上本机拥有的角色 Role 也是 ROLE_Authority，HasAuthority() 会返回 true，
	// 于是客户端会自己把基础值写进本地属性集，与复制过来的权威值打架。
	// 这正是本文件头 IsAuthoritySide() 注释里点名禁止的用法。
	if (!IsAuthoritySide())
	{
		return;
	}

	// ⚠️ 重入守卫：这个函数**只能跑一次**。
	//
	// 它用 SetNumericAttributeBase 写的是属性的 BaseValue，而升级（Instant GE）
	// 改的**也是** BaseValue —— 再灌一次会把玩家已获得的属性升级（生命/护甲上限/移速/
	// 拾取范围/回复/经验/冲刺冷却/减伤）**全部覆盖回初始值**，而且不报任何错。
	//
	// 这条路径真实可达：GameMode 的归属自愈（RepairPlayerPawnOwnership，每 0.5 秒检查）
	// 会为了纠正 Pawn 归属而调 Possess()，进而再次触发 PossessedBy() → 本函数；
	// 项目文档记载过归属自愈在**客户端加入的瞬间实测真的会触发**。
	// 守卫放在 PlayerState（而不是 Pawn）上，因为 ASC 与属性集都挂在它上面 ——
	// 它才是"这一局这个玩家的数值"的载体，Pawn 换了不该重灌。详见
	// ARoguePlayerState::bBaseAttributesInitialized 的注释。
	ARoguePlayerState* RoguePS = GetPlayerState<ARoguePlayerState>();
	if (RoguePS == nullptr || RoguePS->IsBaseAttributesInitialized())
	{
		return;
	}
	RoguePS->MarkBaseAttributesInitialized();

	URogueAttributeSet* AttributeSet = GetRogueAttributeSet();
	if (AttributeSet == nullptr)
	{
		return;
	}

	// 数据源和旧系统保持同一份：URoguePlayerBalanceAsset::BaseStats。
	// 加载失败时用结构体默认值兜底 —— 旧系统在这里踩过坑（兜底缺失会导致玩家数值全 0，无法移动）。
	const FRoguePlayerBaseStatConfig DefaultBaseStats;
	const FRoguePlayerBaseStatConfig& BaseStats =
		LoadedPlayerBalanceAsset ? LoadedPlayerBalanceAsset->BaseStats : DefaultBaseStats;

	// 武器加成属性的上限（**-1 = 不设上限**，资产里默认全 -1 → 行为与"没有上限"完全一致）。
	//
	// 必须在下面写属性**之前**灌入：PreAttributeChange 在每次 SetXxx 时就会跑。
	// （初始加成都是 0，所以顺序的实际影响为零，但摆正可以避免以后往这里加非零初值时踩坑。）
	AttributeSet->SetWeaponBonusCaps(LoadedPlayerBalanceAsset
		? LoadedPlayerBalanceAsset->WeaponBonusCaps
		: FRogueWeaponBonusCaps());

	// 先写上限，再写当前值。
	// 原因：PreAttributeChange 里 Health 的上界取的是 GetMaxHealth()，
	// 顺序反了会被旧上限钳住（例如 MaxHealth 从 100 提到 200 时 Health 只能到 100）。
	AttributeSet->SetMaxHealth(BaseStats.MaxHealth);
	AttributeSet->SetMaxArmor(BaseStats.MaxArmor);
	AttributeSet->SetHealthRegenPerSecond(BaseStats.HealthRegenPerSecond);
	AttributeSet->SetArmorRechargeDelay(BaseStats.ArmorRechargeDelay);
	AttributeSet->SetArmorRechargeRate(BaseStats.ArmorRechargeRate);
	AttributeSet->SetDamageReductionPercent(BaseStats.DamageReductionPercent);
	AttributeSet->SetMoveSpeed(BaseStats.MoveSpeed);
	AttributeSet->SetPickupRadius(BaseStats.PickupRadius);
	AttributeSet->SetExperienceMultiplier(BaseStats.ExperienceMultiplier);
	AttributeSet->SetDashCooldownDuration(BaseStats.DashCooldownDuration);
	AttributeSet->SetMinDashCooldown(BaseStats.DashMinCooldown);
	AttributeSet->SetDashDuration(BaseStats.DashDuration);
	AttributeSet->SetDashSpeed(BaseStats.DashSpeed);

	// 当前值开局补满（与旧逻辑 BeginPlay 里 CurrentHealth = MaxHealth / CurrentArmor = MaxArmor 一致）
	AttributeSet->SetHealth(BaseStats.MaxHealth);
	AttributeSet->SetArmor(BaseStats.MaxArmor);

	// 元属性必须是 0，否则第一次结算会带上一个凭空多出来的伤害
	AttributeSet->SetDamage(0.0f);

	UE_LOG(LogTemp, Log,
		TEXT("[GAS] 玩家属性初始化完成：Health=%.1f/%.1f  Armor=%.1f/%.1f  MoveSpeed=%.1f  DashCD=%.2f"),
		AttributeSet->GetHealth(), AttributeSet->GetMaxHealth(),
		AttributeSet->GetArmor(), AttributeSet->GetMaxArmor(),
		AttributeSet->GetMoveSpeed(), AttributeSet->GetDashCooldownDuration());
	
	// 自然回复（生命 / 护甲）由两个常驻的 Periodic GE 承担，逐帧的
	// HandleRecovery / HandleArmorRecharge 已删除。挂载放在属性灌完之后：
	// 量级是 AttributeBased（每次结算重新读属性），顺序其实不影响正确性，
	// 但这样能保证"第一次结算时速率属性已经就位"。
	EnsureRegenEffects();
}

void ARogueCharacter::EnsureRegenEffects()
{
	URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	// ⚠️ 这个检查是**必须的**，不是防御性编程：
	//    两个回复 GE 都是 Infinite，重复施加会变成两份独立的 ActiveGameplayEffect，
	//    回复速度直接翻倍 —— 而且没有任何报错。
	//    角色重生时 Pawn 换了、PlayerState / ASC 保留，InitializeGASAttributes 会再跑一次。
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

	// ⚠️ 兜底：属性集字段现在是"纯累加器"（默认值全 0），BaseGroup 是绝对值的唯一来源。
	//    因此即使平衡资产加载失败，也必须用一份默认配置把基础值灌进 BaseGroup，
	//    否则玩家数值会全部为 0（无法移动、无生命值）。
	const FRoguePlayerBaseStatConfig DefaultBaseStats;
	const FRoguePlayerBaseStatConfig& BaseStats =
		LoadedPlayerBalanceAsset ? LoadedPlayerBalanceAsset->BaseStats : DefaultBaseStats;

	// 非属性管理的字段仍然直接赋值
	ExperienceToNextLevel = BaseStats.ExperienceToNextLevel;
	// DashMinCooldown 不再在这里赋值：它是 MinDashCooldown 属性的镜像
	//（冲刺冷却的下限现在由属性集在 PreAttributeChange 里执行，见 URogueAttributeSet）。
	JumpZVelocity = BaseStats.JumpZVelocity;
	AirControl = BaseStats.AirControl;
	GravityScale = BaseStats.GravityScale;
	JumpMaxCountConfig = BaseStats.JumpMaxCount;
	GetCharacterMovement()->JumpZVelocity = JumpZVelocity;
	GetCharacterMovement()->AirControl = AirControl;
	GetCharacterMovement()->GravityScale = GravityScale;
	JumpMaxCount = FMath::Max(0, JumpMaxCountConfig);

	// 属性类字段不再在这里赋值。
	// 它们的唯一真相源是 GAS 属性集，由 InitializeGASAttributes()（PossessedBy 时）
	// 从同一份 URoguePlayerBalanceAsset::BaseStats 灌入，之后由升级 GE 修改，
	// 最后经 SyncAttributeMirrors() 每帧缓存回运行时字段。
	//
	// 旧的自研属性系统（FRoguePlayerAttributeSystem / BaseGroup / UpgradeGroup）已整体移除，
	// 连同它那套"累加器复位再补 Base"的结算方式 —— 那套机制曾经导致属性翻倍的 P0 级 bug，
	// 而 GAS 的 BaseValue + 修饰器模型在结构上就不会出现重复计数。
}

float ARogueCharacter::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

UAbilitySystemComponent* ARogueCharacter::GetAbilitySystemComponent() const
{
	// 本项目的 ASC 挂在 PlayerState 上（玩家侧），这里转发过去。
	// 实现这个接口之后，"通过 Actor 找 ASC"的所有引擎/项目代码（含 ApplyDamageToTarget）
	// 都能正确拿到玩家的 ASC —— 这正是"怪物打玩家不掉血"的根因所在，详见头文件。
	const ARoguePlayerState* RoguePlayerState = GetPlayerState<ARoguePlayerState>();
	return RoguePlayerState != nullptr ? RoguePlayerState->GetAbilitySystemComponent() : nullptr;
}