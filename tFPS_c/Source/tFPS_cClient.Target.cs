using UnrealBuildTool;
using System.Collections.Generic;

public class tFPS_cClientTarget : TargetRules
{
	public tFPS_cClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		ExtraModuleNames.AddRange(new string[] { "tFPS_c" });
	}
}
