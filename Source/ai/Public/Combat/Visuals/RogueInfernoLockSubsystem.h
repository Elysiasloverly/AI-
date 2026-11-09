#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RogueInfernoLockSubsystem.generated.h"
class ARogueEnemy;
class ARogueInfernoLockRing;

/** One local ring per enemy generation; concurrent player locks share the same ring. */
UCLASS()
class AI_API URogueInfernoLockSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
  public:
	void Touch(AActor* Source, ARogueEnemy* Target, uint32 Generation, float LockedSeconds,
			   TSubclassOf<ARogueInfernoLockRing> RingClass, float LeaseSeconds);
	void Release(AActor* Source, ARogueEnemy* Target, uint32 Generation);
	void RemoveSource(AActor* Source);
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override
	{
		return !IsTemplate() && Entries.Num() > 0;
	}
	virtual TStatId GetStatId() const override;
	UFUNCTION(BlueprintPure, Category = "锁定圈") int32 GetActiveRingCount() const;
	UFUNCTION(BlueprintPure, Category = "锁定圈") float GetLockProgress(ARogueEnemy* Target) const;

  private:
	struct FSource
	{
		float Age = 0;
		double Seen = 0;
		float Lease = .65f;
	};
	struct FEntry
	{
		uint32 Generation = 0;
		TSubclassOf<ARogueInfernoLockRing> RingClass;
		TWeakObjectPtr<ARogueInfernoLockRing> Ring;
		TMap<TWeakObjectPtr<AActor>, FSource> Sources;
		float Age = 0;
		double UpdatedAt = 0;
	};
	TMap<TWeakObjectPtr<ARogueEnemy>, FEntry> Entries;
	UPROPERTY() TArray<TObjectPtr<ARogueInfernoLockRing>> Pool;
	ARogueInfernoLockRing* Acquire(TSubclassOf<ARogueInfernoLockRing> Class);
	void Return(ARogueInfernoLockRing* Ring);
	float Accumulator = 0;
};
