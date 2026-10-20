// ai 项目的模块/构建入口。

using UnrealBuildTool;

public class ai : ModuleRules
{
	public ai(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// GAS：GameplayAbilities、GameplayTags、GameplayTasks；NetCore 提供底层网络序列化支持。
		// EnhancedInput 处理玩家输入，UMG/Slate 用于界面，AIModule 用于敌人控制。
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "GameplayTasks", "UMG", "GameplayAbilities", "GameplayTags", "NetCore", "EnhancedInput" });

		// EngineCameras：提供 ULegacyCameraShake（受伤镜头震动的参数化基类）。
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "EngineCameras" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
