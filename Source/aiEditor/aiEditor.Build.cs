using UnrealBuildTool;

public class aiEditor : ModuleRules
{
	public aiEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 各编辑器工具的文件内辅助函数重名，独立编译以免依赖 Git 工作集的偶然排除。
        bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
            "ai",
            "GameplayAbilities",
			"UMG",
			"UMGEditor",
			"BlueprintGraph",
			"Kismet",
			"AssetTools",
			"AssetRegistry",
			"EditorScriptingUtilities",
			"Slate",
			"SlateCore", "Json"
		});
	}
}
