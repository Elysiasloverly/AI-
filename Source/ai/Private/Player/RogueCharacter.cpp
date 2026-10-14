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

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
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

	ReviveSpawnTransform = GetActorTransform();
	InitializeWeapons();
	ApplyDownedState();
}

void ARogueCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	UE_LOG(LogTemp,Log,TEXT("netmode = %d"),static_cast<int32>(GetNetMode()));

	// ── 服务端侧 ──
	// 两条顺序不能颠倒：必须先把 ASC 的 Owner/Avatar 设好，
	// 属性集才会被 ASC 登记进 SpawnedAttributes；
	// 之后再写基础值，SetXxx() 内部才能拿到 AbilitySystemComponent。
	if (ARoguePlayerState* RoguePS = GetPlayerState<ARoguePlayerState>())
	{
		RoguePS->RefreshAbilityActorInfo(this);
		InitializeGASAttributes();
		BindAbilitySystemEvents();
		ApplyLifeStateEffects();
	}
	if (ARogueGameMode* Mode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		if (Mode->IsGameOver())
		{
			FinalizeForGameOver();
		}
	}
	ApplyDownedState();
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
	ApplyDownedState();
}

void ARogueCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 属性镜像同步放在最前面，且**在 bDead 早退之前** ——
	// 死亡后 HUD 仍要能读到最终血量（归零），所以这里不能因为死亡就跳过。
	SyncAttributeMirrors();

	// 开发自检（默认关，见 Debug/RogueSelfTest.h）：**每台机器**打印一次本机角色读到的武器加成。
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

	if (!IsAlive())
	{
		ApplyDownedState();
		return;
	}

	HandleDash(DeltaSeconds);
	UpdateVisualPresentation(DeltaSeconds);
	// 生命 / 护甲回复**不在这里了**：已改为两个常驻的 Periodic GE（见 EnsureRegenEffects）。
	// 这一条同时消掉了原来"倒下的角色不再 Tick，于是回复逻辑行为异常"的隐患 ——
	// 现在回复由 GAS 按周期推进，与角色是否 Tick 无关。
	TickWeapons(DeltaSeconds);
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
	DOREPLIFETIME(ARogueCharacter, LifeState);

	// 武器加成快照（复制给所有端）：真相源是属性集，这里是跨端同步管道
	DOREPLIFETIME(ARogueCharacter, WeaponBonusSnapshot);
}

void ARogueCharacter::UpdateVisualPresentation(float DeltaSeconds)
{
	if (Visuals != nullptr)
	{
		Visuals->TickVisuals(DeltaSeconds, GetVelocity().Size2D(), bDashActive, DashDuration, DashTimeRemaining);
	}
}

bool ARogueCharacter::IsAuthoritySide() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() != NM_Client;
}