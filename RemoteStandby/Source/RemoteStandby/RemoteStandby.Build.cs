using UnrealBuildTool;

public class RemoteStandby : ModuleRules
{
	public RemoteStandby(ReadOnlyTargetRules Target) : base(Target)
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
			"ApplicationCore",
			"NetCore",
			"GameplayTags",
			"AbstractInstance",
			"DummyHeaders",
			"FactoryGame",
			"SML",
		});
	}
}
