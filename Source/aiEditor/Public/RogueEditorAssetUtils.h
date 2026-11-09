#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "UI/Widgets/RogueHUDViewData.h"
#include "RogueEditorAssetUtils.generated.h"

class UBlueprint;
class UMaterial;
class UCurveTable;
class UMaterialExpression;
class ARogueEnemy;
class ARogueCharacter;
class ARogueReturningTalisman;
class ARogueOrbitingBlade;
class UStaticMesh;
class UInputAction;
class APlayerController;

/** 编辑器脚本辅助：资产校验、受控保存与临时预览；不参与正式游戏模块。 */
UCLASS()
class URogueEditorAssetUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static int32 GrantPreviewLevels(ARogueCharacter* Player,int32 Count=1);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static FString InspectUpgradeSelectionPreview(APlayerController* Owner);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static bool SendPreviewKey(APlayerController* Owner,FName KeyName,bool bPressed,bool bFocusUpgradeButton=false);
    /** 仅向临时 PIE 的本机 WBP 推预览数据，不修改角色、GAS 或存档。 */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static bool SetPreviewVitalsView(APlayerController* Owner,const FRoguePlayerVitalsView& View);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview")
    static FString VerifyPreviewRangedSpawns(ARogueCharacter* Player, class ARogueEnemySpawnRegion* Region);
    /** 与刷怪运行时共用验证，返回脚下位置；不保存或更改地图。 */
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Map")
    static bool ValidateRangedSpawnSite(ARogueEnemy* Probe, class ARogueEnemySpawnRegion* Region,
        FVector Candidate, FVector& OutLocation);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Map")
    static void ConfigureEnemySpawnProbe(ARogueEnemy* Probe, ERogueEnemyType Type, bool bBoss=false);
    /** 临时 PIE 验证 Esc 的真实 Slate 路由，可覆盖按钮持有焦点的场景。 */
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview")
    static bool SendPreviewEscape(APlayerController* Controller, bool bFocusMenuButton=false);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview")
    static bool RefreshPreviewEnemyLocations(APlayerController* Controller);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview")
    static bool CapturePreviewHUD(APlayerController* Controller, const FString& Filename);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Migration")
    static int32 GetBalanceLayoutVersion(UObject* Asset);

    /** 迁移前定位蓝图实际读取的旧变量；只有资产默认值不算图表调用。 */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Migration")
    static TArray<FString> FindBlueprintVariableReferences(UBlueprint* Blueprint,const TArray<FName>& Names);
    /** 编译蓝图并返回真实状态，供迁移脚本阻止保存损坏的资产。 */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Migration")
    static bool ValidateBlueprint(UBlueprint* Blueprint);
    /** Apply real picks only to a temporary authority gameplay world; never edits assets or local progression. */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static bool ApplyPreviewUpgrade(ARogueCharacter* Player,ERogueUpgradeType Type,int32 Times=1);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static FString InspectOwnedItemsPreview(APlayerController* Owner);
    /** Resize only the temporary PIE preview window; does not save user graphics settings. */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static FIntPoint ResizePreviewWindow(APlayerController* Owner,int32 Width=1440,int32 Height=810);
    /** 只在临时运行世界播放既有池化表现，不触发攻击、伤害或音效。 */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static ARogueCombatAccentEffect* PlayPreviewAccent(UObject* WorldContextObject,const FRogueCombatAccentSettings& Settings,FVector Location,FVector Direction,bool bImpact=false);
    /** Feed an action through real Enhanced Input in a temporary gameplay world. */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static bool InjectPreviewInput(APlayerController* Controller,UInputAction* Action,FVector2D Axis);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Map")
    static void RefreshMapCollision();
    /** Update native default soft references before an authorized level rename; normal asset redirectors are retained. */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Map")
    static int32 MigrateNativeMapDefaults(const FString& SourcePath,const FString& DestinationPath);
    /** Preserve validated simple collision when re-exporting a cosmetic refinement of the same layout. */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Map")
    static int32 CopyMapCollision(UStaticMesh* Source,UStaticMesh* Destination);
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
	static bool ApplyPreviewDamage(AActor* Source, AActor* Target, float Damage,int32 HitVisualStyle=0);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static bool SetPreviewArmor(ARogueCharacter* Player,float Maximum,float Current);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Preview")
    static TArray<AActor*> SpawnOrbitPreview(ARogueCharacter* Player,TSubclassOf<ARogueReturningTalisman> TalismanClass,TSubclassOf<ARogueOrbitingBlade> BladeClass,int32 Talismans=8,int32 Blades=3);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Preview")
	static bool OpenPreviewUpgradeSelection(ARogueCharacter* Player);
};
