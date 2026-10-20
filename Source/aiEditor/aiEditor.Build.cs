using UnrealBuildTool;

public class aiEditor : ModuleRules
{
	public aiEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"UMG",
			"UMGEditor",
			"BlueprintGraph",
			"Kismet",
			"AssetTools",
			"AssetRegistry",
			"EditorScriptingUtilities",
			"Slate",
			"SlateCore"
		});
	}
}
