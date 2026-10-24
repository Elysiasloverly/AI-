#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Progression/Meta/RogueProgressionTypes.h"
#include "RogueUpgradeTreeDefinition.generated.h"

/** 升级树唯一配置入口。节点永久标识用于存档，名称、描述和前置关系可以自由调整。 */
UCLASS(BlueprintType)
class AI_API URogueUpgradeTreeDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	URogueUpgradeTreeDefinition();


#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "调试|冰砖", meta = (DisplayName = "目标冰砖数量", ClampMin = "0", ToolTip = "填写目标余额后点击“应用冰砖数量”。只执行一次，不会在下次开局重置余额。"))
	int32 DebugIceBricks = 100;
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "调试|冰砖", meta = (DisplayName = "本地玩家索引", ClampMin = "0", ToolTip = "通常保持 0；分屏时选择对应本地玩家。"))
	int32 DebugLocalPlayerIndex = 0;
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "调试|冰砖", meta = (DisplayName = "PIE 窗口编号", ClampMin = "0", ToolTip = "仅运行 PIE 时生效。0 通常是房主窗口，1 是第一个加入者窗口。"))
	int32 DebugPIEInstance = 0;
	UPROPERTY(VisibleAnywhere, Transient, Category = "调试|冰砖", meta = (DisplayName = "当前冰砖（-1 为尚未读取）"))
	int32 DebugCurrentIceBricks = -1;
	UPROPERTY(VisibleAnywhere, Transient, Category = "调试|冰砖", meta = (DisplayName = "操作结果"))
	FText DebugIceBrickStatus;
#endif
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "调试|冰砖", meta = (DisplayName = "读取当前冰砖", DevelopmentOnly))
	void RefreshDebugIceBricks();
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "调试|冰砖", meta = (DisplayName = "应用冰砖数量", DevelopmentOnly))
	void ApplyDebugIceBricks();

	/** Resets purchased ranks only; initial-free nodes still come from this asset's configuration. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "调试|进度", meta = (DisplayName = "重置本地解锁进度（保留冰砖）", ToolTip = "清除所选本地玩家已购买的武器、专属卡和基础属性等级，并立即保存。保留冰砖余额及 Boss 奖励记录；不修改节点配置。沿用冰砖调试中的玩家索引和 PIE 窗口编号。", DevelopmentOnly))
	void ResetDebugProgression();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "升级树", meta = (DisplayName = "全部升级节点"))
	TArray<FRogueUpgradeTreeNode> Nodes;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "升级树", meta = (DisplayName = "恢复默认节点配置（不重置存档）", ToolTip = "将名称、说明、费用、初始免费节点和前置关系恢复为代码默认配置。此操作会替换自定义节点，不清除玩家已购买的解锁进度。重置玩家进度请使用“重置本地解锁进度（保留冰砖）”。"))
	void ResetToDefaults();

	UFUNCTION(BlueprintPure, Category = "升级树")
	const TArray<FRogueUpgradeTreeNode>& GetNodes() const { return Nodes; }

	const FRogueUpgradeTreeNode* FindNode(FName NodeId) const;
	static const URogueUpgradeTreeDefinition* GetDefaultDefinition();

	/** 删除未知、超限和没有前置条件的等级；不会凭空赠送付费节点。 */
	UFUNCTION(BlueprintPure, Category = "升级树")
	FRogueProgressionSnapshot SanitizeSnapshot(const FRogueProgressionSnapshot& Snapshot) const;

	UFUNCTION(BlueprintPure, Category = "升级树")
	int32 GetNodeRank(const FRogueProgressionSnapshot& Snapshot, FName NodeId) const;

	UFUNCTION(BlueprintPure, Category = "升级树")
	FText GetPurchaseFailure(const FRogueProgressionSnapshot& Snapshot, int32 IceBricks, FName NodeId) const;

	UFUNCTION(BlueprintPure, Category = "升级树")
	bool IsWeaponUnlocked(const FRogueProgressionSnapshot& Snapshot, ERogueUpgradeType QuantityType) const;

	/** 数量卡：未持有武器时按武器解锁判定，已经持有时按对应专属卡判定。 */
	UFUNCTION(BlueprintPure, Category = "升级树")
	bool IsUpgradeAvailable(const FRogueProgressionSnapshot& Snapshot, ERogueUpgradeType Type, bool bWeaponAlreadyOwned) const;

	UFUNCTION(BlueprintPure, Category = "升级树")
	bool IsArmorUnlocked(const FRogueProgressionSnapshot& Snapshot) const;

	UFUNCTION(BlueprintPure, Category = "升级树")
	float GetStatBonus(const FRogueProgressionSnapshot& Snapshot, ERogueMetaStat Stat) const;

	int32 GetMaximumRank(const FRogueUpgradeTreeNode& Node) const;
};
