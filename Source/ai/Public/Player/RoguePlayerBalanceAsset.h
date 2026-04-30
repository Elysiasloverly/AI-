#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RoguePlayerBalanceAsset.generated.h"

USTRUCT(BlueprintType)
struct AI_API FRoguePlayerBaseStatConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "成长", meta = (DisplayName = "升级所需经验"))
	int32 ExperienceToNextLevel = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "最大生命值"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "最大护甲值"))
	float MaxArmor = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "移动速度"))
	float MoveSpeed = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "拾取范围"))
	float PickupRadius = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "每秒生命恢复"))
	float HealthRegenPerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "伤害减免百分比"))
	float DamageReductionPercent = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "经验获取倍率"))
	float ExperienceMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "护甲", meta = (DisplayName = "护甲开始恢复延迟"))
	float ArmorRechargeDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "护甲", meta = (DisplayName = "护甲恢复速度"))
	float ArmorRechargeRate = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "机动", meta = (DisplayName = "冲刺冷却时间"))
	float DashCooldownDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "机动", meta = (DisplayName = "冲刺最小冷却"))
	float DashMinCooldown = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "机动", meta = (DisplayName = "冲刺持续时间"))
	float DashDuration = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "机动", meta = (DisplayName = "冲刺速度"))
	float DashSpeed = 4200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "跳跃初速度"))
	float JumpZVelocity = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "空中控制力"))
	float AirControl = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "重力倍率"))
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "最大跳跃次数", ClampMin = "0", UIMin = "0"))
	int32 JumpMaxCount = 1;
};

UCLASS(BlueprintType)
class AI_API URoguePlayerBalanceAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URoguePlayerBalanceAsset();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "属性", meta = (DisplayName = "恢复默认玩家数值"))
	void ResetToDefaults();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "基础属性配置"))
	FRoguePlayerBaseStatConfig BaseStats;

	// 武器配置已迁移到 DataTable（DT_WeaponConfig），此处不再包含武器字段
};
