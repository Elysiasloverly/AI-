#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Shared/RogueTypes.h"
#include "RogueGameBalanceAsset.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "基础值与每波成长"))
struct AI_API FRogueStatGrowth
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "基础值", ToolTip = "普通敌人第1波使用基础值；首领仍会额外计算当前波次的成长。"))
	float BaseValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每波增加", ToolTip = "普通敌人：基础值 + (波次 - 1) × 每波增加；首领：基础值 + 波次 × 每波增加。"))
	float PerWave = 0.0f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "每次刷怪数量规则"))
struct AI_API FRogueSpawnBatchRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "从第几波开始", ToolTip = "达到该波次并满足空余名额条件时，本条规则生效。", ClampMin = "1"))
	int32 StartWave = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "至少空出多少个名额", ToolTip = "空余名额 = 本次生效的场上数量上限 - 当前存活敌人数。", ClampMin = "0"))
	int32 RequiredFreeSlots = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每次尝试生成几只", ToolTip = "控制一次刷怪尝试的目标只数；生成上限、类型限制和落点仍可能减少实际数量。多条满足时采用列表中最后一条。", ClampMin = "1"))
	int32 BatchSize = 1;
};

USTRUCT(BlueprintType, meta = (DisplayName = "开局刷怪放缓"))
struct AI_API FRogueEarlySpawnPacing
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "放缓到第几波", ToolTip = "第1波到本波之间线性过渡；之后不再额外放缓。", ClampMin = "1"))
	int32 EndWave = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "第1波间隔倍率", ToolTip = "填1.35表示间隔变为原来的1.35倍，刷得更慢。", ClampMin = "0"))
	float FirstWaveIntervalMultiplier = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "结束波间隔倍率", ToolTip = "放缓结束的当波使用此倍率；之后恢复为1。", ClampMin = "0"))
	float LastWaveIntervalMultiplier = 1.0f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "刷怪数量与速度"))
struct AI_API FRogueSpawnBalanceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每波增加的场上名额", ToolTip = "场上数量上限 = 生成系统的基础上限 + 本值 × (波次 - 1)。0表示不随波次增加。", ClampMin = "0"))
	int32 MaxAlivePerWave = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "场上名额总上限（0=不限）", ToolTip = "先限制普通情况下的上限，再应用Boss在场时的调整；Boss保底值可能抬高此限制。", ClampMin = "0"))
	int32 MaxAliveLimit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Boss在场时减少的名额", ToolTip = "Boss在场时，将场上数量上限减去本值，再与下方最低值取较大值。", ClampMin = "0"))
	int32 BossAliveReduction = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Boss在场时的上限最低值", ToolTip = "这是数量上限的保底值，不是保证至少刷出多少只。", ClampMin = "0"))
	int32 BossAliveMinimumLimit = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每次刷怪数量规则", ToolTip = "默认先尝试每次2只，再尝试每次3只；按列表顺序匹配，后面的匹配项覆盖前面。清空则每次尝试1只。", TitleProperty = "StartWave"))
	TArray<FRogueSpawnBatchRule> BatchRules = { {5, 10, 2}, {10, 24, 3} };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每波刷怪加速增量", ToolTip = "间隔除以 1 + (波次 - 1) × 本值，再受加速上限限制。只改变刷怪速度，不增强敌人属性。", ClampMin = "0"))
	float AccelerationPerWave = 0.21f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "刷怪加速上限（倍）", ToolTip = "填4.8表示最多将基础刷怪间隔除以4.8。", ClampMin = "1"))
	float MaxAcceleration = 4.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "开局刷怪放缓", ToolTip = "用于降低前几波刷怪速度，与每波加速共同计算。"))
	FRogueEarlySpawnPacing EarlyWaves;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "最短刷怪间隔（秒）", ToolTip = "最终间隔至少为此值，防止后期刷怪过密。未加载任何平衡资产时，旧有代码兜底仍为0.18秒。", Units = "s", ClampMin = "0"))
	float MinimumInterval = 0.20f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "额外生命成长阶段"))
struct AI_API FRogueHealthGrowthStage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "从第几波开始", ToolTip = "开始波当波就增加一次；之后每过一波再增加一次。", ClampMin = "1"))
	int32 StartWave = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每波增加比例（0.08=8%）", ToolTip = "第N波的本阶段生命倍率 = 1 + (N - 开始波 + 1) × 本值，再按下方加成上限封顶。"))
	float BonusPerWave = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "加成上限（≤0=不限）", ToolTip = "填0.72表示本阶段最多增加72%，即乘1.72。多条阶段同时生效时，各阶段倍率依次相乘。"))
	float MaxBonus = 0.0f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "普通敌人属性"))
struct AI_API FRogueEnemyStatSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "生命", ToolTip = "先算基础与每波成长，再乘生命强化阶段和敌人类型倍率。"))
	FRogueStatGrowth Health = {35.0f, 10.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "移动速度", ToolTip = "单位为厘米/秒；基础与成长计算后再乘敌人类型速度倍率。"))
	FRogueStatGrowth Speed = {240.0f, 12.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "伤害", ToolTip = "基础与成长计算后再乘敌人类型伤害倍率；实际承伤仍受护甲等规则影响。"))
	FRogueStatGrowth Damage = {8.0f, 1.5f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "额外生命成长阶段", ToolTip = "合并原中期、后期生命强化。已开始的阶段会同时生效并依次相乘；清空表示没有额外阶段强化。", TitleProperty = "StartWave"))
	TArray<FRogueHealthGrowthStage> HealthGrowthStages = { {8, 0.08f, 0.72f}, {16, 0.05f, 0.50f} };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "最终生命上限（负数=不限）", ToolTip = "包含阶段和类型倍率后再封顶，只影响普通敌人。0会生成零生命档案，激活还受敌人初始化钳制影响。"))
	float MaxHealth = -1.0f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "首领阶段强化"))
struct AI_API FRogueBossStageMultipliers
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "生命倍率", ToolTip = "此阶段对所有首领生效，随后还会乘各敌人类型的首领生命倍率。"))
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "伤害倍率", ToolTip = "此阶段对所有首领生效，随后还会乘各敌人类型的首领伤害倍率。"))
	float DamageMultiplier = 1.0f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "首领属性"))
struct AI_API FRogueBossStatSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "生命", ToolTip = "基础值 + 当前波次 × 每波增加，然后乘阶段和类型倍率；首波也计算一次成长。"))
	FRogueStatGrowth Health = {420.0f, 140.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "移动速度", ToolTip = "单位为厘米/秒；基础值 + 当前波次 × 每波增加，再乘首领通用速度倍率和类型倍率。"))
	FRogueStatGrowth Speed = {250.0f, 10.0f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "伤害", ToolTip = "基础值 + 当前波次 × 每波增加，然后乘阶段和类型倍率。"))
	FRogueStatGrowth Damage = {18.0f, 2.6f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "后期强化从第几波开始", ToolTip = "小于该波次使用前期强化；从该波次起改用后期强化，两段不会叠乘。", ClampMin = "1"))
	int32 LateStageStartWave = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "分界波之前的强化", ToolTip = "用于所有首领，不需要额外的精英标记。"))
	FRogueBossStageMultipliers EarlyStage = {1.58f, 1.16f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "从分界波起的强化", ToolTip = "替换前期强化，而不是在前期倍率上继续相乘。"))
	FRogueBossStageMultipliers LateStage = {1.72f, 1.24f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "首领通用速度倍率", ToolTip = "所有波次的首领都应用此倍率。"))
	float SpeedMultiplier = 1.05f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "每隔若干波增加1点"))
struct AI_API FRogueSteppedReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "基础奖励", ToolTip = "最终基础奖励 = 基础奖励 + 波次 / 增加间隔，按整数除法计算，再限制在基础奖励和上限之间。"))
	int32 BaseAmount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每几波增加1点", ToolTip = "例如填6：第6波起多1点，第12波起多2点。运行时至少按1波计算。", ClampMin = "1"))
	int32 WaveInterval = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "基础奖励上限", ToolTip = "普通经验达到该上限后，仍会加上敌人类型的额外经验；金币没有这一项额外加成。"))
	int32 MaximumAmount = 1;
};

USTRUCT(BlueprintType, meta = (DisplayName = "首领经验"))
struct AI_API FRogueBossExperienceReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每波对应的经验", ToolTip = "奖励 = 出生波次 × 本值，再限制在最少与最多经验之间。"))
	int32 PerWave = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "最少经验", ToolTip = "首领经验奖励的下限，不加敌人类型的额外经验。"))
	int32 MinimumAmount = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "最多经验", ToolTip = "首领经验奖励的上限。"))
	int32 MaximumAmount = 120;
};

USTRUCT(BlueprintType, meta = (DisplayName = "首领金币"))
struct AI_API FRogueBossGoldReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "基础金币", ToolTip = "奖励 = 基础金币 + 击杀时的当前波次 × 每波增加；最少为基础金币，最多为下方上限。"))
	int32 BaseAmount = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每波增加金币", ToolTip = "按击杀时的当前波次计算。"))
	int32 PerWave = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "最多金币", ToolTip = "首领金币奖励的上限。"))
	int32 MaximumAmount = 28;
};

USTRUCT(BlueprintType, meta = (DisplayName = "击杀奖励"))
struct AI_API FRogueKillRewardSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "普通敌人经验", ToolTip = "按敌人出生波次确定，最后再加敌人类型的额外经验。"))
	FRogueSteppedReward EnemyExperience = {1, 2, 5};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "普通敌人金币", ToolTip = "按击杀时的当前波次计算。"))
	FRogueSteppedReward EnemyGold = {1, 6, 3};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "首领经验", ToolTip = "按首领出生波次确定。"))
	FRogueBossExperienceReward BossExperience;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "首领金币", ToolTip = "按击杀时的当前波次计算。"))
	FRogueBossGoldReward BossGold;
};

USTRUCT(BlueprintType, meta = (DisplayName = "商店"))
struct AI_API FRogueShopBalanceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "每次提供几张卡", ToolTip = "目标卡数至少按1计算；可用升级类型不足时，实际数量会减少。", ClampMin = "1"))
	int32 OfferCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "卡牌基础价格", ToolTip = "运行时至少为1金币；同类型卡每购买一次，后续该类型的价格翻倍。", ClampMin = "1"))
	int32 OfferCost = 22;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "手动刷新基础价格", ToolTip = "0表示免费刷新；每次手动刷新后当前刷新价格翻倍，自动刷新时恢复基础价格。", ClampMin = "0"))
	int32 RefreshCost = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "自动刷新间隔（秒）", ToolTip = "运行时至少为5秒。", Units = "s", ClampMin = "5"))
	float AutoRefreshInterval = 60.0f;
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyWaveEntryRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "从第几波参与刷怪", ToolTip = "达到本波后持续参与抽取；同类型多条记录的权重会累计，不会互相替换。"))
	int32 MinWave = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "敌人类型", ToolTip = "选择本条规则对应的敌人类型。"))
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "刷出倾向（权重）", ToolTip = "相对数值：权重3通常比权重1更容易抽中；仍受生成上限等条件过滤。"))
	int32 Weight = 1;
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyBalanceRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "敌人类型", ToolTip = "选择本条规则对应的敌人类型。"))
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "生命倍率", ToolTip = "普通敌人完成基础成长和生命阶段强化后，生命再乘本值。"))
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "速度倍率", ToolTip = "普通敌人完成基础成长后，移动速度再乘本值。"))
	float SpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "伤害倍率", ToolTip = "普通敌人完成基础成长后，伤害再乘本值。"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "额外经验奖励", ToolTip = "普通敌人的基础经验封顶后再加本值；首领不使用此项。"))
	int32 ExperienceRewardBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Boss生命倍率", ToolTip = "首领完成基础成长和前后期强化后，生命再乘本值；不使用同类型普通敌人的生命倍率。"))
	float BossHealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Boss速度倍率", ToolTip = "首领完成基础成长并应用通用速度倍率后，移动速度再乘本值。"))
	float BossSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Boss伤害倍率", ToolTip = "首领完成基础成长和前后期强化后，伤害再乘本值；不使用同类型普通敌人的伤害倍率。"))
	float BossDamageMultiplier = 1.0f;
};

UCLASS(BlueprintType)
class AI_API URogueGameBalanceAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	const FRogueEnemyBalanceRow* FindEnemyBalanceRow(ERogueEnemyType Type) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01.敌人种类", meta = (DisplayName = "各波次的刷怪倾向", ToolTip = "留空使用内置波次池；同类型多条记录累计贡献权重。", TitleProperty = "EnemyType"))
	TArray<FRogueEnemyWaveEntryRow> EnemyWaveEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01.敌人种类", meta = (DisplayName = "各敌人类型的属性修正", ToolTip = "按敌人类型调整普通敌人与首领的属性；缺少某类型时使用内置修正。", TitleProperty = "EnemyType"))
	TArray<FRogueEnemyBalanceRow> EnemyBalanceRows;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "02.刷怪数量与速度", meta = (DisplayName = "刷怪设置", ToolTip = "控制场上名额、每次刷怪目标数量和刷怪间隔。", ShowOnlyInnerProperties))
	FRogueSpawnBalanceSettings Spawn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "03.普通敌人属性", meta = (DisplayName = "普通敌人设置", ToolTip = "第1波使用基础属性，后续波次增加成长，再应用阶段和类型修正。", ShowOnlyInnerProperties))
	FRogueEnemyStatSettings EnemyStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "04.首领属性", meta = (DisplayName = "首领设置", ToolTip = "首领按当前波次计算成长，再应用前期或后期强化与类型修正。", ShowOnlyInnerProperties))
	FRogueBossStatSettings BossStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "05.击杀奖励", meta = (DisplayName = "奖励设置", ToolTip = "经验按出生波次确定；金币按击杀时的当前波次计算。", ShowOnlyInnerProperties))
	FRogueKillRewardSettings Rewards;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "06.商店", meta = (DisplayName = "商店设置", ToolTip = "提供卡牌数量、购买价格和刷新设置。", ShowOnlyInnerProperties))
	FRogueShopBalanceSettings Shop;

private:
	// 仅用于读取旧版资产。PostLoad 按自定义版本迁移，保存时弃用字段不再写入；编辑界面仅展示上方结构。
	UPROPERTY()
	int32 BossAliveMinMaxEnemies_DEPRECATED = 20;

	UPROPERTY()
	int32 BossAliveMaxEnemyReduction_DEPRECATED = 12;

	UPROPERTY()
	int32 MaxAliveEnemiesPerWave_DEPRECATED = 8;

	UPROPERTY()
	int32 MaxAliveEnemiesHardLimit_DEPRECATED = 0;

	UPROPERTY()
	int32 DoubleSpawnWave_DEPRECATED = 5;

	UPROPERTY()
	int32 DoubleSpawnSlack_DEPRECATED = 10;

	UPROPERTY()
	int32 TripleSpawnWave_DEPRECATED = 10;

	UPROPERTY()
	int32 TripleSpawnSlack_DEPRECATED = 24;

	UPROPERTY()
	float DifficultyScalePerWave_DEPRECATED = 0.21f;

	UPROPERTY()
	float MaxDifficultyScale_DEPRECATED = 4.8f;

	UPROPERTY()
	int32 EarlyWaveSlowdownEndWave_DEPRECATED = 4;

	UPROPERTY()
	float EarlyWaveSpawnMultiplierAtWaveOne_DEPRECATED = 1.35f;

	UPROPERTY()
	float EarlyWaveSpawnMultiplierAtEnd_DEPRECATED = 1.0f;

	UPROPERTY()
	float MinimumSpawnDelay_DEPRECATED = 0.20f;

	UPROPERTY()
	float BaseEnemyHealth_DEPRECATED = 35.0f;

	UPROPERTY()
	float EnemyHealthPerWave_DEPRECATED = 10.0f;

	UPROPERTY()
	float BaseEnemySpeed_DEPRECATED = 240.0f;

	UPROPERTY()
	float EnemySpeedPerWave_DEPRECATED = 12.0f;

	UPROPERTY()
	float BaseEnemyDamage_DEPRECATED = 8.0f;

	UPROPERTY()
	float EnemyDamagePerWave_DEPRECATED = 1.5f;

	UPROPERTY()
	int32 BaseExperienceReward_DEPRECATED = 1;

	UPROPERTY()
	int32 ExperienceRewardWaveDivisor_DEPRECATED = 2;

	UPROPERTY()
	int32 MaxExperienceReward_DEPRECATED = 5;

	UPROPERTY()
	int32 MidGameHealthStartWave_DEPRECATED = 8;

	UPROPERTY()
	float MidGameHealthPerWave_DEPRECATED = 0.08f;

	UPROPERTY()
	float MidGameHealthCap_DEPRECATED = 0.72f;

	UPROPERTY()
	int32 LateGameHealthStartWave_DEPRECATED = 16;

	UPROPERTY()
	float LateGameHealthPerWave_DEPRECATED = 0.05f;

	UPROPERTY()
	float LateGameHealthCap_DEPRECATED = 0.50f;

	UPROPERTY()
	float MaxEnemyHealth_DEPRECATED = -1.0f;

	UPROPERTY()
	float BaseBossHealth_DEPRECATED = 420.0f;

	UPROPERTY()
	float BossHealthPerWave_DEPRECATED = 140.0f;

	UPROPERTY()
	float BaseBossSpeed_DEPRECATED = 250.0f;

	UPROPERTY()
	float BossSpeedPerWave_DEPRECATED = 10.0f;

	UPROPERTY()
	float BaseBossDamage_DEPRECATED = 18.0f;

	UPROPERTY()
	float BossDamagePerWave_DEPRECATED = 2.6f;

	UPROPERTY()
	int32 BossExperiencePerWave_DEPRECATED = 4;

	UPROPERTY()
	int32 MinBossExperienceReward_DEPRECATED = 30;

	UPROPERTY()
	int32 MaxBossExperienceReward_DEPRECATED = 120;

	UPROPERTY()
	int32 LateEliteBossStartWave_DEPRECATED = 20;

	UPROPERTY()
	float EarlyEliteBossHealthMultiplier_DEPRECATED = 1.58f;

	UPROPERTY()
	float LateEliteBossHealthMultiplier_DEPRECATED = 1.72f;

	UPROPERTY()
	float EarlyEliteBossDamageMultiplier_DEPRECATED = 1.16f;

	UPROPERTY()
	float LateEliteBossDamageMultiplier_DEPRECATED = 1.24f;

	UPROPERTY()
	float EliteBossSpeedMultiplier_DEPRECATED = 1.05f;

	UPROPERTY()
	int32 EnemyCurrencyBaseReward_DEPRECATED = 1;

	UPROPERTY()
	int32 EnemyCurrencyWaveDivisor_DEPRECATED = 6;

	UPROPERTY()
	int32 MaxEnemyCurrencyReward_DEPRECATED = 3;

	UPROPERTY()
	int32 BossCurrencyBaseReward_DEPRECATED = 14;

	UPROPERTY()
	int32 BossCurrencyPerWave_DEPRECATED = 1;

	UPROPERTY()
	int32 MaxBossCurrencyReward_DEPRECATED = 28;

	UPROPERTY()
	int32 ShopOfferCount_DEPRECATED = 6;

	UPROPERTY()
	int32 ShopOfferCost_DEPRECATED = 22;

	UPROPERTY()
	int32 ShopRefreshCost_DEPRECATED = 14;

	UPROPERTY()
	float ShopAutoRefreshInterval_DEPRECATED = 60.0f;

};
