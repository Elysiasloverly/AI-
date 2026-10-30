#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueEditorAssetUtils.generated.h"

class UMaterial;
class UCurveTable;
class UMaterialExpression;
class ARogueEnemy;
class ARogueCharacter;

/** 编辑器脚本辅助：强制把已修改的内存资产标记为 dirty 并保存。 */
UCLASS()
class URogueEditorAssetUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
    /** 临时独立 PIE 窗口，用于检查真实 WBP 的完整布局；不保存编辑器偏好。 */
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview")
    static void StartWidgetPreviewWindow(int32 Width = 1440, int32 Height = 810);
	/** UE 5.5 Python 隐藏了 WPO 枚举；连接环境微光的材质顶点偏移。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Material")
	static bool ConnectWorldPositionOffset(UMaterial* Material, UMaterialExpression* Expression);

	/** MarkPackageDirty + UEditorAssetLibrary::SaveAsset；用于 Python 修改 DataTable 后落盘。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool SaveAsset(UObject* Asset);

    /** 创建可编辑的Rich曲线数据；线性关键帧、右侧线性外推，不自动保存或更新已存在的平衡资产。 */
    UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Balance")
    static bool ImportGrowthCurveTable(UCurveTable* Table, const FString& Json, bool bMergeRows = false);


	/** 临时运行世界中的自动截图/联机检查辅助；不会修改地图资产。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Preview")
	static AActor* SpawnPreviewActor(UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, FTransform Transform);

	/** PIE/临时运行世界验证：读取真实GAS和镜像，不修改地图资产。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Preview")
	static FString InspectEnemyPreview(ARogueEnemy* Enemy);

	/** 仅用于临时运行世界的真实伤害与选卡观察，不在编辑世界执行。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Preview")
	static bool ApplyPreviewDamage(AActor* Source, AActor* Target, float Damage);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Preview")
	static bool OpenPreviewUpgradeSelection(ARogueCharacter* Player);
};
