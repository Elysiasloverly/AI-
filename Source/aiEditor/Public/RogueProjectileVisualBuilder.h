#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueProjectileVisualBuilder.generated.h"

class UBlueprint;

/**
 * 给敌人弹体蓝图生成一套可编辑/可删除的占位视觉组件。
 *
 * 这些组件是 Blueprint 的 SCS 组件，不是 C++ 子对象，
 * 因此在蓝图组件面板里可以直接删除、替换或改材质。
 */
UCLASS()
class URogueProjectileVisualBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 添加 Body/Core/Glow/Trail 四个网格和一个 PointLight；已存在同名组件时跳过。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildTemplate(UBlueprint* Blueprint);

	/** 玩家弹体 BP：Mesh/GlowMesh/RingMesh/TrailMesh/GlowLight，组件归蓝图所有。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildPlayerProjectileVisuals(UBlueprint* Blueprint);

	/** 环绕镰刀 BP：Mesh + Overlay 材质，组件归蓝图所有。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildOrbitingBladeVisuals(UBlueprint* Blueprint);

	/** 迫击炮 BP：落点指示圈（绝对变换、默认隐藏）。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildMortarIndicator(UBlueprint* Blueprint);

	/** 返回当前 SCS 组件列表，便于脚本核对。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static FString DescribeComponents(UBlueprint* Blueprint);

	/** 返回 SCS 网格组件名与静态网格资产路径。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static FString DescribeMeshComponents(UBlueprint* Blueprint);

	/** 返回父类路径，便于脚本确认 reparent 是否生效。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static FString GetParentClassPath(UBlueprint* Blueprint);

	/** 删除指定名字的 SCS 组件（例如整圈弹幕不需要每发点光/拖尾）。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool RemoveComponent(UBlueprint* Blueprint, FName ComponentName);
};
