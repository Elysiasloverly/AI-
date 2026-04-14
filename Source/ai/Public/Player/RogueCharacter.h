#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Player/RogueCharacterVisualComponent.h"
#include "Core/RogueTypes.h"
#include "RogueCharacter.generated.h"

class ARogueWeaponBase;
class ARogueWeapon_Projectile;
class ARogueWeapon_Laser;
class ARogueWeapon_Mortar;
class ARogueEnemy;
class ARogueHUD;
class ARogueRocketProjectile;
class ARogueShopTerminal;
class UCameraComponent;
class USpringArmComponent;
class UDataTable;
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

	// ---- 武器系统接口 ----

	/** 获取所有武器 Actor */
	const TArray<TObjectPtr<ARogueWeaponBase>>& GetWeapons() const { return Weapons; }

	/** 按类型查找武器 */
	template<typename T>
	T* FindWeapon() const
	{
		for (ARogueWeaponBase* Weapon : Weapons)
		{
			if (T* Typed = Cast<T>(Weapon))
			{
				return Typed;
			}
		}
		return nullptr;
	}

	/** 全局武器伤害加成 */
	void ApplySharedWeaponDamageBonus(float Magnitude);

	/** 全局武器攻速加成 */
	void ApplySharedWeaponSpeedBonus(float Magnitude);

	/** 全局武器范围加成 */
	void ApplySharedWeaponRangeBonus(float Magnitude);

	/** 将武器专属升级分发给所有武器 */
	bool DispatchWeaponUpgrade(ERogueUpgradeType UpgradeType, float Magnitude);

	// ---- 角色属性查询 ----

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
	float GetDamageReductionPercent() const { return DamageReductionPercent; }

	UFUNCTION(BlueprintPure)
	float GetExperienceMultiplier() const { return ExperienceMultiplier; }

	UFUNCTION(BlueprintPure)
	float GetDashCooldownDuration() const { return DashCooldownDuration; }

	UFUNCTION(BlueprintPure)
	float GetDashCooldownRemaining() const { return DashCooldownRemaining; }

	UFUNCTION(BlueprintPure)
	bool IsDashReady() const { return !bDashActive && DashCooldownRemaining <= 0.0f; }

	UFUNCTION(BlueprintPure)
	bool IsDead() const { return bDead; }

	// ---- 向后兼容的武器查询（供 HUD / 升级规则使用） ----

	UFUNCTION(BlueprintPure)
	float GetAttackDamage() const;

	UFUNCTION(BlueprintPure)
	float GetAttackInterval() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveProjectileCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveScytheCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveRocketLauncherCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveLaserCannonCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveHellTowerCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetEffectiveMortarCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetLaserRefractionCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetScytheCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetRocketLauncherCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetLaserCannonCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetHellTowerCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetMortarCount() const;

	UFUNCTION(BlueprintPure)
	int32 GetProjectileCount() const;

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
	void HandleRecovery(float DeltaSeconds);
	FVector GetDashDirection() const;
	void ApplyBalanceAsset();
	void InitializeWeapons();
	void TickWeapons(float DeltaSeconds);
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

	/** 武器配置数据表 —— 每行对应一种武器 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TObjectPtr<UDataTable> WeaponDataTable;

	/** 武器子类列表 —— 旧资源仍按顺序使用，缺失项会由代码默认武器补齐 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TArray<TSubclassOf<ARogueWeaponBase>> WeaponClasses;

	/** 当 DataTable / WeaponClasses 里还没正式接入迫击炮时，代码兜底生成用的武器类 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Fallback")
	TSubclassOf<ARogueWeaponBase> DefaultMortarWeaponClass;

	/** 当 DataTable 里还没正式填入迫击炮弹体时，代码兜底生成用的迫击炮弹体类 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Fallback")
	TSubclassOf<ARogueRocketProjectile> DefaultMortarProjectileClass;

	/** 运行时生成的武器 Actor 实例 */
	UPROPERTY()
	TArray<TObjectPtr<ARogueWeaponBase>> Weapons;

	UPROPERTY()
	TObjectPtr<URoguePlayerBalanceAsset> LoadedPlayerBalanceAsset;

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

	UPROPERTY(VisibleInstanceOnly, Category = "Progression")
	int32 Money = 0;

	float ExperienceRemainder = 0.0f;
	float DashCooldownRemaining = 0.0f;
	float DashTimeRemaining = 0.0f;
	float ArmorRechargeDelayRemaining = 0.0f;
	float ForwardInputValue = 0.0f;
	float RightInputValue = 0.0f;
	FVector DashDirection = FVector::ForwardVector;
	TWeakObjectPtr<ARogueShopTerminal> NearbyShopTerminal;
	bool bDashActive = false;
	bool bDead = false;
};
