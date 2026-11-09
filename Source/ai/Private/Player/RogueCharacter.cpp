#include "Player/RogueCharacter.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Player/RogueFloatingComponent.h"
#include "Player/RoguePlayerMotionComponent.h"

#include "Player/RogueTurnSmoothingComponent.h"
#include "Player/RogueShieldOrbitComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"

#include "Framework/RogueGameMode.h"
#include "Player/RoguePlayerState.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
// Enhanced Input：绑定与必需资产校验见 RogueCharacter.Input.cpp；缺失时不绑定本角色动作。
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"


void ARogueCharacter::Multicast_DashPresentation_Implementation(
    bool bStarting, FVector_NetQuantize Position, FVector_NetQuantizeNormal Direction)
{
    if (GetNetMode() == NM_DedicatedServer) return;
    if (auto* Motion = FindComponentByClass<URoguePlayerMotionComponent>())
        Motion->PlayDashPresentation(bStarting, Position, Direction);
}

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
    PlayerVisualMotion = CreateDefaultSubobject<URoguePlayerMotionComponent>(TEXT("PlayerVisualMotion"));
	ShieldOrbit = CreateDefaultSubobject<URogueShieldOrbitComponent>(TEXT("ShieldOrbit"));

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	GetCharacterMovement()->AirControl = AirControl;
	GetCharacterMovement()->GravityScale = GravityScale;
	JumpMaxCount = 0;


	AutoPossessPlayer = EAutoReceiveInput::Disabled;
}

bool ARogueCharacter::CanJumpInternal_Implementation() const
{
	return false;
}

void ARogueCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	
	ApplyBalanceAsset();

	// 正式生命/护甲初始化等待本玩家永久成长档案就绪，再经 InitializeGASAttributes 写入。
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
		// 远端档案可能晚于接管到达；PlayerState 在接收成功后会再次通知本 Pawn。
		NotifyProgressionReady();
	}
	if (ARogueGameMode* Mode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		if (Mode->IsGameOver())
		{
            if (Mode->IsVictory()) FinalizeForVictory(); else FinalizeForGameOver();
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

	// 节流窗口到期时补弹累计的回血量（否则窗口内最后一跳会一直压着不显示）。
	TryFlushHealingNumber();

	// 跳跃手感：土狼时间、输入缓冲、下落重力。放在移动组件 Tick 之前生效。
	UpdateFallingGravity();

	// 开发诊断独立实现；正式构建为空操作，不保留日志采样状态。
	LogWeaponStateForSelfTest();

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
