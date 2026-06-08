// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class tFPS_c : ModuleRules
{
	public tFPS_c(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",        // UUserWidget 基类（计分板/HUD WBP 的 C++ 父类）
			"Slate",
			"SlateCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
