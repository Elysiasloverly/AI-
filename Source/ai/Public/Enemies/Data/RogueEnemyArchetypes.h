#pragma once

#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"
#include "Enemies/Data/RogueFlyingEnemySettings.h"
#include "Enemies/Data/RogueEnemyUtilitySettings.h"
#include "RogueEnemyArchetypes.generated.h"

class ARogueEnemySkyfallProjectile;

UENUM(BlueprintType)
enum class ERogueEnemyMovementModel : uint8
{
	Direct       UMETA(DisplayName = "直线追击"),
	BurstCharge  UMETA(DisplayName = "爆发冲刺"),
	Orbit        UMETA(DisplayName = "环绕运动"),
	RangedKite   UMETA(DisplayName = "远程风筝"),
	ZigZagChase  UMETA(DisplayName = "Z字形追击"),
	Stationary   UMETA(DisplayName = "固定不动")
};

UENUM(BlueprintType)
enum class ERogueEnemyShotPattern : uint8
{
	None     UMETA(DisplayName = "无"),
	Single   UMETA(DisplayName = "单发"),
	Spread3  UMETA(DisplayName = "散射"),
	RadialRing UMETA(DisplayName = "整圈弹幕"),
    CrossWeave UMETA(DisplayName = "交织蝶舞"),
    ReturningFan UMETA(DisplayName = "折返风刃"),
    StarTrap UMETA(DisplayName = "星阵合围"),
    Skyfall UMETA(DisplayName = "天坠弹·触地散华")
};

UENUM(BlueprintType)
enum class ERogueEnemyVisualKey : uint8
{
	Rifleman,
	Assault,
	Juggernaut,
	Orbiter,
	Marksman,
	Skirmisher,
	Cannoneer,
	Scatterer,
	ShockPillar,
	Sniper,
	Shotgunner,
	Repeater,
	Ringcaster, SkyMoth, WindReaver, StarLantern,
    AuraHealer,
    LinkHealer
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyMovementArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	ERogueEnemyMovementModel Model = ERogueEnemyMovementModel::Direct;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseMoveMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Burst")
	float BurstCycleTime = 2.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Burst")
	float BurstDuration = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Burst")
	float BurstActiveMultiplier = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Burst")
	float BurstIdleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Range")
	float PreferredRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Range")
	float FarDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Range")
	float CloseDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float FarTowardWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float FarTangentWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float CloseTowardWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float CloseTangentWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float MidTowardWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float MidTangentWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Range")
	bool bUseRangeBiasForMidToward = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|ZigZag")
	float ZigZagFrequency = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|ZigZag")
	float ZigZagTowardWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|ZigZag")
	float ZigZagTangentWeight = 0.0f;
};

/** 震荡柱一次齐射的参数；所有子弹水平飞行，碰到玩家/墙体或到时回收。 */
USTRUCT(BlueprintType)
struct AI_API FRogueEnemyBulletRingSettings
{
	GENERATED_BODY()

	static constexpr float MaxProjectileLifetime = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "每圈子弹数量", ClampMin = "8", ClampMax = "128"))
	int32 ProjectileCount = 48;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "子弹速度（厘米/秒）", ClampMin = "100", ClampMax = "3000"))
	float ProjectileSpeed = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "最长飞行时间（秒）", ClampMin = "0.1", ClampMax = "15"))
	float ProjectileLifetime = MaxProjectileLifetime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "碰撞与特效大小倍率", ToolTip = "影响碰撞球、灯光、拖尾偏移和命中特效；模型网格大小以蓝图组件的缩放为准。", ClampMin = "0.1", ClampMax = "1"))
	float ProjectileScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "单发伤害倍率", ClampMin = "0"))
	float DamageMultiplier = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "发射高度（距柱底，厘米）", ClampMin = "20", ClampMax = "200"))
	float HeightAboveGround = 48.0f;

	/** 配置导入/蓝图赋值也需要边界约束，不能只依赖编辑器输入框。 */
	FRogueEnemyBulletRingSettings Sanitized() const
	{
		FRogueEnemyBulletRingSettings Result = *this;
		Result.ProjectileCount = FMath::Clamp(ProjectileCount, 8, 128);
		Result.ProjectileSpeed = FMath::Clamp(ProjectileSpeed, 100.0f, 3000.0f);
		Result.ProjectileLifetime = FMath::Clamp(
			FMath::IsFinite(ProjectileLifetime) ? ProjectileLifetime : MaxProjectileLifetime, 0.1f, MaxProjectileLifetime);
		Result.ProjectileScale = FMath::Clamp(ProjectileScale, 0.1f, 1.0f);
		Result.DamageMultiplier = FMath::Max(DamageMultiplier, 0.0f);
		Result.HeightAboveGround = FMath::Clamp(HeightAboveGround, 20.0f, 200.0f);
		return Result;
	}
};

/** 鸦天狗的锁定落点、红圈预警及触地散弹。预警时间与下落时间均不受黑夜攻速影响。 */
USTRUCT(BlueprintType)
struct AI_API FRogueEnemySkyfallSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="大型落弹蓝图"))
    TSubclassOf<ARogueEnemySkyfallProjectile> ProjectileClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="悬停预警时间（秒）",ClampMin="0.3",ClampMax="5"))
    float WarningSeconds = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="下落时间（秒）",ClampMin="0.4",ClampMax="5"))
    float FallSeconds = 0.9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="召唤离地高度（厘米）",ClampMin="300",ClampMax="3000"))
    float SpawnHeight = 900.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="朝施法者偏移（厘米）",ClampMin="0",ClampMax="1000",ToolTip="弹体从落点朝施法者方向偏移后升空，再斜向砸向锁定落点。0为垂直下落。"))
    float LaunchOffset = 260.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="红圈与落地伤害半径（厘米）",ClampMin="80",ClampMax="600"))
    float ImpactRadius = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="大型弹体地面碰撞半径（厘米）",ClampMin="20",ClampMax="160"))
    float CollisionRadius = 65.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="落地伤害倍率",ClampMin="0",ClampMax="5"))
    float ImpactDamageMultiplier = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="触地环形弹幕"))
    FRogueEnemyBulletRingSettings Burst;

    FRogueEnemySkyfallSettings()
    {
        Burst.ProjectileCount=24; Burst.ProjectileSpeed=650; Burst.ProjectileScale=1;
        Burst.DamageMultiplier=.45f; Burst.HeightAboveGround=55;
    }
    FRogueEnemySkyfallSettings Sanitized() const
    {
        auto R=*this;
        auto Safe=[](float V,float D,float Min,float Max){return FMath::Clamp(FMath::IsFinite(V)?V:D,Min,Max);};
        R.WarningSeconds=Safe(WarningSeconds,.8f,.3f,5); R.FallSeconds=Safe(FallSeconds,.9f,.4f,5);
        R.SpawnHeight=Safe(SpawnHeight,900,300,3000); R.LaunchOffset=Safe(LaunchOffset,260,0,1000);
        R.ImpactRadius=Safe(ImpactRadius,180,80,600); R.CollisionRadius=Safe(CollisionRadius,65,20,160);
        R.ImpactDamageMultiplier=Safe(ImpactDamageMultiplier,1.2f,0,5);
        R.Burst=Burst.Sanitized(); return R;
    }
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyRangedArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	bool bUsesRangedAttack = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Cooldown")
	float InitialCooldownMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Cooldown")
	float InitialCooldownMax = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Cooldown")
	float NextShotCooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Projectile")
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Projectile")
	float ProjectileDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Visual")
	bool bLargeVisual = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Visual")
	bool bSpawnMuzzleEffect = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Visual")
	FVector MuzzleEffectScale = FVector(0.2f, 0.2f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Visual")
	float MuzzleEffectLifetime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Spread")
	int32 SpreadShotCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Ring", meta = (DisplayName = "整圈弹幕参数"))
	FRogueEnemyBulletRingSettings BulletRing;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ranged|Skyfall", meta=(DisplayName="天坠弹参数"))
    FRogueEnemySkyfallSettings Skyfall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Spread")
	float SpreadAngleStep = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Spread")
	float SpreadSpawnOffset = 12.0f;
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyVisualArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	ERogueEnemyVisualKey VisualKey = ERogueEnemyVisualKey::Rifleman;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FVector MeshScale = FVector(0.52f, 0.52f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FVector MeshLocation = FVector(0.0f, 0.0f, -40.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FRotator MeshRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	float CapsuleRadius = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	float CapsuleHalfHeight = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	bool bUseBossMaterialOverride = false;
};

class URogueAbilitySet;

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype")
	FRogueEnemyMovementArchetype Movement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype")
	FRogueEnemyRangedArchetype Ranged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype")
	FRogueEnemyVisualArchetype Visual;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="飞行敌人")
    FRogueFlyingEnemySettings Flying;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="治疗敌人")
    FRogueEnemyHealingSettings Healing;

	/** 可选攻击能力集；加载失败或未配置时按原型标志授予默认接触/远程能力。
	 *  默认 AI 只识别这两类能力槽；新增独立攻击决策仍需扩展 C++ 或蓝图调用路径。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype|Abilities")
	TSoftObjectPtr<URogueAbilitySet> AttackAbilitySet;

	/** 这一种敌人的弹体蓝图；由原型数据表写入，空则回退敌人 BP / 原生类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype|Combat")
	TSubclassOf<class ARogueEnemyProjectile> ProjectileClass;
};

class UDataTable;

namespace RogueEnemyArchetypes
{
    inline bool IsHealerType(ERogueEnemyType Type)
    { return Type==ERogueEnemyType::AuraHealer || Type==ERogueEnemyType::LinkHealer; }

    inline bool IsFlyingType(ERogueEnemyType Type)
    { return Type==ERogueEnemyType::SkyMoth || Type==ERogueEnemyType::WindReaver || Type==ERogueEnemyType::StarLantern; }

	/** 从硬编码构建原型（保留向后兼容，DataTable 未配置时的回退） */
	AI_API FRogueEnemyArchetype BuildEnemyArchetype(ERogueEnemyType Type, bool bIsBoss);

	/** 从 DataTable 构建原型（优先使用） */
	AI_API FRogueEnemyArchetype BuildEnemyArchetypeFromDataTable(const UDataTable* DataTable, ERogueEnemyType Type, bool bIsBoss);

	/** 在 DataTable 中查找指定敌人类型的行名（RowName） */
	AI_API FName FindRowNameForEnemyType(const UDataTable* DataTable, ERogueEnemyType Type);

	/** 读取该敌人类型的"每波刷新上限"（一整个波次内的累计刷出只数上限）。
	 *  返回 0 表示**不限制**，包括下面三种情况：DataTable 为空、表里没有这个类型的行、
	 *  行里该字段没填（默认就是 0）。 */
	AI_API int32 GetMaxSpawnPerWave(const UDataTable* DataTable, ERogueEnemyType Type);

	/** 读取该敌人类型的"同时存活上限"（场上同时存在的只数上限）。
	 *  返回 0 表示**不限制**（判定口径与 GetMaxSpawnPerWave 一致）。 */
	AI_API int32 GetMaxAlive(const UDataTable* DataTable, ERogueEnemyType Type);
}
