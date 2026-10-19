// ai 项目的模块/构建入口。

using UnrealBuildTool;
using System.Collections.Generic;

public class aiEditorTarget : TargetRules
{
	public aiEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;

		ExtraModuleNames.AddRange( new string[] { "ai" } );
	}
}
