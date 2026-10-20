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
// 所需资产缺失时 SetupEnhancedInput 会记录错误并返回，便于定位输入配置问题。
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
	 *  找不到返回 nullptr，由 SetupEnhancedInput 汇总并报告缺失的必需资产。 */
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
void ARogueCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 角色动作统一经 Enhanced Input 绑定；必需资产验证失败会中止本次绑定，无 legacy 回退。
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

	// 局部输入资产使用 Asset 后缀，避免遮蔽同名成员，便于区分配置引用与加载结果。
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

	// 汇总全部必需资产；缺少任一项就中止本角色动作绑定，不做部分绑定或 legacy 回退。
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
		// 必需配置缺失作为错误记录；这里只能确定本函数未绑定角色动作，
		// 不能由此推断其他组件、UI 或控制器也完全没有输入。
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

	// 输入适配函数转调角色移动、摄像机与技能方法，
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

	// 记录映射条数以发现空上下文；非零数量不能证明具体按键、修饰器或动作类型配置正确。
	// 默认资产预期共 17 条，若调整按键需同时核对实际映射。
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
	if (!IsAlive() || GetWorld() == nullptr || GetWorld()->IsPaused())
	{
		return;
	}

	// 走 GAS 的**原生输入路径**：按 InputID 找技能并激活。
	// 冲刺能力在授予时带了 RogueAbilityInputID::Dash（见 URogueAbilitySystemComponent::GrantDefaultAbilities）。
	// "能不能冲刺"仍完全由技能自己判定（ActivationBlockedTags + 冷却 GE）——
	// 生命快照先阻止濒死输入，其余条件由能力判定，避免等待迟到的 GAS 标签。
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
	// 这里尝试按标签激活已存在的能力；列表尚未复制时不会缓存或重放本次输入。
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

// ---- 角色移动 / 摄像机 / 冲刺 / 护甲 / 恢复 ----

void ARogueCharacter::MoveForward(float Value)
{
	ForwardInputValue = Value;

	if (!IsAlive() || bDashActive || FMath::IsNearlyZero(Value))
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

	if (!IsAlive() || bDashActive || FMath::IsNearlyZero(Value))
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
	if (!IsAlive() || FMath::IsNearlyZero(Value))
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
	if (!IsAlive() || FMath::IsNearlyZero(Value))
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
	if (!IsAlive())
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

	// Esc 仅切换本机菜单和输入，不主动暂停世界。服务器暂停会影响整局推进，
	// 但 SetGamePaused 是否成功还受 GameMode、暂停权限及控制器状态约束，
	// 客户端本地调用也不等于服务器一定暂停。
}

void ARogueCharacter::Interact()
{
	if (!IsAlive())
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
	if (!IsAlive() || GetWorld() == nullptr || GetWorld()->IsPaused())
	{
		return;
	}

	// 输入 → GAS。
	// "能不能冲刺"（是否死亡 / 是否已在冲刺 / 冷却是否走完）全部交给技能判定，
	// 见 URogueGameplayAbility_Dash 的 ActivationBlockedTags 与 CooldownTags。
	//
	// ⚠️ 本函数现在是**兜底路径**。正常路径是 HandleDashInput 里的
	// `ASC->AbilityLocalInputPressed(RogueAbilityInputID::Dash)` —— 那是 GAS 的原生输入路径，
	// 冲刺能力在授予时就带了 InputID（见 URogueAbilitySystemComponent::GrantDefaultAbilities）。
	// 未找到 Dash InputID 时落到这里，尝试按 Tag 激活已有能力；
	// 若能力尚不存在，这次尝试仍可能失败，当前没有输入缓冲。
	//
	// （这段注释此前写的是"项目目前用的是旧版 InputComponent 绑定，
	//   将来迁到 Enhanced Input 后应改为 AbilityLocalInputPressed" —— 那个"将来"已经到了。）
	if (URogueAbilitySystemComponent* ASC = GetRogueAbilitySystemComponent())
	{
		FGameplayTagContainer DashAbilityTag;
		DashAbilityTag.AddTag(RogueGameplayTags::Ability_Dash);
		ASC->TryActivateAbilitiesByTag(DashAbilityTag);
	}
}

void ARogueCharacter::UpdateJumpFeel(float DeltaSeconds)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement == nullptr)
	{
		return;
	}

	// 土狼时间：刚离开地面仍保留一次起跳机会。
	if (Movement->IsMovingOnGround())
	{
		CoyoteTimeRemaining = FMath::Max(0.0f, CoyoteTimeSeconds);
		bJumpedSinceGrounded = false;
	}
	else if (CoyoteTimeRemaining > 0.0f)
	{
		CoyoteTimeRemaining = FMath::Max(0.0f, CoyoteTimeRemaining - DeltaSeconds);
	}

	// 跳跃输入缓冲：落地/恢复可跳时消费。
	if (JumpBufferRemaining > 0.0f)
	{
		JumpBufferRemaining = FMath::Max(0.0f, JumpBufferRemaining - DeltaSeconds);
		if (IsAlive() && !bDashActive && CanJump())
		{
			JumpBufferRemaining = 0.0f;
			CoyoteTimeRemaining = 0.0f;
			bJumpedSinceGrounded = true;
			Jump();
		}
	}

	// 下落阶段额外加一份重力：上升慢、下落快，落地不拖沓。
	const bool bShouldFallFast = Movement->IsFalling() && Movement->Velocity.Z < 0.0f && !bDashActive;
	const float DesiredGravity = GravityScale * (bShouldFallFast ? FMath::Max(1.0f, FallGravityMultiplier) : 1.0f);
	if (!FMath::IsNearlyEqual(Movement->GravityScale, DesiredGravity, 0.001f))
	{
		Movement->GravityScale = DesiredGravity;
	}
}

void ARogueCharacter::StartJump()
{
	UWorld* World = GetWorld();
	if (!IsAlive() || bDashActive || World == nullptr || World->IsPaused() || JumpMaxCount <= 0)
	{
		return;
	}

	if (CanJump())
	{
		JumpBufferRemaining = 0.0f;
		CoyoteTimeRemaining = 0.0f;
		bJumpedSinceGrounded = true;
		Jump();
		return;
	}

	// 土狼跳：已经离地但在宽限时间内，且本次离地后还没跳过。
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement != nullptr && Movement->IsFalling() && !bJumpedSinceGrounded
		&& CoyoteTimeRemaining > 0.0f && JumpCurrentCount == 0)
	{
		CoyoteTimeRemaining = 0.0f;
		bJumpedSinceGrounded = true;
		JumpCurrentCount = 1;
		LaunchCharacter(FVector(0.0f, 0.0f, JumpZVelocity), false, true);
		return;
	}

	// 空中按跳：记一个短缓冲，落地后自动起跳。
	JumpBufferRemaining = JumpBufferSeconds;
}

void ARogueCharacter::StopJump()
{
	// 可变跳跃高度：上升中提前松键就砍掉一部分垂直速度。
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (Movement->IsFalling() && Movement->Velocity.Z > 0.0f)
		{
			Movement->Velocity.Z *= FMath::Clamp(JumpCutMultiplier, 0.0f, 1.0f);
		}
	}

	StopJumping();
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

void ARogueCharacter::SetMenuInteractionInput(bool bEnabled)
{
	// 只处理打开；最终输入模式由 ARogueHUD::UpdateInteractionInputMode 推导。
	//
	// 这条 `if` 就是那个真实 bug 的修复点：关店时会调本函数传 false，
	// 而"关店"并不等于"不再需要光标"（可能还有待选的升级卡挂着）。
	// 由本函数盲目关掉，HUD 又因为推导状态没变而不重设 → 鼠标消失、卡点不了。
	if (!bEnabled)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController == nullptr)
	{
		return;
	}

	PlayerController->bShowMouseCursor = true;
	PlayerController->bEnableClickEvents = true;
	PlayerController->bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
}
