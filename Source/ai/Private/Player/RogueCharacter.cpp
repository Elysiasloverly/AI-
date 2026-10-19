#include "Player/RogueCharacter.h"
#include "Player/RogueFloatingComponent.h"
#include "Player/RogueTurnSmoothingComponent.h"
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


ARogueCharacter::ARogueCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// UE5.5 的 APawn 构造已默认启用复制；这里显式表达角色的复制与移动同步要求。
	// 派生蓝图的默认属性仍可能覆盖 C++ 默认值，排查时需检查实例实际配置。
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
    FloatingMotion=CreateDefaultSubobject<URogueFloatingComponent>(TEXT("FloatingMotion"));
    FloatingMotion->SetTargetComponent(Visuals);
    TurnSmoothing=CreateDefaultSubobject<URogueTurnSmoothingComponent>(TEXT("TurnSmoothing"));
    TurnSmoothing->SetTargetComponent(Visuals);

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

	// 正式生命/护甲初始化在 PossessedBy → InitializeGASAttributes。
	// 尚无属性集时这里只初始化显示镜像，不会创建 ASC 或写入 GAS 生命值。
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
	// 先让 ASC 认识 PlayerState/Pawn，再初始化数值、绑定事件并同步生命周期效果。
	// AttributeSet 的构造和复制登记由 PlayerState 负责，并非 InitAbilityActorInfo 自动登记。
	if (ARoguePlayerState* RoguePS = GetPlayerState<ARoguePlayerState>())
	{
		RoguePS->RefreshAbilityActorInfo(this);//先把ASC的owner/avatar绑定好
		InitializeGASAttributes();//输入基础数值
		BindAbilitySystemEvents();//绑定事件
		ApplyLifeStateEffects();//应用当前生命阶段的效果
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

	// 属性镜像在 IsAlive() 早退前同步：
	// 死亡后 HUD 仍要能读到最终血量（归零），所以这里不能因为死亡就跳过。
	SyncAttributeMirrors();

	// 自检仅记录本机控制角色的属性集、角色快照与武器生效值，最多输出八组。
	// 放在存活检查之前，允许倒地期间继续诊断；需与服务端同玩家、同阶段的日志对比。
	// 单有客户端日志不能证明复制成功，更不能证明预测或长期一致性。
	if (IsLocallyControlled() && RogueSelfTest::IsWeaponUpgradeChainEnabled() && WeaponBonusLogCount < 8)
	{
		// 注意这里**不是"只打一次"**：客户端第一帧通常还没收到复制过来的属性值，
		// 只打一次可能看到尚未同步的初值；值变化时再记录（最多八组），
		// 日志里就能看出 0 → 真值 的变化过程，从而区分"没复制"和"还没到"。
		if (const ARoguePlayerState* RoguePS = GetPlayerState<ARoguePlayerState>())
		{
			const URogueAttributeSet* WeaponSet = RoguePS->GetAttributeSet();
			if (WeaponSet != nullptr)
			{
				// 同时记录生命/护甲、五项通用加成与角色快照，帮助定位两条数值路径的差异。
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
					// 带上玩家名帮助匹配日志；还需确认对应玩家身份、测试阶段和时间，避免比较不同角色。
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
					// 与服务端同玩家、同阶段对比可检查本次读取结果，不代表所有状态与时序都已覆盖。
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
					// 访问器读取 CurrentValue；分别打印 BaseValue/CurrentValue 并对照护甲，
					// 便于研究基值、聚合修饰器和复制通知路径；单次差异本身不足以确定故障原因。
					UE_LOG(LogTemp, Log, TEXT("[SelfTest]   原始结构体：伤害加成(Base=%.2f Cur=%.2f) 护甲(Base=%.2f Cur=%.2f)"),
						WeaponSet->AttackPowerBonus.GetBaseValue(), WeaponSet->AttackPowerBonus.GetCurrentValue(),
						WeaponSet->Armor.GetBaseValue(), WeaponSet->Armor.GetCurrentValue());

					// 再查"这两个属性在本机的复制注册状态"。
					// CPF_Net 与 RepIndex 可辅助检查反射层复制声明；
					// 生命周期注册、对象复制通道与条件仍需另外核验，不能只凭索引判断已发送或收到。
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

	// 先准备能力和诊断，再按存活状态决定是否运行武器逻辑；倒地期间也可等待复制的能力列表。
	EnsureWeaponAbilities();
	TickWeaponFireReport(DeltaSeconds);

	if (!IsAlive())
	{
		ApplyDownedState();
		return;
	}

	HandleDash(DeltaSeconds);
	// 生命 / 护甲回复**不在这里了**：已改为两个常驻的 Periodic GE（见 EnsureRegenEffects）。
	// 这一条同时消掉了原来"倒下的角色不再 Tick，于是回复逻辑行为异常"的隐患 ——
	// 回复由 ASC 的周期执行推进，不依赖此角色 Tick；世界暂停仍会影响世界定时器。
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

bool ARogueCharacter::IsAuthoritySide() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() != NM_Client;
}