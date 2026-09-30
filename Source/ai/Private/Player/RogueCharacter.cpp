#include "Player/RogueCharacter.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Core/RogueSelfTest.h"

#include "Combat/RogueWeaponBase.h"
#include "Combat/RogueWeapon_Projectile.h"
#include "Combat/RogueWeapon_Scythe.h"
#include "Combat/RogueWeapon_Rocket.h"
#include "Combat/RogueWeapon_Laser.h"
#include "Combat/RogueWeapon_HellTower.h"
#include "Combat/RogueWeapon_Mortar.h"
#include "Combat/RogueMortarProjectile.h"
#include "Combat/RogueRocketProjectile.h"
#include "Enemies/RogueEnemy.h"
#include "Core/RogueGameMode.h"
#include "Core/RogueGameState.h"
#include "Player/RoguePlayerBalanceAsset.h"
#include "Player/RoguePlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/Abilities/RogueWeaponAbility.h"
#include "AbilitySystem/RogueStateEffects.h"
#include "AbilitySystem/Abilities/RogueEnemyAttackAbility.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "AbilitySystem/RogueAttributeUpgradeEffect.h"
#include "Core/RogueGameplayTags.h"
#include "World/RogueShopTerminal.h"
#include "UI/RogueHUD.h"
#include "Combat/RogueImpactEffect.h"
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
	/** Enhanced Input 资产的默认目录（见 Docs/Enhanced-Input-Migration.md）。
	 *  约定：映射上下文直接放在 `/Game/Input/` 下，10 个输入动作统一放在 `/Game/Input/IA/` 下。 */
	const TCHAR* DefaultInputContextFolder = TEXT("/Game/Input");
	const TCHAR* DefaultInputActionFolder = TEXT("/Game/Input/IA");

	/** 解析一个 Enhanced Input 资产：优先用角色上配置的引用（可在 BP_RogueCharacter 里指定）；
	 *  留空则按 `<DefaultFolder>/<名字>.<名字>` 的约定路径加载。
	 *
	 *  找不到返回 nullptr —— 调用方据此**整体回退**到 legacy 输入。
	 *  换句话说：**没有这些资产，项目照样能玩**，只是走旧输入路径。 */
	template<typename TAsset>
	TAsset* ResolveInputAsset(const TSoftObjectPtr<TAsset>& Configured, const TCHAR* DefaultFolder, const TCHAR* DefaultAssetName)
	{
		if (!Configured.IsNull())
		{
			return Configured.LoadSynchronous();
		}

		const FString AssetPath = FString::Printf(TEXT("%s/%s.%s"),
			DefaultFolder, DefaultAssetName, DefaultAssetName);
		return LoadObject<TAsset>(nullptr, *AssetPath);
	}
}

namespace
{
	const FName MortarWeaponRowName(TEXT("Mortar"));
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

ARogueCharacter::ARogueCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 玩家角色必须复制（与敌人同理，见 ARogueEnemy 构造函数里的长注释）：
	// ACharacter / APawn 都**不会**自动打开 bReplicates，AActor 默认是 false。
	// 当前之所以能用，是因为 BP_RogueCharacter 上勾了 Replicates —— 那是隐性依赖，
	// 换个蓝图或新建子类就会静默失效（表现：客户端收不到任何 pawn，完全无法操作）。
	// 在 C++ 里显式声明，让"必须复制"这件事跟着代码走。
	// （蓝图默认值在构造之后应用：若某个 BP 手动取消勾选，仍会覆盖这里，需要在 BP 里勾回来。）
	bReplicates = true;
	SetReplicatingMovement(true);

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	bUseControllerRotationYaw = false;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 900.0f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 5.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	Visuals = CreateDefaultSubobject<URogueCharacterVisualComponent>(TEXT("Visuals"));
	Visuals->SetupAttachment(RootComponent);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	GetCharacterMovement()->JumpZVelocity = JumpZVelocity;
	GetCharacterMovement()->AirControl = AirControl;
	GetCharacterMovement()->GravityScale = GravityScale;
	JumpMaxCount = JumpMaxCountConfig;

	DefaultMortarWeaponClass = ARogueWeapon_Mortar::StaticClass();
	DefaultMortarProjectileClass = ARogueMortarProjectile::StaticClass();

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ARogueCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyBalanceAsset();

	// 血量/护甲的开局补满，正式路径是 PossessedBy → InitializeGASAttributes()（真相源是属性集）。
	// 这里只给"GAS 尚未就绪"的情况兜底（例如 Pawn 不是由 GameMode 生成、拿不到 PlayerState），
	// 避免出现"属性集是 0、HUD 却显示满血"这种半迁移状态 ——
	// 那种状态下玩家会被一次普通伤害直接打死。
	if (GetRogueAttributeSet() == nullptr)
	{
		CurrentHealth = MaxHealth;
		CurrentArmor = MaxArmor;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->SetControlRotation(FRotator(-55.0f, 45.0f, 0.0f));
		PlayerController->bShowMouseCursor = false;
		PlayerController->bEnableClickEvents = false;
		PlayerController->bEnableMouseOverEvents = false;
	}

	InitializeWeapons();
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

	const FGameplayTag PayloadTag = RogueGameplayTags::GetUpgradeAttributeTag(UpgradeType);
	if (!PayloadTag.IsValid())
	{
		// 没有载荷 = 这个升级类型既不是属性类也不是武器类（枚举加了但映射忘了加）。
		// 静默忽略会让"卡牌能抽到但没效果"，所以打一条 Warning。
		UE_LOG(LogTemp, Warning, TEXT("[Rogue] 升级类型 %d 没有载荷 Tag，本次升级被忽略（检查 RogueGameplayTags::GetUpgradeAttributeTag）"),
			static_cast<int32>(UpgradeType));
		return false;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(URogueAttributeUpgradeEffect::StaticClass(), 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	// 载荷："这次改哪个属性"。用动态资源 Tag 传 —— SetByCaller 只能传 float，传不了"是哪个属性"。
	SpecHandle.Data->AddDynamicAssetTag(PayloadTag);
	// 数值。冲刺冷却是负向升级，调用方传负数（符号语义由数据决定，不写死在 Execution 里）。
	SpecHandle.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Magnitude, Magnitude);

	const bool bApplied = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()).WasSuccessfullyApplied();

	if (bApplied)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[GAS] 升级已施加：类型=%d 数值=%.2f 载荷=%s"),
			static_cast<int32>(UpgradeType), Magnitude, *PayloadTag.ToString());
	}

	return bApplied;
}

void ARogueCharacter::HandleDeathTagChanged(FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0 || bDead || bPendingDeath)
	{
		return;
	}

	bPendingDeath = true;

	// 与敌人同样的问题：这个回调正在"施加授予 State.Dead 的 GE"过程中执行，
	// 而死亡善后会去移除/清理效果，在它自己的标签回调里做属于重入。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ARogueCharacter::ProcessDeath);
	}
}

void ARogueCharacter::ProcessDeath()
{
	bPendingDeath = false;

	if (bDead)
	{
		return;
	}

	Die();
}

void ARogueCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// ── 服务端侧 ──
	// 两条顺序不能颠倒：必须先把 ASC 的 Owner/Avatar 设好，
	// 属性集才会被 ASC 登记进 SpawnedAttributes；
	// 之后再写基础值，SetXxx() 内部才能拿到 AbilitySystemComponent。
	if (ARoguePlayerState* RoguePS = GetPlayerState<ARoguePlayerState>())
	{
		RoguePS->RefreshAbilityActorInfo(this);
		InitializeGASAttributes();
		BindAbilitySystemEvents();
	}
}

void ARogueCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// ── 客户端侧 ──
	// PlayerState（连同它上面的 ASC）刚刚复制到位，把 Avatar 指到这个 Pawn。
	// 这里**不**初始化属性：基础值是服务端权威数据，等它同步过来。
	// 少了这一步，客户端会出现"技能按不出来 / 属性全是 0"，而单机测试完全正常。
	if (ARoguePlayerState* RoguePS = GetPlayerState<ARoguePlayerState>())
	{
		RoguePS->RefreshAbilityActorInfo(this);
		BindAbilitySystemEvents();
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
	// SetNumericAttributeBase 会改属性的 BaseValue —— 这是服务端权威操作
	if (!HasAuthority())
	{
		return;
	}

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

void ARogueCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 属性镜像同步放在最前面，且**在 bDead 早退之前** ——
	// 死亡后 HUD 仍要能读到最终血量（归零），所以这里不能因为死亡就跳过。
	SyncAttributeMirrors();

	// ── 倒地等待 → 复活：**必须在 `if (bDead) return;` 之前** ──
	//
	// ⚠️⚠️ 这里踩过一个很贵的坑，务必不要再把这段挪到早退后面：
	//   复活轮询最初写在 `if (bDead) return;` **之后**，而 bDead 为真时函数在到达它之前就返回了，
	//   于是轮询代码**永远执行不到**。实测日志表现完全吻合：
	//     · "进入倒地等待：10.0 秒后回出生点复活" → 有
	//     · "已复活"                              → 一次都没有
	//   观感就是"倒下之后再也不会起来，整局像卡住了"。
	//   同一局里定时器那条路（SetTimer → Revive）也没触发，所以两条路都不通；
	//   现在"复活到点"由三处独立驱动（本函数、GameMode 的 TickReviveQueue()、角色身上的定时器），
	//   三处都走 TryReviveIfExpired() —— 判据是**倒地等待 GE 的剩余时长**，不依赖任何定时器状态。
	if (bDead && IsAuthoritySide())
	{
		TryReviveIfExpired();
	}

	// 开发自检（默认关，见 Core/RogueSelfTest.h）：**每台机器**打印一次本机角色读到的武器加成。
	// 客户端那一行能打出来，就证明"加成属性确实复制到了客户端" —— 这是"升级只改属性"的前提。
	//
	// ⚠️ 位置：必须放在 `if (bDead) return;` **之前**。
	// 实测踩过：客户端加载完地图时，服务端可能已经把玩家判定倒地（复制过来 bDead=1），
	// 于是这段自检永远走不到 —— 与上面复活轮询踩的是同一个坑（死亡早退把诊断也一起挡掉了）。
	if (IsLocallyControlled() && RogueSelfTest::IsWeaponUpgradeChainEnabled() && WeaponBonusLogCount < 8)
	{
		// 注意这里**不是"只打一次"**：客户端第一帧通常还没收到复制过来的属性值，
		// 只打一次会看到全 0 而误判为"没复制"。改成"值一变就打一条"（最多 6 条），
		// 日志里就能看出 0 → 真值 的变化过程，从而区分"没复制"和"还没到"。
		if (const ARoguePlayerState* RoguePS = GetPlayerState<ARoguePlayerState>())
		{
			const URogueAttributeSet* WeaponSet = RoguePS->GetAttributeSet();
			if (WeaponSet != nullptr)
			{
				// 一份完整快照：
				//   · 生命/护甲/移速/拾取半径 —— **老属性**（已知能复制到客户端，作为对照组）
				//   · 5 个武器加成 —— 本次新增的属性（疑似不复制）
				// 两者在同一套属性集、同样的复制声明下表现不同，才能把问题定位到"属性"这一层。
				const FString Snapshot = FString::Printf(
					TEXT("生命=%.1f/%.1f 护甲=%.1f/%.1f | 属性加成: 伤害=%.2f 攻速=%.2f 范围=%.2f 弹速=%.2f 数量=%.2f | 快照(武器实际读): 伤害=%.2f 攻速=%.2f 范围=%.2f 弹速=%.2f 数量=%.2f"),
					WeaponSet->GetHealth(), WeaponSet->GetMaxHealth(),
					WeaponSet->GetArmor(), WeaponSet->GetMaxArmor(),
					WeaponSet->GetAttackPowerBonus(), WeaponSet->GetAttackSpeedBonus(),
					WeaponSet->GetAttackRangeBonus(), WeaponSet->GetProjectileSpeedBonus(),
					WeaponSet->GetProjectileCountBonus(),
					GetWeaponBonusSnapshot().AttackPower, GetWeaponBonusSnapshot().AttackSpeed,
					GetWeaponBonusSnapshot().AttackRange, GetWeaponBonusSnapshot().ProjectileSpeed,
					GetWeaponBonusSnapshot().ProjectileCount);

				if (WeaponBonusLogCount == 0 || Snapshot != LastLoggedSelfTestSignature)
				{
					++WeaponBonusLogCount;
					LastLoggedSelfTestSignature = Snapshot;
					// 带上玩家名：服务端和客户端打的是**各自机器上的同一个玩家**，
					// 只有名字对上才能两行对比（之前正是拿主机 vs 客户端自己，结论错了）。
					const FString SelfTestPlayerName = RoguePS->GetPlayerName();
					UE_LOG(LogTemp, Log, TEXT("[SelfTest] 本机(%s) 玩家=%s #%d %s（世界时间 %.2f）"),
						HasAuthority() ? TEXT("服务端") : TEXT("客户端"),
						*SelfTestPlayerName,
						WeaponBonusLogCount, *Snapshot,
						GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f);

					// 再确认"客户端读到的那套集"与 PlayerState 上登记的是同一个对象
					//（排除"引擎在客户端另建了一个集实例"这种可能）
					if (const URogueAbilitySystemComponent* ASC = RoguePS->GetRogueAbilitySystemComponent())
					{
						const TArray<UAttributeSet*>& Sets = ASC->GetSpawnedAttributes();
						const bool bSameObject = Sets.Num() > 0 && Sets[0] == RoguePS->GetAttributeSet();
						UE_LOG(LogTemp, Log, TEXT("[SelfTest]   ASC 登记 %d 套集，[0] 与 PlayerState->GetAttributeSet() 同一对象=%s"),
							Sets.Num(), bSameObject ? TEXT("是") : TEXT("否"));
					}

					// 本机武器算出来的**生效值**（这是最终要给玩家看的东西）。
					// 客户端武器是各自 BeginPlay 里本地生成的，所以这一行就是"客户端算得对不对"的答案：
					// 与服务端同玩家的同名武器数值一致 → 整条链（属性 → 快照 → 武器系数）两端等价。
					for (const ARogueWeaponBase* Weapon : Weapons)
					{
						if (!IsValid(Weapon))
						{
							continue;
						}
						UE_LOG(LogTemp, Log, TEXT("[SelfTest]   武器 %-26s 数量=%d 伤害=%.2f 冷却=%.2f 范围=%.1f"),
							*Weapon->GetClass()->GetName(),
							Weapon->GetEffectiveCount(), Weapon->GetEffectiveDamage(),
							Weapon->GetEffectiveCooldown(), Weapon->GetEffectiveRange());
					}

					// 关键诊断：**BaseValue 与 CurrentValue 分开看**。
					// 访问器（GetXxx）读的是 CurrentValue；若 BaseValue 到了而 CurrentValue 没更新，
					// 说明"值复制到了、但客户端的聚合值没重算"（另一类问题），而不是"属性没复制"。
					// 对照一个老属性（护甲）。
					UE_LOG(LogTemp, Log, TEXT("[SelfTest]   原始结构体：伤害加成(Base=%.2f Cur=%.2f) 护甲(Base=%.2f Cur=%.2f)"),
						WeaponSet->AttackPowerBonus.GetBaseValue(), WeaponSet->AttackPowerBonus.GetCurrentValue(),
						WeaponSet->Armor.GetBaseValue(), WeaponSet->Armor.GetCurrentValue());

					// 再查"这两个属性在本机的复制注册状态"。
					// FProperty::RepIndex 是 UHT/引擎为复制属性分配的索引：
					// 若是 INDEX_NONE(0xFFFF) 说明**根本没注册进复制系统**（而不是"注册了但没发"）。
					// 两边打印同一对值，就能区分注册问题与发送/接收问题。
					{
						const FProperty* ArmorProp = FindFProperty<FProperty>(URogueAttributeSet::StaticClass(), GET_MEMBER_NAME_CHECKED(URogueAttributeSet, Armor));
						const FProperty* BonusProp = FindFProperty<FProperty>(URogueAttributeSet::StaticClass(), GET_MEMBER_NAME_CHECKED(URogueAttributeSet, AttackPowerBonus));
						UE_LOG(LogTemp, Log, TEXT("[SelfTest]   复制注册：Armor 有CPF_Net=%d RepIndex=%d ；AttackPowerBonus 有CPF_Net=%d RepIndex=%d （%d 表示未注册）"),
							(ArmorProp != nullptr && ArmorProp->HasAnyPropertyFlags(CPF_Net)) ? 1 : 0,
							ArmorProp != nullptr ? static_cast<int32>(ArmorProp->RepIndex) : -2,
							(BonusProp != nullptr && BonusProp->HasAnyPropertyFlags(CPF_Net)) ? 1 : 0,
							BonusProp != nullptr ? static_cast<int32>(BonusProp->RepIndex) : -2,
							static_cast<int32>(INDEX_NONE));
					}
				}
			}
		}
	}

	// 武器能力的授予/激活：**必须在 `if (bDead) return;` 之前**。
	// 理由与上面那段复活轮询一样：倒地的玩家也要把能力准备好，
	// 否则"加载时就已经倒地"的客户端会一直激活不了（本项目在诊断代码上踩过同一个坑）。
	EnsureWeaponAbilities();
	TickWeaponFireReport(DeltaSeconds);

	if (bDead)
	{
		return;
	}

	// 复活无敌的到期检查（服务端自己算，不需要复制字段：伤害只在服务端结算）。

	HandleDash(DeltaSeconds);
	UpdateVisualPresentation(DeltaSeconds);
	// 生命 / 护甲回复**不在这里了**：已改为两个常驻的 Periodic GE（见 EnsureRegenEffects）。
	// 这一条同时消掉了原来"倒下的角色不再 Tick，于是回复逻辑行为异常"的隐患 ——
	// 现在回复由 GAS 按周期推进，与角色是否 Tick 无关。
	TickWeapons(DeltaSeconds);
}

void ARogueCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input（UE5 标准输入）。资产在 `/Game/Input/`，清单见 Docs/Enhanced-Input-Migration.md。
	//
	// 历史：迁移期这里曾有一条支路 —— "任一资产缺失就整体回退到 legacy 的 BindAxis / BindAction"，
	// 配套 DefaultInput.ini 里的 8 条 ActionMappings + 11 条 AxisMappings。
	// 资产就位后那条支路**永远走不到了**，已随那些旧映射一起删除。
	//
	// 为什么删而不是留着当保险：留着的代价不是那几十行，而是
	// "看起来还能回退、实际上从没被走到过" —— 这种代码比没有更容易误导后来的人。
	SetupEnhancedInput(PlayerInputComponent);
}

void ARogueCharacter::SetupEnhancedInput(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInput == nullptr)
	{
		// 正常情况下不会发生：DefaultInput.ini 已把 DefaultInputComponentClass 设为 EnhancedInputComponent。
		UE_LOG(LogTemp, Error, TEXT("[Rogue][Input] 输入组件不是 UEnhancedInputComponent —— 本机将没有任何输入"));
		return;
	}

	// 局部名统一带 Asset 后缀：成员名本身就叫 MoveAction 等，
	// 同名局部变量会遮蔽成员（本工程开了 -WarningsAsErrors，C4458 直接报错）。
	UInputMappingContext* MappingContext = ResolveInputAsset(DefaultMappingContext, DefaultInputContextFolder, TEXT("IMC_Default"));
	UInputAction* MoveAsset = ResolveInputAsset(this->MoveAction, DefaultInputActionFolder, TEXT("IA_Move"));
	UInputAction* LookAsset = ResolveInputAsset(this->LookAction, DefaultInputActionFolder, TEXT("IA_Look"));
	UInputAction* ZoomAsset = ResolveInputAsset(this->ZoomAction, DefaultInputActionFolder, TEXT("IA_Zoom"));
	UInputAction* DashAsset = ResolveInputAsset(this->DashAction, DefaultInputActionFolder, TEXT("IA_Dash"));
	UInputAction* JumpAsset = ResolveInputAsset(this->JumpAction, DefaultInputActionFolder, TEXT("IA_Jump"));
	UInputAction* InteractAsset = ResolveInputAsset(this->InteractAction, DefaultInputActionFolder, TEXT("IA_Interact"));
	UInputAction* PauseMenuAsset = ResolveInputAsset(this->TogglePauseMenuAction, DefaultInputActionFolder, TEXT("IA_TogglePauseMenu"));
	UInputAction* Upgrade1Asset = ResolveInputAsset(this->ChooseUpgrade1Action, DefaultInputActionFolder, TEXT("IA_ChooseUpgrade1"));
	UInputAction* Upgrade2Asset = ResolveInputAsset(this->ChooseUpgrade2Action, DefaultInputActionFolder, TEXT("IA_ChooseUpgrade2"));
	UInputAction* Upgrade3Asset = ResolveInputAsset(this->ChooseUpgrade3Action, DefaultInputActionFolder, TEXT("IA_ChooseUpgrade3"));

	// ⚠️ 缺任何一个就**整体**回退，不做"能用几个算几个"的部分绑定：
	//    半套绑定（例如移动正常但冲刺没反应）比全 legacy 难排查得多，
	//    而且那种状态下"输入坏了"和"技能坏了"从表现上分不出来。
	TArray<FString> MissingAssets;
	if (MappingContext == nullptr) { MissingAssets.Add(TEXT("IMC_Default")); }
	if (MoveAsset == nullptr)      { MissingAssets.Add(TEXT("IA_Move")); }
	if (LookAsset == nullptr)      { MissingAssets.Add(TEXT("IA_Look")); }
	if (ZoomAsset == nullptr)      { MissingAssets.Add(TEXT("IA_Zoom")); }
	if (DashAsset == nullptr)      { MissingAssets.Add(TEXT("IA_Dash")); }
	if (JumpAsset == nullptr)      { MissingAssets.Add(TEXT("IA_Jump")); }
	if (InteractAsset == nullptr)  { MissingAssets.Add(TEXT("IA_Interact")); }
	if (PauseMenuAsset == nullptr) { MissingAssets.Add(TEXT("IA_TogglePauseMenu")); }
	if (Upgrade1Asset == nullptr)  { MissingAssets.Add(TEXT("IA_ChooseUpgrade1")); }
	if (Upgrade2Asset == nullptr)  { MissingAssets.Add(TEXT("IA_ChooseUpgrade2")); }
	if (Upgrade3Asset == nullptr)  { MissingAssets.Add(TEXT("IA_ChooseUpgrade3")); }

	if (MissingAssets.Num() > 0)
	{
		// 用 Error 而不是 Warning：资产齐全是**前提**，缺了就是本机完全没有输入 ——
		// 这不是"降级运行"，而是坏掉了，不该被当成一条普通提示划过去。
		UE_LOG(LogTemp, Error,
			TEXT("[Rogue][Input] Enhanced Input 资产缺失 %d 个（%s）—— 本机将没有任何输入。")
			TEXT("资产清单见 Docs/Enhanced-Input-Migration.md"),
			MissingAssets.Num(), *FString::Join(MissingAssets, TEXT("、")));
		return;
	}

	// 映射上下文按**本机**添加：联机下每台客户端各自加自己那一份
	// （AddMappingContext 是本地玩家子系统的东西，不参与复制）。
	if (const APlayerController* PC = GetController<APlayerController>())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			InputSubsystem->AddMappingContext(MappingContext, /*Priority=*/0);
		}
	}

	// 全部走"薄适配层"：这些处理函数只是转调 legacy 的处理函数，
	// 所以"按 WASD 怎么移动""冲刺怎么判定"这些逻辑仍然只有一份实现。
	EnhancedInput->BindAction(MoveAsset,      ETriggerEvent::Triggered, this, &ARogueCharacter::HandleMoveInput);
	EnhancedInput->BindAction(LookAsset,      ETriggerEvent::Triggered, this, &ARogueCharacter::HandleLookInput);
	EnhancedInput->BindAction(ZoomAsset,      ETriggerEvent::Triggered, this, &ARogueCharacter::HandleZoomInput);
	EnhancedInput->BindAction(DashAsset,      ETriggerEvent::Started,   this, &ARogueCharacter::HandleDashInput);
	// 跳跃要成对绑定：按下起跳、松开截断（对应 legacy 的 IE_Pressed / IE_Released）
	EnhancedInput->BindAction(JumpAsset,      ETriggerEvent::Started,   this, &ARogueCharacter::HandleJumpStarted);
	EnhancedInput->BindAction(JumpAsset,      ETriggerEvent::Completed, this, &ARogueCharacter::HandleJumpStopped);
	EnhancedInput->BindAction(InteractAsset,  ETriggerEvent::Started,   this, &ARogueCharacter::Interact);
	EnhancedInput->BindAction(PauseMenuAsset, ETriggerEvent::Started,   this, &ARogueCharacter::TogglePauseMenu);
	EnhancedInput->BindAction(Upgrade1Asset,  ETriggerEvent::Started,   this, &ARogueCharacter::ChooseUpgradeOne);
	EnhancedInput->BindAction(Upgrade2Asset,  ETriggerEvent::Started,   this, &ARogueCharacter::ChooseUpgradeTwo);
	EnhancedInput->BindAction(Upgrade3Asset,  ETriggerEvent::Started,   this, &ARogueCharacter::ChooseUpgradeThree);

	// 日志里带上**映射条数**：这是"资产是否真的配了按键"的唯一直接证据。
	// 光看"资产加载成功"是不够的 —— 一个空的 IMC 也能加载成功，
	// 表现却是"所有按键都没反应"，且不会报任何错。
	// 期望条数：Move 8（WASD + 四个方向键）+ Look 1 + Zoom 1 + Dash 1 + Jump 1
	//          + Interact 1 + PauseMenu 1 + ChooseUpgrade 3 = 17。
	UE_LOG(LogTemp, Log, TEXT("[Rogue][Input] Enhanced Input 已启用：IMC=%s（%d 条按键映射），动作 10/10 就绪"),
		*MappingContext->GetName(), MappingContext->GetMappings().Num());
}

void ARogueCharacter::HandleMoveInput(const FInputActionValue& Value)
{
	// IA_Move 是 Axis2D：X = 右、Y = 前。
	// 注意 W 能给出 +Y 靠的是资产上的 Swizzle 修饰器，不是代码 —— 见迁移文档 §3.3。
	const FVector2D Axis = Value.Get<FVector2D>();
	MoveForward(Axis.Y);
	MoveRight(Axis.X);
}

void ARogueCharacter::HandleLookInput(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	TurnCamera(Axis.X);
	PitchCamera(Axis.Y);
}

void ARogueCharacter::HandleZoomInput(const FInputActionValue& Value)
{
	ZoomCamera(Value.Get<float>());
}

void ARogueCharacter::HandleDashInput()
{
	// 走 GAS 的**原生输入路径**：按 InputID 找技能并激活。
	// 冲刺能力在授予时带了 RogueAbilityInputID::Dash（见 URogueAbilitySystemComponent::GrantDefaultAbilities）。
	// "能不能冲刺"仍完全由技能自己判定（ActivationBlockedTags + 冷却 GE）——
	// 输入层不做任何玩法判断，这也是这段代码能这么短的原因。
	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.InputID == RogueAbilityInputID::Dash)
			{
				ASC->AbilityLocalInputPressed(RogueAbilityInputID::Dash);
				return;
			}
		}
	}

	// 兜底：技能列表是**复制**过来的，客户端刚连上时可能还没到达。
	// 这时退回按标签激活，免得玩家要再按一次。
	// （Dash() 自己会再判一次死亡 / 暂停，所以这里不必重复判断。）
	Dash();
}

void ARogueCharacter::HandleJumpStarted()
{
	StartJump();
}

void ARogueCharacter::HandleJumpStopped()
{
	StopJump();
}

void ARogueCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 成长数据只发给拥有者：击杀奖励、升级抽卡、商店扣费都在服务端结算，
	// 客户端只负责显示。COND_OwnerOnly 既够用又不把钱包广播给别人。
	DOREPLIFETIME_CONDITION(ARogueCharacter, CurrentExperience, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARogueCharacter, PlayerLevel, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARogueCharacter, Money, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARogueCharacter, ExperienceToNextLevel, COND_OwnerOnly);

	// 倒地状态发给**所有人**（不是 OwnerOnly）：队友要能看见你躺在地上，
	// 而本机的 HUD / 输入模式 / 死亡界面全都读它。漏了它就等于客户端永远不知道自己死了
	// （实测症状：客户端死后依然能操作、没有倒计时、死亡界面不弹）。
	DOREPLIFETIME(ARogueCharacter, bDead);

	// 武器加成快照（复制给所有端）：真相源是属性集，这里是跨端同步管道
	DOREPLIFETIME(ARogueCharacter, WeaponBonusSnapshot);
}

void ARogueCharacter::AddExperience(int32 ExperienceAmount)
{
	if (bDead)
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
	// 再转一手，纯粹多一层跳转，已合并进来）。分三类：
	//   · 属性类 → GAS 的 GameplayEffect（URogueAttributeUpgradeEffect + ExecutionCalculation）
	//   · 武器类 → NetMulticast，让服务端与所有客户端各自更新本地武器副本
	//   · 联动效果 → 护甲上限提升后立刻开始回复、冲刺冷却缩短后不必等旧冷却

	// 记录升级前的上限，用于护甲上限提升后的"立即开始回复"联动
	const float PreviousMaxArmor = MaxArmor;

	switch (Upgrade.Type)
	{
	// ---- 属性类升级：统一通过 GAS 的 GameplayEffect 施加 ----
	// 一份 URogueAttributeUpgradeEffect 承载全部 8 种属性升级：
	// 枚举 → 载荷 Tag（Upgrade.Attribute.*）→ ExecutionCalculation 决定改哪个属性，
	// 数值走 SetByCaller(Data.Magnitude)。
	//
	// 迁移前的做法是把 Magnitude 塞进自研的 UpgradeGroup 再整表重算，
	// 那条路径已经整体删除 —— 属性现在只有一个真相源（GAS 属性集），
	// 而且升级结果会随属性复制自动同步到客户端。
	//
	// 注意"等量回血/回甲"这条联动**没有丢**：它搬到了
	// URogueAttributeSet::PostGameplayEffectExecute（MaxHealth/MaxArmor 变化时按增量补当前值），
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
	// 历史：这里原来是 `Multicast_ApplyWeaponUpgrade` —— 让服务端与所有客户端各自把
	// 同一个数值加到本地武器副本的配置结构体上（"各端重放一遍加法"）。
	// 那套做法有三个代价：数值出现第二个真相源（武器 Actor 上的 Config）、
	// 顺序/丢包会让各端数值漂移、加一类新升级要同时改分发+多播+每把武器的应用函数。
	//
	// 现在：升级 GE 只改玩家 ASC 上 URogueAttributeSet 里的加成属性，
	// 武器开火时读「数据表基础值 + 属性加成」（ARogueWeaponBase::GetEffective*）。
	// 结算只发生在服务端一次，客户端通过属性复制天然一致。
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
	// 其余联动（回血/回甲、冲刺冷却收缩）已分别由属性集与 ClearDashCooldown 处理。
	if (Upgrade.Type == ERogueUpgradeType::ArmorCapacity && MaxArmor > PreviousMaxArmor)
	{
		ClearRecentlyDamagedState();
	}
}

void ARogueCharacter::BroadcastWeaponCosmetic(const ARogueWeaponBase* Weapon, const FRogueWeaponCosmeticFire& Fire)
{
	if (!HasAuthority() || Weapon == nullptr)
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
	if (!HasAuthority() || PendingWeaponCosmetics.Num() == 0)
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
	// 两类机器跳过（它们各自已经有"真实/预测"的那一份了）：
	//   · 服务端：权威那一发就是它自己生成的；
	//   · 本人：自己那一发是本地预测。
	if (HasAuthority() || IsLocallyControlled())
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

float ARogueCharacter::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

float ARogueCharacter::GetExperiencePercent() const
{
	return ExperienceToNextLevel > 0 ? static_cast<float>(CurrentExperience) / static_cast<float>(ExperienceToNextLevel) : 0.0f;
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

// ---- 角色移动 / 摄像机 / 冲刺 / 护甲 / 恢复 ----

void ARogueCharacter::MoveForward(float Value)
{
	ForwardInputValue = Value;

	if (bDead || bDashActive || FMath::IsNearlyZero(Value))
	{
		return;
	}

	if (GetWorld()->IsPaused())
	{
		return;
	}

	FVector MoveDirection = Camera != nullptr ? Camera->GetForwardVector() : FVector::ForwardVector;
	MoveDirection.Z = 0.0f;
	MoveDirection.Normalize();

	AddMovementInput(MoveDirection, Value);
}

void ARogueCharacter::MoveRight(float Value)
{
	RightInputValue = Value;

	if (bDead || bDashActive || FMath::IsNearlyZero(Value))
	{
		return;
	}

	if (GetWorld()->IsPaused())
	{
		return;
	}

	FVector MoveDirection = Camera != nullptr ? Camera->GetRightVector() : FVector::RightVector;
	MoveDirection.Z = 0.0f;
	MoveDirection.Normalize();

	AddMovementInput(MoveDirection, Value);
}

void ARogueCharacter::TurnCamera(float Value)
{
	if (bDead || FMath::IsNearlyZero(Value))
	{
		return;
	}

	if (GetWorld()->IsPaused())
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		FRotator ControlRotation = PlayerController->GetControlRotation();
		ControlRotation.Yaw += Value * CameraTurnSpeed;
		PlayerController->SetControlRotation(ControlRotation);
	}
}

void ARogueCharacter::PitchCamera(float Value)
{
	if (bDead || FMath::IsNearlyZero(Value))
	{
		return;
	}

	if (GetWorld()->IsPaused())
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		FRotator ControlRotation = PlayerController->GetControlRotation();
		ControlRotation.Pitch = FMath::ClampAngle(ControlRotation.Pitch + Value * CameraPitchSpeed, MinCameraPitch, MaxCameraPitch);
		PlayerController->SetControlRotation(ControlRotation);
	}
}

void ARogueCharacter::ZoomCamera(float Value)
{
	if (FMath::IsNearlyZero(Value) || SpringArm == nullptr)
	{
		return;
	}

	if (GetWorld()->IsPaused())
	{
		return;
	}

	SpringArm->TargetArmLength = FMath::Clamp(SpringArm->TargetArmLength - Value * CameraZoomStep, MinCameraDistance, MaxCameraDistance);
}

void ARogueCharacter::TogglePauseMenu()
{
	if (bDead)
	{
		return;
	}

	// 守卫改用本地 PlayerState 读"是否正在选卡/在商店"——这两个状态是每玩家的、且已复制到客户端。
	// 原来用 GetAuthGameMode()：客户端返回 nullptr → 守卫整个被跳过 →
	// 客户端玩家能在选升级卡的时候打开暂停菜单（服务端玩家不会）。
	if (ARoguePlayerState* RoguePlayerState = GetPlayerState<ARoguePlayerState>())
	{
		if (RoguePlayerState->IsAwaitingUpgradeChoice() || RoguePlayerState->IsShopOpen())
		{
			return;
		}
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController == nullptr)
	{
		return;
	}

	ARogueHUD* RogueHUD = Cast<ARogueHUD>(PlayerController->GetHUD());
	if (!IsValid(RogueHUD))
	{
		return;
	}

	if (RogueHUD->IsPauseMenuOpen())
	{
		RogueHUD->ClosePauseMenu();
		SetUpgradeSelectionInput(false);
	}
	else
	{
		RogueHUD->OpenPauseMenu();
		SetUpgradeSelectionInput(true);
	}

	// 注意：这里**不暂停游戏**。
	// 联机下 SetGamePaused 是全连接生效的 —— 一个人按 Esc 就会把所有人的世界冻住。
	// 现在 Esc 只是一个"非模态覆盖菜单"：菜单照常显示、鼠标照常出来
	// （SetUpgradeSelectionInput 用的是 FInputModeGameAndUI），但世界继续跑。
	// 单人游玩时如果需要"真正的暂停"，那应该做成一个显式的开关（比如只在
	// 玩家人数为 1 时才暂停），而不是默认行为。
}

void ARogueCharacter::Interact()
{
	if (bDead)
	{
		return;
	}

	// 走 PlayerState 的 ServerRPC —— 客户端同样可用。
	// 旧写法用 `GetWorld()->GetAuthGameMode()`，在客户端返回 nullptr，
	// 于是客户端玩家**根本打不开商店**（连交互提示也不会出现，见 HUD 的同类修正）。
	ARoguePlayerState* RoguePlayerState = GetPlayerState<ARoguePlayerState>();
	if (RoguePlayerState == nullptr)
	{
		return;
	}

	if (RoguePlayerState->IsShopOpen())
	{
		RoguePlayerState->Server_RequestCloseShop();
		return;
	}

	// 这两个判断是"本地意图"的前置条件，服务端还会再校一遍
	if (GetWorld()->IsPaused() || !NearbyShopTerminal.IsValid())
	{
		return;
	}

	RoguePlayerState->Server_RequestOpenShop();
}

void ARogueCharacter::Dash()
{
	if (bDead || GetWorld() == nullptr || GetWorld()->IsPaused())
	{
		return;
	}

	// 输入 → GAS。
	// "能不能冲刺"（是否死亡 / 是否已在冲刺 / 冷却是否走完）全部交给技能判定，
	// 见 URogueGameplayAbility_Dash 的 ActivationBlockedTags 与 CooldownTags。
	// 项目目前用的是旧版 InputComponent 绑定，所以在这里按 Tag 激活；
	// 将来迁到 Enhanced Input 后应改为 AbilityLocalInputPressed。
	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		FGameplayTagContainer DashAbilityTag;
		DashAbilityTag.AddTag(RogueGameplayTags::Ability_Dash);
		ASC->TryActivateAbilitiesByTag(DashAbilityTag);
	}
}

void ARogueCharacter::StartJump()
{
	if (bDead || bDashActive || GetWorld()->IsPaused() || JumpMaxCount <= 0)
	{
		return;
	}

	Jump();
}

void ARogueCharacter::StopJump()
{
	StopJumping();
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

void ARogueCharacter::HandleDash(float DeltaSeconds)
{
	// 迁移后这里**不再自己推进冲刺位移**（位移已交给 URogueAbilityTask_Dash），
	// 也不再用本地 float 当冷却账本。这个函数只剩两件事：
	//   1) 把"是否在冲刺中"从技能的 State.Dashing 标签同步过来 —— 表现层与
	//      移动/跳跃的守卫仍然读 bDashActive，但它们现在读的是一个**全局可查询的状态**，
	//      而不是一个只有本地知道的 bool；
	//   2) 把冷却剩余时间从冷却 GE 上查出来，供 HUD 画冷却条。
	URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent();
	if (ASC == nullptr)
	{
		// PlayerState 还没复制到位（或还没接上 GAS）——退化为"没有冲刺状态"
		bDashActive = false;
		DashTimeRemaining = 0.0f;
		return;
	}

	const bool bWasDashing = bDashActive;
	bDashActive = ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dashing);

	if (bDashActive)
	{
		// 刚进入冲刺时用属性里的时长做起点，之后逐帧倒数（纯表现用）
		if (!bWasDashing)
		{
			DashTimeRemaining = DashDuration;
		}
		else
		{
			DashTimeRemaining = FMath::Max(0.0f, DashTimeRemaining - DeltaSeconds);
		}
	}
	else
	{
		DashTimeRemaining = 0.0f;
	}

	// 冷却剩余时间：直接向 GAS 查询"身上还挂着多久的 Cooldown.Dash 效果"。
	// 这样冷却的唯一真相源就是那个 GE，而不是角色上的一个 float。
	FGameplayTagContainer CooldownTagContainer;
	CooldownTagContainer.AddTag(RogueGameplayTags::Cooldown_Dash);
	const TArray<float> RemainingTimes = ASC->GetActiveEffectsTimeRemaining(
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTagContainer));
	DashCooldownRemaining = RemainingTimes.Num() > 0 ? RemainingTimes[0] : 0.0f;
}

FVector ARogueCharacter::GetDashDirection() const
{
	FVector ForwardDirection = Camera != nullptr ? Camera->GetForwardVector() : GetActorForwardVector();
	ForwardDirection.Z = 0.0f;
	ForwardDirection.Normalize();

	FVector RightDirection = Camera != nullptr ? Camera->GetRightVector() : GetActorRightVector();
	RightDirection.Z = 0.0f;
	RightDirection.Normalize();

	FVector DesiredDirection = ForwardDirection * ForwardInputValue + RightDirection * RightInputValue;
	if (!DesiredDirection.Normalize())
	{
		DesiredDirection = GetVelocity().GetSafeNormal2D();
		if (DesiredDirection.IsNearlyZero())
		{
			DesiredDirection = GetActorForwardVector().GetSafeNormal2D();
		}
	}

	return DesiredDirection;
}

void ARogueCharacter::UpdateVisualPresentation(float DeltaSeconds)
{
	if (Visuals != nullptr)
	{
		Visuals->TickVisuals(DeltaSeconds, GetVelocity().Size2D(), bDashActive, DashDuration, DashTimeRemaining);
	}
}

void ARogueCharacter::LevelUp()
{
	++PlayerLevel;
	ExperienceToNextLevel = FMath::RoundToInt(static_cast<float>(ExperienceToNextLevel) * 1.30f + 2.0f);

	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->StartUpgradeSelection(this);
	}
}

UAbilitySystemComponent* ARogueCharacter::GetAbilitySystemComponent() const
{
	// 本项目的 ASC 挂在 PlayerState 上（玩家侧），这里转发过去。
	// 实现这个接口之后，"通过 Actor 找 ASC"的所有引擎/项目代码（含 ApplyDamageToTarget）
	// 都能正确拿到玩家的 ASC —— 这正是"怪物打玩家不掉血"的根因所在，详见头文件。
	const ARoguePlayerState* RoguePlayerState = GetPlayerState<ARoguePlayerState>();
	return RoguePlayerState != nullptr ? RoguePlayerState->GetAbilitySystemComponent() : nullptr;
}

void ARogueCharacter::ScheduleRevive(float DelaySeconds)
{
	// 只有权威侧能排复活。判据用 IsAuthoritySide()（GetNetMode）而不是 HasAuthority()：
	// 客户端上本机拥有的角色 HasAuthority() 也是 true，
	// 那会让客户端给自己排一个本地复活（本地改血/传送），和服务端的复制打架。
	if (!IsAuthoritySide() || GetWorld() == nullptr)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ReviveTimerHandle);

	// ---- 倒地等待 = 一个带时长的 GE（State.Downed，时长 = DelaySeconds）----
	//
	// 迁移前这里做的是"把截止时刻复制到 PlayerState，再让三处各自比对时钟"。
	// 现在"还剩多久"就是 GE 的剩余时长：到期 GAS 自己移除，客户端 HUD 读同一个 GE。
	// bRevivePending 只回答"有没有排过复活"（团灭时会取消），时长本身仍然只有 GE 一个来源。
	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		// 先清旧的（重复排程时不应该叠加两个倒计时）
		RemoveDownedEffect();

		if (DelaySeconds > 0.0f)
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(this);

			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(URogueDownedEffect::StaticClass(), 1.0f, Context);
			if (SpecHandle.IsValid() && SpecHandle.Data != nullptr)
			{
				SpecHandle.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Duration, DelaySeconds);
				ReviveEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}

			if (!ReviveEffectHandle.IsValid())
			{
				// 挂不上的后果是"倒地等待瞬间到期、立刻复活"（下面的轮询会把剩余时间当 0）。
				// 这比"永远躺着"好，但必须报错 —— 否则表现成"死亡没有代价"，很难定位。
				UE_LOG(LogTemp, Error, TEXT("[Rogue] %s 的倒地等待 GE 施加失败 —— 会立刻复活，请检查属性集/ASC 是否就绪"), *GetName());
			}
		}
	}

	bRevivePending = DelaySeconds > 0.0f;

	// 负值 / 0 = 取消（团灭时由 GameMode 调用来取消待复活）
	if (DelaySeconds <= 0.0f)
	{
		return;
	}

	// 定时器是**兜底驱动之一**，不是真相源：真相源是上面那个 GE 的剩余时长。
	// 保留它是因为本项目踩过"复活定时器没触发 → 人永远躺着"的坑，
	// 现在有三处独立驱动（角色 Tick、GameMode 兜底、这个定时器），全部走 TryReviveIfExpired() 这一个闸门。
	GetWorldTimerManager().SetTimer(ReviveTimerHandle, this, &ARogueCharacter::TryReviveIfExpired, DelaySeconds, false);

	UE_LOG(LogTemp, Log, TEXT("[Rogue] %s 进入倒地等待：%.1f 秒后回出生点复活（等待状态 = GE %s）"),
		*GetName(), DelaySeconds, *URogueDownedEffect::StaticClass()->GetName());
}

void ARogueCharacter::RemoveDownedEffect()
{
	ReviveEffectHandle = FActiveGameplayEffectHandle();
	bRevivePending = false;

	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		FGameplayTagContainer DownedTag;
		DownedTag.AddTag(RogueGameplayTags::State_Downed);
		ASC->RemoveActiveEffectsWithGrantedTags(DownedTag);
	}
}

float ARogueCharacter::GetReviveSecondsRemaining() const
{
	// 服务端与客户端都读同一个 GE（玩家 ASC 是 Mixed 模式，带时长的 GE 会同步给本人）。
	return GetDownedSecondsRemaining(GetRogueAbilitySystemComponent());
}

bool ARogueCharacter::IsRevivePending() const
{
	return bRevivePending && bDead;
}

void ARogueCharacter::TryReviveIfExpired()
{
	// 唯一的复活闸门：三个驱动（角色 Tick / GameMode 兜底 / 定时器）都调它。
	// 用"GE 剩余时长 <= 0"作为判据，不依赖任何定时器是否真的触发过。
	if (!IsAuthoritySide() || !IsRevivePending())
	{
		return;
	}

	if (GetReviveSecondsRemaining() > 0.0f)
	{
		return;
	}

	Revive();
}

void ARogueCharacter::Revive()
{
	// 复活是权威操作：只有服务端能做（判据见 ScheduleRevive 的注释）
	if (!IsAuthoritySide() || !bDead)
	{
		return;
	}

	// 1) 清死亡标签 —— 它是"是否已死"的唯一真相源。
	//    注意这会触发 HandleDeathTagChanged(NewCount=0)，那边的实现对新标签数 <= 0 直接返回，
	//    所以不会和本函数打架。
	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		FGameplayTagContainer DeadTag;
		DeadTag.AddTag(RogueGameplayTags::State_Dead);
		ASC->RemoveActiveEffectsWithGrantedTags(DeadTag);
	}

	// 2) 解除"已死"标记，并回血到上限的 50%
	bDead = false;

	// 清掉倒地等待状态（GE 移除 → State.Downed 消失 → 客户端 HUD 从"倒地等待"切回正常显示）
	RemoveDownedEffect();

	if (const URogueAttributeSet* AttrSet = GetRogueAttributeSet())
	{
		SetCurrentHealth(AttrSet->GetMaxHealth() * 0.5f);
	}

	// 3) 清掉"刚受伤"状态，让护甲能正常回复
	ClearRecentlyDamagedState();

	// 3.5) 复活保护：1.5 秒无敌（设计：刚回出生点的玩家不该被瞬间再打死）。
	//
	// 迁移前这是"loose tag + 角色上的 float 截止时刻 + Tick 比对服务器时间"。
	// 现在是一个带时长的 GE：到期由 GAS 移除，不再依赖角色 Tick 是否还在跑
	// （这个项目已经踩过两次"倒下的角色不再 Tick → 清理逻辑永远不执行"的坑）。
	if (URogueAbilitySystemComponent* ReviveASC = GetRogueAbilitySystemComponent())
	{
		RemoveInvulnerableEffect();

		FGameplayEffectContextHandle Context = ReviveASC->MakeEffectContext();
		Context.AddSourceObject(this);

		FGameplayEffectSpecHandle SpecHandle = ReviveASC->MakeOutgoingSpec(URogueInvulnerableEffect::StaticClass(), 1.0f, Context);
		if (SpecHandle.IsValid() && SpecHandle.Data != nullptr)
		{
			SpecHandle.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Duration, ReviveInvulnerableSeconds);
			InvulnerableEffectHandle = ReviveASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}

		if (!InvulnerableEffectHandle.IsValid())
		{
			// 挂不上的后果是"复活后没有无敌帧"，属于玩法强度问题，必须能看见。
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] %s 的复活无敌 GE 施加失败 —— 复活后不会有无敌帧"), *GetName());
		}
	}

	// 4) 回出生点（与 GameMode 开局摆放玩家的落点一致）
	SetActorLocation(FVector(0.0f, 0.0f, 120.0f), false, nullptr, ETeleportType::TeleportPhysics);

	// 5) 恢复输入 / 移动 / 碰撞（客户端由 bDead 复制的 OnRep 走同一个函数）
	ApplyDownedState();

	// 6) 关掉死亡界面的 UI 输入模式（HUD 每帧的推导也会随之切回 GameOnly）
	SetUpgradeSelectionInput(false);

	UE_LOG(LogTemp, Log, TEXT("[Rogue] %s 已复活（回到出生点，生命 %.0f%%，%.1f 秒无敌；无敌 GE=%s 当前带 State.Invulnerable=%d）"),
		*GetName(),
		(GetRogueAttributeSet() != nullptr && GetRogueAttributeSet()->GetMaxHealth() > 0.0f)
			? (GetRogueAttributeSet()->GetHealth() / GetRogueAttributeSet()->GetMaxHealth() * 100.0f)
			: 0.0f,
		ReviveInvulnerableSeconds,
		InvulnerableEffectHandle.IsValid() ? TEXT("已挂") : TEXT("未挂"),
		(GetRogueAbilitySystemComponent() != nullptr
			&& GetRogueAbilitySystemComponent()->HasMatchingGameplayTag(RogueGameplayTags::State_Invulnerable)) ? 1 : 0);
}

void ARogueCharacter::RemoveInvulnerableEffect()
{
	InvulnerableEffectHandle = FActiveGameplayEffectHandle();

	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		// 按"授予的标签"移除（而不是按 GE 类）：这样将来冲刺无敌帧之类的效果
		// 只要也授予 State.Invulnerable，死亡时会被一起清掉 —— 与迁移前的语义一致
		// （迁移前是 SetLooseGameplayTagCount(State.Invulnerable, 0)，同样是按标签清）。
		FGameplayTagContainer InvulnerableTag;
		InvulnerableTag.AddTag(RogueGameplayTags::State_Invulnerable);
		ASC->RemoveActiveEffectsWithGrantedTags(InvulnerableTag);
	}
}

void ARogueCharacter::SetMenuInteractionInput(bool bEnabled){
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController == nullptr)
	{
		return;
	}

	PlayerController->bShowMouseCursor = bEnabled;
	PlayerController->bEnableClickEvents = bEnabled;
	PlayerController->bEnableMouseOverEvents = bEnabled;

	if (bEnabled)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
	else
	{
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
	}
}

void ARogueCharacter::Die()
{
	// 死亡表现由复制驱动：客户端不在这里做任何"权威"动作。
	//
	// ⚠️ 判据必须是 GetNetMode()，**不能是 HasAuthority()**：
	//   客户端上"本机拥有"的角色 Role 也是 ROLE_Authority（本地控制的 Pawn 由客户端自己模拟），
	//   用 HasAuthority() 会让客户端给自己排一个"10 秒后复活"、
	//   甚至在本地把血量改回 50%（这些值都是服务端复制的，会被立刻覆盖，表现为抖动）。
	if (IsAuthoritySide() == false)
	{
		ApplyDownedState();
		return;
	}

	bDead = true;

	// 死了就不该再带着"复活无敌"标签（否则尸体在伤害闸门那里是"无敌"，
	// 排查时会看到 Dead=1 Invul=1 这种自相矛盾的状态）。
	RemoveInvulnerableEffect();

	// 联机合作：死亡不是出局，而是**倒地等待**（默认 10 秒后回出生点复活）。
	//
	// ⚠️ 这里对延迟做了下限保护：ReviveDelaySeconds 是 EditDefaultsOnly，
	// 而**蓝图子类是早于这个属性存在的**——BP 里该字段可能是 0。
	// 一旦传 0 就会走 ScheduleRevive 的"取消"分支，后果是死后永远躺着、
	// 死亡界面也永远不显示倒计时（实测正是如此：日志里没有任何"已复活"记录）。
	const float EffectiveReviveDelay = ReviveDelaySeconds > 0.0f ? ReviveDelaySeconds : 10.0f;
	ScheduleRevive(EffectiveReviveDelay);
	SetUpgradeSelectionInput(true);

	// 通知所有武器角色死亡
	for (ARogueWeaponBase* Weapon : Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->OnOwnerDied();
		}
	}

	// 本机表现：关输入、关移动、关碰撞。客户端由 OnRep_Dead 走同一个函数。
	ApplyDownedState();

	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->NotifyPlayerDied(this);
	}
}

void ARogueCharacter::ApplyDownedState()
{
	if (bDead)
	{
		DisableInput(Cast<APlayerController>(GetController()));
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->DisableMovement();
		}

		// 濒死状态：**取消碰撞**。
		// 理由：倒下的玩家不该再和世界发生任何物理/查询交互 ——
		// 怪物不能撞到他（配合"不索敌"）、他也捡不到经验球、不会挡住队友与弹体。
		// 复活时由 OnRep_Dead / Revive() 恢复。
		SetActorEnableCollision(false);
	}
	else
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			EnableInput(PlayerController);
		}
		SetActorEnableCollision(true);
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
}

void ARogueCharacter::OnRep_Dead()
{
	// 服务端改了 bDead → 所有端在这里切换"倒地/正常"表现。
	// ⚠️ 位置/血量不在这里改：那些只由服务端的 Revive() 决定，客户端等复制。
	ApplyDownedState();

	UE_LOG(LogTemp, Log, TEXT("[Rogue] %s %s（复制同步）"),
		*GetName(), bDead ? TEXT("进入倒地等待") : TEXT("已复活"));
}

bool ARogueCharacter::IsAuthoritySide() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() != NM_Client;
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