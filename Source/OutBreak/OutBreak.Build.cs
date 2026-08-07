// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OutBreak : ModuleRules
{
	public OutBreak(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput",
			"UMG",
			"Niagara",
			"AIModule",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"NavigationSystem",
			"DeveloperSettings",
			"AssetRegistry",
			"MotionWarping",
			"PhysicsCore",
			"AnimationBudgetAllocator",
			"Navmesh",
			"StateTreeModule",
			"GameplayStateTreeModule"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"ModelViewViewModel",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"PropertyBindingUtils",
			
		});
		
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimationData",
					"AnimationDataController"
				});

			PrivateDefinitions.Add("WITH_OUTBREAK_EDITOR=1");
		}
		else
		{
			PrivateDefinitions.Add("WITH_OUTBREAK_EDITOR=0");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
