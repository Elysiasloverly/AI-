#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueEditorAssetUtils.generated.h"

/** 编辑器脚本辅助：强制把已修改的内存资产标记为 dirty 并保存。 */
UCLASS()
class URogueEditorAssetUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** MarkPackageDirty + UEditorAssetLibrary::SaveAsset；用于 Python 修改 DataTable 后落盘。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool SaveAsset(UObject* Asset);
};
