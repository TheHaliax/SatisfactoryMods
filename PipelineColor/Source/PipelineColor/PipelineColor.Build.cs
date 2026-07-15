using UnrealBuildTool;

public class PipelineColor : ModuleRules
{
	public PipelineColor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"PhysicsCore",
			"InputCore",
			"AssetRegistry",
			"SlateCore",
			"Slate",
			"UMG",
			"RenderCore",
			"NetCore",
			"GameplayTags",
			"Json",
			"JsonUtilities",
			"AbstractInstance",
			"DummyHeaders",
			"FactoryGame",
			"SML",
		});
	}
}
