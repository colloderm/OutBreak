using UnrealBuildTool;

public class InteriorPCGEditor : ModuleRules
{
	public InteriorPCGEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AssetRegistry",
			"UnrealEd",
			"InteriorPCGRuntime",
			"PCG"
		});
	}
}
