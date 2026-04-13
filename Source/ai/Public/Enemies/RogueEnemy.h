#pragma once

#include "CoreMinimal.h"
#include "Enemies/RogueEnemyArchetypes.h"
#include "Enemies/RogueEnemyVisualResources.h"
#include "GameFramework/Character.h"
#include "Core/RogueTypes.h"
#include "RogueEnemy.generated.h"

class UStaticMeshComponent;
class ARogueCharacter;

UCLASS()
class AI_API ARogueEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	ARogueEnemy();

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	void InitializeEnemy(const FRogueEnemyProfile& InProfile);
	void ActivatePooledEnemy(AActor* InOwner, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

	UFUNCTION(BlueprintPure)
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure)
	int32 GetExperienceReward() const { return ExperienceReward; }

	UFUNCTION(BlueprintPure)
	bool IsBoss() const { return bIsBoss; }

	UFUNCTION(BlueprintPure)
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure)
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure)
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintPure)
	FVector GetHealthBarWorldLocation() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void TouchPlayer(float DeltaSeconds);
	void HandleRangedAttack(ARogueCharacter* PlayerCharacter, float DistanceToPlayer, float DeltaSeconds);
	void FireRangedShot(ARogueCharacter* PlayerCharacter);
	void Die(AActor* Killer);
	void ApplyEnemyStyle();
	FVector GetMovementDirection(const FVector& ToPlayer, float DistanceToPlayer, float DeltaSeconds);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float MaxHealth = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float CurrentHealth = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float MoveSpeed = 260.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float ContactDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy")
	float ContactInterval = 1.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Enemy")
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(VisibleInstanceOnly, Category = "Enemy")
	int32 ExperienceReward = 1;

	float BurstCooldown = 0.0f;
	float BurstTimeRemaining = 0.0f;
	float OrbitDirection = 1.0f;
	float ContactTimer = 0.0f;
	float RangedAttackTimer = 0.0f;
	float LastDamageNumberTime = -100.0f;
	TWeakObjectPtr<ARogueCharacter> CachedPlayerCharacter;
	FRogueEnemyArchetype CurrentArchetype;
	FRogueEnemyVisualResourceLibrary VisualResourceLibrary;
	bool bPoolAvailable = true;
	bool bDead = false;
	bool bIsBoss = false;
};
