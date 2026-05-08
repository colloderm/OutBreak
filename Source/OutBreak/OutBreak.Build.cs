// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OutBreak : ModuleRules
{
	public OutBreak(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"OutBreak",
			"OutBreak/Variant_Platforming",
			"OutBreak/Variant_Platforming/Animation",
			"OutBreak/Variant_Combat",
			"OutBreak/Variant_Combat/AI",
			"OutBreak/Variant_Combat/Animation",
			"OutBreak/Variant_Combat/Gameplay",
			"OutBreak/Variant_Combat/Interfaces",
			"OutBreak/Variant_Combat/UI",
			"OutBreak/Variant_SideScrolling",
			"OutBreak/Variant_SideScrolling/AI",
			"OutBreak/Variant_SideScrolling/Gameplay",
			"OutBreak/Variant_SideScrolling/Interfaces",
			"OutBreak/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
