#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/RogueCharacterVisualComponent.h"
#include "Player/RogueWeaponConfig.h"
#include "Core/RogueTypes.h"
#include "RogueCharacter.generated.h"

class ARogueProjectile;
class ARogueRocketProjectile;
class ARogueLaserBeam;
class ARogueOrbitingBlade;
class ARogueEnemy;
class ARogueHUD;
class ARogueShopTerminal;
class UCameraComponent;
class USpringArmComponent;
class URogueCharacterVisualComponent;
class URoguePlayerBalanceAsset;
struct FRogueUpgradeEffectApplier;

UCLASS()
class AI_API ARogueCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARogueCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	void AddExperience(int32 ExperienceAmount);
	void AddMoney(int32 Amount);
	void ApplyUpgrade(const FRogueUpgradeOption& Upgrade);
	void SetMenuInteractionInput(bool bEnabled);
	void SetUpgradeSelectionInput(bool bEnabled) { SetMenuInteractionInput(bEnabled); }
	bool TrySpendMoney(int32 Amount);
	void SetNearbyShopTerminal(ARogueShopTerminal* ShopTerminal);
	void ClearNearbyShopTerminal(const ARogueShopTerminal* ShopTerminal);
	bool GetShopPromptWorldLocation(FVector& OutLocation) const;

	UFUNCTION(BlueprintPure)
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure)
	float GetExperiencePercent() const;

	UFUNCTION(BlueprintPure)
	float GetPickupRadius() const { return PickupRadius; }

	UFUNCTION(BlueprintPure)
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure)
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure)
	float GetCurrentArmor() const { return CurrentArmor; }

	UFUNCTION(BlueprintPure)
	float GetMaxArmor() const { return MaxArmor; }

	UFUNCTION(BlueprintPure)
	float GetArmorPercent() const { return MaxArmor > 0.0f ? CurrentArmor / MaxArmor : 0.0f; }

	UFUNCTION(BlueprintPure)
	bool HasArmor() const { return MaxArmor > 0.0f; }

	UFUNCTION(BlueprintPure)
	bool HasArmorUnlocked() const { return HasArmor(); }

	UFUNCTION(BlueprintPure)
	int32 GetPlayerLevel() const { return PlayerLevel; }

	UFUNCTION(BlueprintPure)
	int32 GetCurrentExperience() const { return CurrentExperience; }

	UFUNCTION(BlueprintPure)
	int32 GetExperienceToNextLevel() const { return ExperienceToNextLevel; }

	UFUNCTION(BlueprintPure)
	int32 GetMoney() const { return Money; }

	UFUNCTION(BlueprintPure)
	bool CanInteractWithShop() const { return NearbyShopTerminal.IsValid(); }

	UFUNCTION(BlueprintPure)
	float GetAttackDamage() const { return ProjectileWeapon.Damage; }

	UFUNCTION(BlueprintPure)
	float GetAttackInterval() const { return ProjectileWeapon.Interval; }

	UFUNCTION(BlueprintPure)
	int32 GetProjectileCount() const { return ProjectileWeapon.Count; }

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveProjectileCount() const;

	UFUNCTION(BlueprintPure)
	float GetDamageReductionPercent() const { return DamageReductionPercent; }

	UFUNCTION(BlueprintPure)
	float GetExperienceMultiplier() const { return ExperienceMultiplier; }

	UFUNCTION(BlueprintPure)
	int32 GetScytheCount() const { return ScytheWeapon.Count; }

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveScytheCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetRocketLauncherCount() const { return RocketWeapon.Count; }

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveRocketLauncherCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetLaserCannonCount() const { return LaserWeapon.Count; }

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveLaserCannonCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetHellTowerCount() const { return HellTowerWeapon.Count; }

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveHellTowerCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetLaserRefractionCount() const { return LaserWeapon.RefractionCount; }

	UFUNCTION(BlueprintPure)
	float GetDashCooldownDuration() const { return DashCooldownDuration; }

	UFUNCTION(BlueprintPure)
	float GetDashCooldownRemaining() const { return DashCooldownRemaining; }

	UFUNCTION(BlueprintPure)
	bool IsDashReady() const { return !bDashActive && DashCooldownRemaining <= 0.0f; }

	UFUNCTION(BlueprintPure)
	bool IsDead() const { return bDead; }

protected:
	virtual void BeginPlay() override;

private:
	friend struct FRogueUpgradeEffectApplier;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnCamera(float Value);
	void PitchCamera(float Value);
	void ZoomCamera(float Value);
	void TogglePauseMenu();
	void Interact();
	void Dash();
	void ConfirmUpgradeSelection();
	void ChooseUpgradeOne();
	void ChooseUpgradeTwo();
	void ChooseUpgradeThree();
	void HandleDash(float DeltaSeconds);
	void HandleArmorRecharge(float DeltaSeconds);
	void HandleAutoAttack(float DeltaSeconds);
	void HandleRecovery(float DeltaSeconds);
	void HandleRocketLaunchers(float DeltaSeconds);
	void HandleLaserCannons(float DeltaSeconds);
	void HandleHellTowers(float DeltaSeconds);
	void SyncOrbitingBlades();
	FVector GetDashDirection() const;
	void CollectEnemiesInRange(float Range, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults = 0) const;
	void FireAtTarget(AActor* TargetActor);
	void FireRocketVolley(const TArray<ARogueEnemy*>& Enemies);
	void FireLaserBurst(const TArray<ARogueEnemy*>& Enemies);
	void FireLaserRefractionChain(ARogueEnemy* InitialTarget, const FVector& InitialImpactLocation, float InitialDamage);
	ARogueEnemy* FindNearestEnemyFrom(const FVector& Origin, const TArray<TObjectPtr<ARogueEnemy>>& IgnoredEnemies, float MaxRange) const;
	void SpawnLaserBeam(const FVector& StartLocation, const FVector& EndLocation, bool bUseInfernoStyle = false, bool bSpawnImpactEffect = true, float BeamLifetime = 0.10f);
	void ApplySharedWeaponDamageBonus(float Magnitude);
	void ApplySharedWeaponSpeedBonus(float Magnitude);
	void ApplySharedWeaponRangeBonus(float Magnitude);
	void ApplyBalanceAsset();
	void UpdateVisualPresentation(float DeltaSeconds);
	void LevelUp();
	void Die();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URogueCharacterVisualComponent> Visuals;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSoftObjectPtr<URoguePlayerBalanceAsset> PlayerBalanceAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	FRogueProjectileWeaponConfig ProjectileWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	FRogueScytheWeaponConfig ScytheWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	FRogueRocketWeaponConfig RocketWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	FRogueLaserWeaponConfig LaserWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	FRogueHellTowerWeaponConfig HellTowerWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraTurnSpeed = 2.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraPitchSpeed = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraZoomStep = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MinCameraPitch = -75.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MaxCameraPitch = -35.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MinCameraDistance = 700.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MaxCameraDistance = 1400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Progression")
	int32 ExperienceToNextLevel = 6;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float MaxArmor = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float MoveSpeed = 650.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float PickupRadius = 260.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float HealthRegenPerSecond = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float DamageReductionPercent = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float ExperienceMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float ArmorRechargeDelay = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	float ArmorRechargeRate = 45.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Mobility")
	float DashCooldownDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Mobility")
	float DashMinCooldown = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Mobility")
	float DashDuration = 0.16f;

	UPROPERTY(EditDefaultsOnly, Category = "Mobility")
	float DashSpeed = 4200.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Stats")
	float CurrentArmor = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Progression")
	int32 CurrentExperience = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Progression")
	int32 PlayerLevel = 1;

	UPROPERTY()
	TObjectPtr<URoguePlayerBalanceAsset> LoadedPlayerBalanceAsset;

	UPROPERTY(VisibleInstanceOnly, Category = "Progression")
	int32 Money = 0;

	float ExperienceRemainder = 0.0f;
	float AttackTimer = 0.0f;
	float RocketTimer = 0.0f;
	float LaserTimer = 0.0f;
	float SharedScytheOrbitAngle = 0.0f;
	float DashCooldownRemaining = 0.0f;
	float DashTimeRemaining = 0.0f;
	float ArmorRechargeDelayRemaining = 0.0f;
	float HellTowerUpdateAccumulator = 0.0f;
	float ForwardInputValue = 0.0f;
	float RightInputValue = 0.0f;
	FVector DashDirection = FVector::ForwardVector;
	TArray<TWeakObjectPtr<ARogueEnemy>> HellTowerTargets;
	TArray<float> HellTowerCurrentDamages;
	TArray<float> HellTowerDamageTickTimers;
	TArray<float> HellTowerBeamTimers;
	TWeakObjectPtr<ARogueShopTerminal> NearbyShopTerminal;
	UPROPERTY()
	TArray<TObjectPtr<ARogueOrbitingBlade>> OrbitingBlades;
	bool bDashActive = false;
	bool bDead = false;
};
