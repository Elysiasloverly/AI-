#include "Player/RogueCharacter.h"

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
#include "Core/RogueUpgradeEffectApplier.h"
#include "Core/RogueGameMode.h"
#include "Player/RoguePlayerBalanceAsset.h"
#include "World/RogueShopTerminal.h"
#include "UI/RogueHUD.h"
#include "Combat/RogueImpactEffect.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"

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
	CurrentHealth = MaxHealth;
	CurrentArmor = MaxArmor;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->SetControlRotation(FRotator(-55.0f, 45.0f, 0.0f));
		PlayerController->bShowMouseCursor = false;
		PlayerController->bEnableClickEvents = false;
		PlayerController->bEnableMouseOverEvents = false;
	}

	InitializeWeapons();
}

void ARogueCharacter::ApplyBalanceAsset()
{
	LoadedPlayerBalanceAsset = LoadConfiguredOrDefaultPlayerBalanceAsset(PlayerBalanceAsset);
	if (LoadedPlayerBalanceAsset == nullptr)
	{
		return;
	}

	const FRoguePlayerBaseStatConfig& BaseStats = LoadedPlayerBalanceAsset->BaseStats;
	ExperienceToNextLevel = BaseStats.ExperienceToNextLevel;
	MaxHealth = BaseStats.MaxHealth;
	MaxArmor = BaseStats.MaxArmor;
	MoveSpeed = BaseStats.MoveSpeed;
	PickupRadius = BaseStats.PickupRadius;
	HealthRegenPerSecond = BaseStats.HealthRegenPerSecond;
	DamageReductionPercent = BaseStats.DamageReductionPercent;
	ExperienceMultiplier = BaseStats.ExperienceMultiplier;
	ArmorRechargeDelay = BaseStats.ArmorRechargeDelay;
	ArmorRechargeRate = BaseStats.ArmorRechargeRate;
	DashCooldownDuration = BaseStats.DashCooldownDuration;
	DashMinCooldown = BaseStats.DashMinCooldown;
	DashDuration = BaseStats.DashDuration;
	DashSpeed = BaseStats.DashSpeed;
	JumpZVelocity = BaseStats.JumpZVelocity;
	AirControl = BaseStats.AirControl;
	GravityScale = BaseStats.GravityScale;
	JumpMaxCountConfig = BaseStats.JumpMaxCount;
	GetCharacterMovement()->JumpZVelocity = JumpZVelocity;
	GetCharacterMovement()->AirControl = AirControl;
	GetCharacterMovement()->GravityScale = GravityScale;
	JumpMaxCount = FMath::Max(0, JumpMaxCountConfig);
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
	for (ARogueWeaponBase* Weapon : Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->WeaponTick(DeltaSeconds);
		}
	}
}

void ARogueCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead)
	{
		return;
	}

	HandleDash(DeltaSeconds);
	HandleArmorRecharge(DeltaSeconds);
	UpdateVisualPresentation(DeltaSeconds);
	HandleRecovery(DeltaSeconds);
	TickWeapons(DeltaSeconds);
}

void ARogueCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ARogueCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ARogueCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("TurnCamera"), this, &ARogueCharacter::TurnCamera);
	PlayerInputComponent->BindAxis(TEXT("PitchCamera"), this, &ARogueCharacter::PitchCamera);
	PlayerInputComponent->BindAxis(TEXT("ZoomCamera"), this, &ARogueCharacter::ZoomCamera);
	FInputActionBinding& PauseMenuBinding = PlayerInputComponent->BindAction(TEXT("TogglePauseMenu"), IE_Pressed, this, &ARogueCharacter::TogglePauseMenu);
	PauseMenuBinding.bExecuteWhenPaused = true;
	FInputActionBinding& InteractBinding = PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &ARogueCharacter::Interact);
	InteractBinding.bExecuteWhenPaused = true;
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ARogueCharacter::StartJump);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ARogueCharacter::StopJump);
	PlayerInputComponent->BindAction(TEXT("Dash"), IE_Pressed, this, &ARogueCharacter::Dash);

	FInputActionBinding& UpgradeOneBinding = PlayerInputComponent->BindAction(TEXT("ChooseUpgrade1"), IE_Pressed, this, &ARogueCharacter::ChooseUpgradeOne);
	UpgradeOneBinding.bExecuteWhenPaused = true;

	FInputActionBinding& UpgradeTwoBinding = PlayerInputComponent->BindAction(TEXT("ChooseUpgrade2"), IE_Pressed, this, &ARogueCharacter::ChooseUpgradeTwo);
	UpgradeTwoBinding.bExecuteWhenPaused = true;

	FInputActionBinding& UpgradeThreeBinding = PlayerInputComponent->BindAction(TEXT("ChooseUpgrade3"), IE_Pressed, this, &ARogueCharacter::ChooseUpgradeThree);
	UpgradeThreeBinding.bExecuteWhenPaused = true;
}

float ARogueCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead)
	{
		return 0.0f;
	}

	const float IncomingDamage = FMath::Max(0.0f, DamageAmount);
	const float MitigatedDamage = IncomingDamage * (1.0f - DamageReductionPercent);
	float RemainingDamage = MitigatedDamage;

	if (RemainingDamage > 0.0f)
	{
		ArmorRechargeDelayRemaining = ArmorRechargeDelay;
	}

	if (CurrentArmor > 0.0f && RemainingDamage > 0.0f)
	{
		const float ArmorAbsorbedDamage = FMath::Min(CurrentArmor, RemainingDamage);
		CurrentArmor -= ArmorAbsorbedDamage;
		RemainingDamage -= ArmorAbsorbedDamage;
	}

	if (RemainingDamage > 0.0f)
	{
		CurrentHealth -= FMath::Min(CurrentHealth, RemainingDamage);
	}

	if (CurrentHealth <= KINDA_SMALL_NUMBER)
	{
		Die();
	}

	return MitigatedDamage;
}

void ARogueCharacter::AddExperience(int32 ExperienceAmount)
{
	if (bDead)
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
	Money += FMath::Max(0, Amount);
}

void ARogueCharacter::SetNearbyShopTerminal(ARogueShopTerminal* ShopTerminal)
{
	NearbyShopTerminal = ShopTerminal;
}

bool ARogueCharacter::TrySpendMoney(int32 Amount)
{
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
	FRogueUpgradeEffectApplier::ApplyToCharacter(*this, Upgrade);
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

void ARogueCharacter::ApplySharedWeaponDamageBonus(float Magnitude)
{
	for (ARogueWeaponBase* Weapon : Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->ApplySharedDamageBonus(Magnitude);
		}
	}
}

void ARogueCharacter::ApplySharedWeaponSpeedBonus(float Magnitude)
{
	for (ARogueWeaponBase* Weapon : Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->ApplySharedSpeedBonus(Magnitude);
		}
	}
}

void ARogueCharacter::ApplySharedWeaponRangeBonus(float Magnitude)
{
	for (ARogueWeaponBase* Weapon : Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->ApplySharedRangeBonus(Magnitude);
		}
	}
}

bool ARogueCharacter::DispatchWeaponUpgrade(ERogueUpgradeType UpgradeType, float Magnitude)
{
	for (ARogueWeaponBase* Weapon : Weapons)
	{
		if (IsValid(Weapon) && Weapon->OnUpgradeApplied(UpgradeType, Magnitude))
		{
			return true;
		}
	}
	return false;
}

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

void ARogueCharacter::HandleArmorRecharge(float DeltaSeconds)
{
	if (MaxArmor <= 0.0f || CurrentArmor >= MaxArmor)
	{
		return;
	}

	ArmorRechargeDelayRemaining = FMath::Max(0.0f, ArmorRechargeDelayRemaining - DeltaSeconds);
	if (ArmorRechargeDelayRemaining > 0.0f)
	{
		return;
	}

	CurrentArmor = FMath::Min(MaxArmor, CurrentArmor + ArmorRechargeRate * DeltaSeconds);
}

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

	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		if (RogueGameMode->IsAwaitingUpgradeChoice() || RogueGameMode->IsShopOpen())
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
		UGameplayStatics::SetGamePaused(this, false);
	}
	else
	{
		RogueHUD->OpenPauseMenu();
		SetUpgradeSelectionInput(true);
		UGameplayStatics::SetGamePaused(this, true);
	}
}

void ARogueCharacter::Interact()
{
	if (bDead)
	{
		return;
	}

	ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	if (RogueGameMode == nullptr)
	{
		return;
	}

	if (RogueGameMode->IsShopOpen())
	{
		RogueGameMode->CloseShop();
		return;
	}

	if (GetWorld()->IsPaused() || !NearbyShopTerminal.IsValid())
	{
		return;
	}

	RogueGameMode->TryOpenShop(this);
}

void ARogueCharacter::Dash()
{
	if (bDead || bDashActive || GetWorld()->IsPaused() || DashCooldownRemaining > 0.0f)
	{
		return;
	}

	DashDirection = GetDashDirection();
	if (DashDirection.IsNearlyZero())
	{
		return;
	}

	bDashActive = true;
	DashTimeRemaining = DashDuration;
	DashCooldownRemaining = DashCooldownDuration;
	GetCharacterMovement()->StopMovementImmediately();
	SetActorRotation(DashDirection.Rotation());

	ARogueImpactEffect::SpawnImpactEffect(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, 38.0f), DashDirection.Rotation(), ERogueImpactVisualStyle::Laser, FVector(0.42f, 0.42f, 0.20f), 0.18f, this);
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

void ARogueCharacter::ChooseUpgradeOne()
{
	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->TrySelectUpgrade(0);
	}
}

void ARogueCharacter::ChooseUpgradeTwo()
{
	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->TrySelectUpgrade(1);
	}
}

void ARogueCharacter::ChooseUpgradeThree()
{
	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->TrySelectUpgrade(2);
	}
}

void ARogueCharacter::HandleDash(float DeltaSeconds)
{
	DashCooldownRemaining = FMath::Max(0.0f, DashCooldownRemaining - DeltaSeconds);

	if (!bDashActive)
	{
		return;
	}

	DashTimeRemaining -= DeltaSeconds;

	FHitResult HitResult;
	AddActorWorldOffset(DashDirection * DashSpeed * DeltaSeconds, true, &HitResult);
	if (HitResult.bBlockingHit)
	{
		DashTimeRemaining = 0.0f;
	}

	if (DashTimeRemaining <= 0.0f)
	{
		bDashActive = false;
	}
}

void ARogueCharacter::HandleRecovery(float DeltaSeconds)
{
	if (HealthRegenPerSecond <= 0.0f || CurrentHealth >= MaxHealth)
	{
		return;
	}

	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + HealthRegenPerSecond * DeltaSeconds);
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

void ARogueCharacter::SetMenuInteractionInput(bool bEnabled)
{
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
	bDead = true;
	SetUpgradeSelectionInput(true);

	// 通知所有武器角色死亡
	for (ARogueWeaponBase* Weapon : Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->OnOwnerDied();
		}
	}

	DisableInput(Cast<APlayerController>(GetController()));
	GetCharacterMovement()->DisableMovement();

	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->NotifyPlayerDied();
	}
}
