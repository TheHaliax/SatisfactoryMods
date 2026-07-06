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
			"AnimGraphRuntime",
			"AssetRegistry",
			"NavigationSystem",
			"AIModule",
			"GameplayTasks",
			"SlateCore",
			"Slate",
			"ApplicationCore",
			"UMG",
			"RenderCore",
			"CinematicCamera",
			"Foliage",
			"NetCore",
			"GameplayTags",
			"Json",
			"JsonUtilities",
			"AbstractInstance",
			"DummyHeaders",
			"FactoryGame",
			"SML",
		});

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PublicDependencyModuleNames.Add("AnimGraph");
		}
	}
}
