using UnrealBuildTool;
using System.Collections.Generic;

public class tFPS_cServerTarget : TargetRules
{
	public tFPS_cServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		ExtraModuleNames.AddRange(new string[] { "tFPS_c" });
	}
}
