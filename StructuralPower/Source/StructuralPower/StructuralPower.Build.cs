using UnrealBuildTool;

public class StructuralPower : ModuleRules
{
	public StructuralPower(ReadOnlyTargetRules Target) : base(Target)
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
			"GeometryCollectionEngine",
			"AssetRegistry",
			"SlateCore",
			"Slate",
			"ApplicationCore",
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
