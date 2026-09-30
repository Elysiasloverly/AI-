// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ai : ModuleRules
{
	public ai(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// GAS 迁移：GameplayAbilities（ASC / AttributeSet / GameplayEffect / GameplayAbility）
		// GameplayTags 提供 FGameplayTag 体系；GameplayTasks 是 UAbilityTask 的依赖（本来就有）。
		// 联机化还需要 "NetCore"（FGameplayTagContainer 的网络序列化与 RPC 参数）。
		// 输入迁移到 EnhancedInput（UInputAction / UInputMappingContext / UEnhancedInputComponent），
		// 见 Docs/Enhanced-Input-Migration.md。
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "GameplayTasks", "UMG", "GameplayAbilities", "GameplayTags", "NetCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
