using UnrealBuildTool;

public class OutBreakEditor : ModuleRules
{
	public OutBreakEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "OutBreak" });
		PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "AssetRegistry" });
	}
}
