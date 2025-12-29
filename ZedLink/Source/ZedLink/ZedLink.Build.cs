// Copyright ZedLink Contributors. All Rights Reserved.

using UnrealBuildTool;

public class ZedLink : ModuleRules
{
	public ZedLink(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Sockets",
			"Networking",
			"Json",
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
			"EditorStyle",
			"LevelEditor",
			"Kismet",
			"BlueprintGraph",
			"AssetRegistry",
			"ToolMenus",
			"EditorSubsystem",
			"Projects"
		});

		// Live Coding support (UE 5.0+)
		if (Target.bWithLiveCoding)
		{
			PrivateDependencyModuleNames.Add("LiveCoding");
			PublicDefinitions.Add("WITH_LIVE_CODING=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_LIVE_CODING=0");
		}
	}
}
