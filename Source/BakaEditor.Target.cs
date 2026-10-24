// Baka！项目（对外名）的模块/构建入口；内部模块名仍为 ai。

using UnrealBuildTool;
using System.Collections.Generic;

public class BakaEditorTarget : TargetRules
{
	public BakaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;

		ExtraModuleNames.AddRange( new string[] { "ai", "aiEditor" } );
	}
}
