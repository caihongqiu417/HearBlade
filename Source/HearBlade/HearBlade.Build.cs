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
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"HearBlade",
			"HearBlade/Variant_Platforming",
			"HearBlade/Variant_Platforming/Animation",
			"HearBlade/Variant_Combat",
			"HearBlade/Variant_Combat/AI",
			"HearBlade/Variant_Combat/Animation",
			"HearBlade/Variant_Combat/Gameplay",
			"HearBlade/Variant_Combat/Interfaces",
			"HearBlade/Variant_Combat/UI",
			"HearBlade/Variant_SideScrolling",
			"HearBlade/Variant_SideScrolling/AI",
			"HearBlade/Variant_SideScrolling/Gameplay",
			"HearBlade/Variant_SideScrolling/Interfaces",
			"HearBlade/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
