// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HearBlade : ModuleRules
{
	public HearBlade(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"AudioMixer",
			"AnimGraphRuntime",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"MotionWarping",
			"Niagara"
			,"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "RenderCore", "MovieSceneCapture", "ImageCore" });

		PublicIncludePaths.AddRange(new string[] {
			"HearBlade"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
