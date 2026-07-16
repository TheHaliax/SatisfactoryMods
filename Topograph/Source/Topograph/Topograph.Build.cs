using UnrealBuildTool;

public class Topograph : ModuleRules
{
	public Topograph(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"PhysicsCore",
			"InputCore",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"Landscape",
			"AbstractInstance",
			"DummyHeaders",
			"FactoryGame",
			"SML",
		});
	}
}
