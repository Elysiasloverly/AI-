#pragma once

#include "CoreMinimal.h"
#include "RogueHUDViewData.generated.h"

/** 本地玩家状态条（生命 / 护甲 / 经验）的视图数据。
 *  HUD 每帧组装，WBP 只读；字段变化才触发蓝图事件。 */
USTRUCT(BlueprintType)
struct FRoguePlayerVitalsView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	bool bVisible = false;

	/** 没有护甲上限时不显示护甲条。 */
	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	bool bHasArmor = false;

	/** 濒死期间仍显示血条（为 0），WBP 可据此换色或加提示。 */
	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	bool bDowned = false;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float HealthPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float ArmorPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float ExperiencePercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float MaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float CurrentArmor = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	float MaxArmor = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	int32 Experience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	int32 ExperienceToNextLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	int32 Money = 0;

	/** 当前波数，来源于 GameState（客户端可读）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Vitals")
	int32 CurrentWave = 0;

	/** 用于 HUD 侧脏检查；浮点按容差比较。 */
	bool Equals(const FRoguePlayerVitalsView& Other) const
	{
		return bVisible == Other.bVisible
			&& bHasArmor == Other.bHasArmor
			&& bDowned == Other.bDowned
			&& FMath::IsNearlyEqual(HealthPercent, Other.HealthPercent, 0.0005f)
			&& FMath::IsNearlyEqual(ArmorPercent, Other.ArmorPercent, 0.0005f)
			&& FMath::IsNearlyEqual(ExperiencePercent, Other.ExperiencePercent, 0.0005f)
			&& FMath::IsNearlyEqual(CurrentHealth, Other.CurrentHealth, 0.01f)
			&& FMath::IsNearlyEqual(MaxHealth, Other.MaxHealth, 0.01f)
			&& FMath::IsNearlyEqual(CurrentArmor, Other.CurrentArmor, 0.01f)
			&& FMath::IsNearlyEqual(MaxArmor, Other.MaxArmor, 0.01f)
			&& Level == Other.Level
			&& Experience == Other.Experience
			&& ExperienceToNextLevel == Other.ExperienceToNextLevel
			&& Money == Other.Money
			&& CurrentWave == Other.CurrentWave;
	}
};

/** 单个敌人血条的视图数据。位置由 HUD 投影，尺寸由 WBP 设计决定。 */
USTRUCT(BlueprintType)
struct FRogueEnemyHealthBarView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EnemyBar")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "EnemyBar")
	bool bBoss = false;

	UPROPERTY(BlueprintReadOnly, Category = "EnemyBar")
	float HealthPercent = 0.0f;

	/** 屏幕坐标（像素，已去掉 DPI 缩放）；WBP 以该点作为血条顶部中心。 */
	UPROPERTY(BlueprintReadOnly, Category = "EnemyBar")
	FVector2D ScreenPosition = FVector2D::ZeroVector;
};
