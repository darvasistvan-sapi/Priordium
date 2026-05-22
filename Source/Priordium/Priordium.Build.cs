// Copyright Priordium. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Priordium : ModuleRules
{
	public Priordium(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Landscape",
			"Water",
			"GameplayTags",
			"NavigationSystem",
			"UMG",
			"Slate",
			"SlateCore",
			"RenderCore"   // FlushRenderingCommands() used in ULandscapeBuilder::FlattenArea
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"LandscapeEditor",
				"Foliage",
				"UnrealEd",
				"AssetTools"
			});
		}

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[]
		{
			ModuleDirectory + "/MapGenerator/Public",
			ModuleDirectory + "/Tribe",
			ModuleDirectory + "/UI"
		});

		string FastNoisePath = Path.Combine(ModuleDirectory, "../../Source/ThirdParty/FastNoise");
		PublicIncludePaths.Add(FastNoisePath);
		PublicDefinitions.Add("WITH_FASTNOISE=1");

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AutomationController"
			});
		}
	}
}

