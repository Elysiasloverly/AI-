#include "Player/RogueCharacter.h"

#include "Enemies/RogueEnemy.h"
#include "Core/RogueUpgradeEffectApplier.h"
#include "Core/RogueGameMode.h"
#include "Player/RoguePlayerBalanceAsset.h"
#include "World/RogueShopTerminal.h"
#include "UI/RogueHUD.h"
#include "Combat/RogueImpactEffect.h"
#include "Combat/RogueLaserBeam.h"
#include "Combat/RogueOrbitingBlade.h"
#include "Combat/RogueProjectile.h"
#include "Combat/RogueRocketProjectile.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"

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
	ProjectileWeapon.ProjectileClass = ARogueProjectile::StaticClass();
	RocketWeapon.ProjectileClass = ARogueRocketProjectile::StaticClass();
	LaserWeapon.BeamClass = ARogueLaserBeam::StaticClass();
	ScytheWeapon.BladeClass = ARogueOrbitingBlade::StaticClass();

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
}

void ARogueCharacter::ApplyBalanceAsset()
{
	LoadedPlayerBalanceAsset = PlayerBalanceAsset.IsNull() ? nullptr : PlayerBalanceAsset.LoadSynchronous();
	if (LoadedPlayerBalanceAsset == nullptr)
	{
		return;
	}

	const FRoguePlayerBaseStatConfig& BaseStats = LoadedPlayerBalanceAsset->BaseStats;
	ProjectileWeapon = LoadedPlayerBalanceAsset->ProjectileWeapon;
	ScytheWeapon = LoadedPlayerBalanceAsset->ScytheWeapon;
	RocketWeapon = LoadedPlayerBalanceAsset->RocketWeapon;
	LaserWeapon = LoadedPlayerBalanceAsset->LaserWeapon;
	HellTowerWeapon = LoadedPlayerBalanceAsset->HellTowerWeapon;
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

	if (GetEffectiveScytheCount() > 0)
	{
		SharedScytheOrbitAngle = FMath::Fmod(SharedScytheOrbitAngle + ScytheWeapon.RotationSpeed * DeltaSeconds, 360.0f);
		if (SharedScytheOrbitAngle < 0.0f)
		{
			SharedScytheOrbitAngle += 360.0f;
		}
	}

	SyncOrbitingBlades();
	HandleAutoAttack(DeltaSeconds);
	HandleRocketLaunchers(DeltaSeconds);
	HandleLaserCannons(DeltaSeconds);
	HandleHellTowers(DeltaSeconds);
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
	PlayerInputComponent->BindAction(TEXT("Dash"), IE_Pressed, this, &ARogueCharacter::Dash);

	FInputActionBinding& ConfirmUpgradeBinding = PlayerInputComponent->BindAction(TEXT("ConfirmUpgradeSelection"), IE_Pressed, this, &ARogueCharacter::ConfirmUpgradeSelection);
	ConfirmUpgradeBinding.bExecuteWhenPaused = true;

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

	while (CurrentExperience >= ExperienceToNextLevel)
	{
		CurrentExperience -= ExperienceToNextLevel;
		LevelUp();
		break;
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

int32 ARogueCharacter::GetEffectiveProjectileCount() const
{
	return FMath::Max(1, ProjectileWeapon.Count);
}

int32 ARogueCharacter::GetEffectiveScytheCount() const
{
	return FMath::Max(0, ScytheWeapon.Count);
}

int32 ARogueCharacter::GetEffectiveRocketLauncherCount() const
{
	return FMath::Max(0, RocketWeapon.Count);
}

int32 ARogueCharacter::GetEffectiveLaserCannonCount() const
{
	return FMath::Max(0, LaserWeapon.Count);
}

int32 ARogueCharacter::GetEffectiveHellTowerCount() const
{
	return FMath::Max(0, HellTowerWeapon.Count);
}

void ARogueCharacter::ApplySharedWeaponDamageBonus(float Magnitude)
{
	ProjectileWeapon.Damage += Magnitude;
	ScytheWeapon.Damage += Magnitude;
	RocketWeapon.Damage += Magnitude;
	LaserWeapon.Damage += Magnitude;
	HellTowerWeapon.BaseDamage += Magnitude * 0.125f;
	HellTowerWeapon.DamageRampPerTick += Magnitude * 0.0625f;
}

void ARogueCharacter::ApplySharedWeaponSpeedBonus(float Magnitude)
{
	ProjectileWeapon.Interval = FMath::Max(0.15f, ProjectileWeapon.Interval - Magnitude);
	RocketWeapon.Cooldown = FMath::Max(0.55f, RocketWeapon.Cooldown - Magnitude * 2.5f);
	LaserWeapon.Cooldown = FMath::Max(0.35f, LaserWeapon.Cooldown - Magnitude * 2.0f);
	HellTowerWeapon.DamageTickInterval = FMath::Max(0.03f, HellTowerWeapon.DamageTickInterval - Magnitude * 0.08f);
	ScytheWeapon.RotationSpeed += Magnitude * 280.0f;
}

void ARogueCharacter::ApplySharedWeaponRangeBonus(float Magnitude)
{
	ProjectileWeapon.Range += Magnitude;
	LaserWeapon.Range += Magnitude;
	LaserWeapon.RefractionRange += Magnitude * 0.45f;
	RocketWeapon.ExplosionRadius += Magnitude * 0.25f;
	HellTowerWeapon.Range += Magnitude * 0.70f;
	ScytheWeapon.OrbitRadius += Magnitude * 0.12f;
}

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

void ARogueCharacter::ConfirmUpgradeSelection()
{
	ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>();
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!IsValid(RogueGameMode) || !RogueGameMode->IsAwaitingUpgradeChoice() || PlayerController == nullptr)
	{
		return;
	}

	ARogueHUD* RogueHUD = Cast<ARogueHUD>(PlayerController->GetHUD());
	if (!IsValid(RogueHUD))
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	const int32 UpgradeIndex = RogueHUD->GetUpgradeIndexAtScreenPosition(MouseX, MouseY);
	if (UpgradeIndex != INDEX_NONE)
	{
		RogueGameMode->TrySelectUpgrade(UpgradeIndex);
	}
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

void ARogueCharacter::HandleAutoAttack(float DeltaSeconds)
{
	AttackTimer -= DeltaSeconds;
	if (AttackTimer > 0.0f)
	{
		return;
	}

	ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	if (RogueGameMode == nullptr)
	{
		return;
	}

	TArray<TObjectPtr<ARogueEnemy>> IgnoredEnemies;
	AActor* ClosestEnemy = RogueGameMode->FindNearestEnemyInRange(GetActorLocation(), ProjectileWeapon.Range, IgnoredEnemies);
	if (ClosestEnemy != nullptr)
	{
		FireAtTarget(ClosestEnemy);
		AttackTimer = ProjectileWeapon.Interval;
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

void ARogueCharacter::SyncOrbitingBlades()
{
	const int32 TargetBladeCount = GetEffectiveScytheCount();

	for (int32 Index = OrbitingBlades.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(OrbitingBlades[Index]) || OrbitingBlades[Index]->IsAvailableInPool())
		{
			OrbitingBlades.RemoveAt(Index);
		}
	}

	while (OrbitingBlades.Num() < TargetBladeCount)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.Instigator = this;

		UClass* BladeClassToSpawn = ScytheWeapon.BladeClass ? ScytheWeapon.BladeClass.Get() : ARogueOrbitingBlade::StaticClass();
		ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
		ARogueOrbitingBlade* Blade = RogueGameMode != nullptr
			? RogueGameMode->AcquireOrbitingBlade(BladeClassToSpawn, this, GetActorLocation(), FRotator::ZeroRotator)
			: GetWorld()->SpawnActor<ARogueOrbitingBlade>(BladeClassToSpawn, GetActorLocation(), FRotator::ZeroRotator, SpawnParameters);
		if (Blade != nullptr)
		{
			Blade->ActivatePooledBlade(this);
			OrbitingBlades.Add(Blade);
		}
		else
		{
			break;
		}
	}

	while (OrbitingBlades.Num() > TargetBladeCount)
	{
		if (ARogueOrbitingBlade* Blade = OrbitingBlades.Pop())
		{
			Blade->DeactivateToPool();
		}
	}

	for (int32 Index = 0; Index < OrbitingBlades.Num(); ++Index)
	{
		if (IsValid(OrbitingBlades[Index]))
		{
			OrbitingBlades[Index]->ConfigureBlade(this, Index, OrbitingBlades.Num(), ScytheWeapon.OrbitRadius, ScytheWeapon.RotationSpeed, ScytheWeapon.Damage, SharedScytheOrbitAngle);
		}
	}
}

void ARogueCharacter::CollectEnemiesInRange(float Range, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults) const
{
	ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	if (RogueGameMode == nullptr)
	{
		OutEnemies.Reset();
		return;
	}

	RogueGameMode->CollectEnemiesInRange(GetActorLocation(), Range, OutEnemies, MaxResults, true);
}

void ARogueCharacter::HandleRocketLaunchers(float DeltaSeconds)
{
	if (GetEffectiveRocketLauncherCount() <= 0)
	{
		return;
	}

	RocketTimer -= DeltaSeconds;
	if (RocketTimer > 0.0f)
	{
		return;
	}

	TArray<ARogueEnemy*> Enemies;
	CollectEnemiesInRange(ProjectileWeapon.Range + 700.0f, Enemies, FMath::Max(12, GetEffectiveRocketLauncherCount() * 3));
	if (Enemies.Num() == 0)
	{
		return;
	}

	FireRocketVolley(Enemies);
	RocketTimer = RocketWeapon.Cooldown;
}

void ARogueCharacter::HandleLaserCannons(float DeltaSeconds)
{
	if (GetEffectiveLaserCannonCount() <= 0)
	{
		return;
	}

	LaserTimer -= DeltaSeconds;
	if (LaserTimer > 0.0f)
	{
		return;
	}

	TArray<ARogueEnemy*> Enemies;
	CollectEnemiesInRange(LaserWeapon.Range, Enemies, FMath::Max(8, GetEffectiveLaserCannonCount() * 3));
	if (Enemies.Num() == 0)
	{
		return;
	}

	FireLaserBurst(Enemies);
	LaserTimer = LaserWeapon.Cooldown;
}

void ARogueCharacter::HandleHellTowers(float DeltaSeconds)
{
	const int32 EffectiveHellTowerCount = GetEffectiveHellTowerCount();
	if (EffectiveHellTowerCount <= 0)
	{
		HellTowerUpdateAccumulator = 0.0f;
		HellTowerTargets.Reset();
		HellTowerCurrentDamages.Reset();
		HellTowerDamageTickTimers.Reset();
		HellTowerBeamTimers.Reset();
		return;
	}

	ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	const bool bHeavyCombat = RogueGameMode != nullptr && (RogueGameMode->ShouldCullCombatEffects() || RogueGameMode->GetActiveEnemyCount() >= 40);
	const float UpdateStep = bHeavyCombat ? HellTowerWeapon.HeavyCombatUpdateInterval : HellTowerWeapon.UpdateInterval;
	HellTowerUpdateAccumulator += DeltaSeconds;
	if (HellTowerUpdateAccumulator < UpdateStep)
	{
		return;
	}

	const float SimulatedDeltaSeconds = FMath::Min(HellTowerUpdateAccumulator, UpdateStep * 2.0f);
	HellTowerUpdateAccumulator = 0.0f;

	HellTowerTargets.SetNum(EffectiveHellTowerCount);
	HellTowerCurrentDamages.SetNum(EffectiveHellTowerCount);
	HellTowerDamageTickTimers.SetNum(EffectiveHellTowerCount);
	HellTowerBeamTimers.SetNum(EffectiveHellTowerCount);

	const FVector PlayerLocation = GetActorLocation();
	const float HellTowerRangeSquared = FMath::Square(HellTowerWeapon.Range);
	TArray<TObjectPtr<ARogueEnemy>> LockedEnemies;
	LockedEnemies.Reserve(EffectiveHellTowerCount);

	for (int32 TowerIndex = 0; TowerIndex < EffectiveHellTowerCount; ++TowerIndex)
	{
		ARogueEnemy* CurrentTarget = HellTowerTargets[TowerIndex].Get();
		const bool bHasValidTarget =
			IsValid(CurrentTarget) &&
			!CurrentTarget->IsDead() &&
			!CurrentTarget->IsAvailableInPool() &&
			!CurrentTarget->IsActorBeingDestroyed() &&
			FVector::DistSquared2D(PlayerLocation, CurrentTarget->GetActorLocation()) <= HellTowerRangeSquared;

		if (!bHasValidTarget)
		{
			HellTowerTargets[TowerIndex] = nullptr;
			HellTowerCurrentDamages[TowerIndex] = FMath::Max(1.0f, HellTowerWeapon.BaseDamage);
			HellTowerDamageTickTimers[TowerIndex] = 0.0f;
			HellTowerBeamTimers[TowerIndex] = 0.0f;
		}
		else
		{
			HellTowerCurrentDamages[TowerIndex] = FMath::Max(HellTowerCurrentDamages[TowerIndex], FMath::Max(1.0f, HellTowerWeapon.BaseDamage));
			LockedEnemies.Add(CurrentTarget);
		}
	}

	for (int32 TowerIndex = 0; TowerIndex < EffectiveHellTowerCount; ++TowerIndex)
	{
		if (HellTowerTargets[TowerIndex].IsValid())
		{
			continue;
		}

		ARogueEnemy* NewTarget = FindNearestEnemyFrom(PlayerLocation, LockedEnemies, HellTowerWeapon.Range);
		if (!IsValid(NewTarget))
		{
			continue;
		}

		HellTowerTargets[TowerIndex] = NewTarget;
		HellTowerCurrentDamages[TowerIndex] = FMath::Max(1.0f, HellTowerWeapon.BaseDamage);
		HellTowerDamageTickTimers[TowerIndex] = 0.0f;
		HellTowerBeamTimers[TowerIndex] = 0.0f;
		LockedEnemies.Add(NewTarget);
	}

	FVector LateralDirection = Camera != nullptr ? Camera->GetRightVector() : GetActorRightVector();
	LateralDirection.Z = 0.0f;
	if (!LateralDirection.Normalize())
	{
		LateralDirection = FVector::RightVector;
	}

	const int32 TowerHalfCount = EffectiveHellTowerCount / 2;
	const float BeamSpacing = 18.0f;
	const FVector BeamOriginBase = PlayerLocation + FVector(0.0f, 0.0f, 62.0f);
	const float EffectiveBeamRefreshInterval = HellTowerWeapon.BeamRefreshInterval + (bHeavyCombat ? HellTowerWeapon.HeavyCombatBeamRefreshPenalty : 0.0f);

	for (int32 TowerIndex = 0; TowerIndex < EffectiveHellTowerCount; ++TowerIndex)
	{
		ARogueEnemy* TargetEnemy = HellTowerTargets[TowerIndex].Get();
		if (!IsValid(TargetEnemy) || TargetEnemy->IsDead() || TargetEnemy->IsAvailableInPool() || TargetEnemy->IsActorBeingDestroyed())
		{
			continue;
		}

		HellTowerDamageTickTimers[TowerIndex] += SimulatedDeltaSeconds;
		while (HellTowerDamageTickTimers[TowerIndex] >= HellTowerWeapon.DamageTickInterval)
		{
			HellTowerDamageTickTimers[TowerIndex] -= HellTowerWeapon.DamageTickInterval;
			UGameplayStatics::ApplyDamage(TargetEnemy, HellTowerCurrentDamages[TowerIndex], nullptr, this, UDamageType::StaticClass());
			HellTowerCurrentDamages[TowerIndex] += HellTowerWeapon.DamageRampPerTick;
		}

		HellTowerBeamTimers[TowerIndex] -= SimulatedDeltaSeconds;
		if (HellTowerBeamTimers[TowerIndex] > 0.0f)
		{
			continue;
		}

		const float OffsetIndex = static_cast<float>(TowerIndex - TowerHalfCount);
		const float LateralOffset = EffectiveHellTowerCount % 2 == 0 ? (OffsetIndex + 0.5f) * BeamSpacing : OffsetIndex * BeamSpacing;
		const FVector BeamOrigin = BeamOriginBase + LateralDirection * LateralOffset;
		const FVector TargetLocation = TargetEnemy->GetActorLocation() + FVector(0.0f, 0.0f, 42.0f);
		SpawnLaserBeam(BeamOrigin, TargetLocation, true, false, EffectiveBeamRefreshInterval + 0.03f);
		HellTowerBeamTimers[TowerIndex] = EffectiveBeamRefreshInterval;
	}
}

void ARogueCharacter::FireAtTarget(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	const FVector AimOrigin = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	FVector Direction = TargetActor->GetActorLocation() - AimOrigin;
	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		Direction = GetActorForwardVector().GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = FVector::ForwardVector;
		}
	}
	const float SpreadStepDegrees = 9.0f;
	const int32 EffectiveProjectileCount = GetEffectiveProjectileCount();
	const int32 ProjectileHalfCount = EffectiveProjectileCount / 2;

	for (int32 ProjectileIndex = 0; ProjectileIndex < EffectiveProjectileCount; ++ProjectileIndex)
	{
		const float OffsetIndex = static_cast<float>(ProjectileIndex - ProjectileHalfCount);
		const float SpreadOffset = EffectiveProjectileCount % 2 == 0 ? (OffsetIndex + 0.5f) * SpreadStepDegrees : OffsetIndex * SpreadStepDegrees;
		const FVector ShotDirection = Direction.RotateAngleAxis(SpreadOffset, FVector::UpVector).GetSafeNormal();
		const FVector SpawnLocation = AimOrigin + ShotDirection * 100.0f;

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.Instigator = this;

		UClass* ProjectileToSpawn = ProjectileWeapon.ProjectileClass ? ProjectileWeapon.ProjectileClass.Get() : ARogueProjectile::StaticClass();
		ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
		ARogueProjectile* Projectile = RogueGameMode != nullptr
			? RogueGameMode->AcquirePlayerProjectile(ProjectileToSpawn, this, this, SpawnLocation, ShotDirection.Rotation())
			: GetWorld()->SpawnActor<ARogueProjectile>(ProjectileToSpawn, SpawnLocation, ShotDirection.Rotation(), SpawnParameters);
		if (Projectile != nullptr)
		{
			Projectile->ActivatePooledProjectile(this, this, SpawnLocation, ShotDirection.Rotation(), ShotDirection, ProjectileWeapon.Speed, ProjectileWeapon.Damage);
		}
	}
}

void ARogueCharacter::FireRocketVolley(const TArray<ARogueEnemy*>& Enemies)
{
	const FVector LaunchOrigin = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
	const int32 EffectiveRocketCount = GetEffectiveRocketLauncherCount();

	for (int32 RocketIndex = 0; RocketIndex < EffectiveRocketCount; ++RocketIndex)
	{
		if (!Enemies.IsValidIndex(RocketIndex % Enemies.Num()) || !IsValid(Enemies[RocketIndex % Enemies.Num()]))
		{
			continue;
		}

		ARogueEnemy* TargetEnemy = Enemies[RocketIndex % Enemies.Num()];
		const FVector Direction = (TargetEnemy->GetActorLocation() - LaunchOrigin).GetSafeNormal();
		const FVector SpawnLocation = LaunchOrigin + Direction * 90.0f + FVector(0.0f, 0.0f, RocketIndex * 8.0f);

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.Instigator = this;

		UClass* RocketClassToSpawn = RocketWeapon.ProjectileClass ? RocketWeapon.ProjectileClass.Get() : ARogueRocketProjectile::StaticClass();
		ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
		ARogueRocketProjectile* Rocket = RogueGameMode != nullptr
			? RogueGameMode->AcquireRocketProjectile(RocketClassToSpawn, this, this, SpawnLocation, Direction.Rotation())
			: GetWorld()->SpawnActor<ARogueRocketProjectile>(RocketClassToSpawn, SpawnLocation, Direction.Rotation(), SpawnParameters);
		if (Rocket != nullptr)
		{
			Rocket->ActivatePooledRocket(this, this, SpawnLocation, Direction.Rotation(), Direction, RocketWeapon.Speed, RocketWeapon.Damage, RocketWeapon.ExplosionRadius);
		}
	}
}

void ARogueCharacter::FireLaserBurst(const TArray<ARogueEnemy*>& Enemies)
{
	const FVector BeamOriginBase = GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
	const int32 EffectiveLaserCount = GetEffectiveLaserCannonCount();
	FVector LateralDirection = Camera != nullptr ? Camera->GetRightVector() : GetActorRightVector();
	LateralDirection.Z = 0.0f;
	if (!LateralDirection.Normalize())
	{
		LateralDirection = FVector::RightVector;
	}

	const int32 LaserHalfCount = EffectiveLaserCount / 2;
	const float BeamSpacing = 34.0f;

	for (int32 LaserIndex = 0; LaserIndex < EffectiveLaserCount; ++LaserIndex)
	{
		if (!Enemies.IsValidIndex(LaserIndex % Enemies.Num()) || !IsValid(Enemies[LaserIndex % Enemies.Num()]))
		{
			continue;
		}

		ARogueEnemy* TargetEnemy = Enemies[LaserIndex % Enemies.Num()];
		const float OffsetIndex = static_cast<float>(LaserIndex - LaserHalfCount);
		const float LateralOffset = EffectiveLaserCount % 2 == 0 ? (OffsetIndex + 0.5f) * BeamSpacing : OffsetIndex * BeamSpacing;
		const FVector BeamOrigin = BeamOriginBase + LateralDirection * LateralOffset;
		const FVector TargetLocation = TargetEnemy->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		UGameplayStatics::ApplyDamage(TargetEnemy, LaserWeapon.Damage, nullptr, this, UDamageType::StaticClass());
		SpawnLaserBeam(BeamOrigin, TargetLocation);
		if (LaserWeapon.RefractionCount > 0)
		{
			FireLaserRefractionChain(TargetEnemy, TargetLocation, LaserWeapon.Damage * 0.5f);
		}
	}
}

void ARogueCharacter::FireLaserRefractionChain(ARogueEnemy* InitialTarget, const FVector& InitialImpactLocation, float InitialDamage)
{
	if (!IsValid(InitialTarget) || LaserWeapon.RefractionCount <= 0 || InitialDamage <= 0.0f)
	{
		return;
	}

	TArray<TObjectPtr<ARogueEnemy>> HitEnemies;
	HitEnemies.Add(InitialTarget);

	ARogueEnemy* CurrentTarget = InitialTarget;
	FVector CurrentStartLocation = InitialImpactLocation;
	float CurrentDamage = InitialDamage;

	for (int32 RefractionIndex = 0; RefractionIndex < LaserWeapon.RefractionCount; ++RefractionIndex)
	{
		if (!IsValid(CurrentTarget) || CurrentDamage <= 1.0f)
		{
			break;
		}

		ARogueEnemy* NextTarget = FindNearestEnemyFrom(CurrentTarget->GetActorLocation(), HitEnemies, LaserWeapon.RefractionRange);
		if (!IsValid(NextTarget))
		{
			break;
		}

		const FVector NextImpactLocation = NextTarget->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		UGameplayStatics::ApplyDamage(NextTarget, CurrentDamage, nullptr, this, UDamageType::StaticClass());
		SpawnLaserBeam(CurrentStartLocation, NextImpactLocation);

		HitEnemies.Add(NextTarget);
		CurrentTarget = NextTarget;
		CurrentStartLocation = NextImpactLocation;
	}
}

ARogueEnemy* ARogueCharacter::FindNearestEnemyFrom(const FVector& Origin, const TArray<TObjectPtr<ARogueEnemy>>& IgnoredEnemies, float MaxRange) const
{
	ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	if (RogueGameMode == nullptr)
	{
		return nullptr;
	}

	return RogueGameMode->FindNearestEnemyInRange(Origin, MaxRange, IgnoredEnemies);
}

void ARogueCharacter::SpawnLaserBeam(const FVector& StartLocation, const FVector& EndLocation, bool bUseInfernoStyle, bool bSpawnImpactEffect, float BeamLifetime)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;

	UClass* BeamClassToSpawn = LaserWeapon.BeamClass ? LaserWeapon.BeamClass.Get() : ARogueLaserBeam::StaticClass();
	ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	ARogueLaserBeam* Beam = RogueGameMode != nullptr
		? RogueGameMode->AcquireLaserBeam(BeamClassToSpawn, this, StartLocation, FRotator::ZeroRotator)
		: GetWorld()->SpawnActor<ARogueLaserBeam>(BeamClassToSpawn, StartLocation, FRotator::ZeroRotator, SpawnParameters);
	if (Beam != nullptr)
	{
		Beam->ActivatePooledBeam(this, StartLocation, EndLocation, bUseInfernoStyle, bSpawnImpactEffect, BeamLifetime);
	}
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
	for (ARogueOrbitingBlade* Blade : OrbitingBlades)
	{
		if (IsValid(Blade))
		{
			Blade->DeactivateToPool();
		}
	}
	OrbitingBlades.Reset();
	HellTowerTargets.Reset();
	HellTowerCurrentDamages.Reset();
	HellTowerDamageTickTimers.Reset();
	HellTowerBeamTimers.Reset();
	DisableInput(Cast<APlayerController>(GetController()));
	GetCharacterMovement()->DisableMovement();

	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->NotifyPlayerDied();
	}
}
