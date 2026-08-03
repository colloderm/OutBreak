using UnrealBuildTool;

public class InteriorPCGRuntime : ModuleRules
{
	public InteriorPCGRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PCG"
		});
	}
}
