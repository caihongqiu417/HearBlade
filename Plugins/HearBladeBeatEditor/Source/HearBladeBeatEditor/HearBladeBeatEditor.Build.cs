using UnrealBuildTool;

public class HearBladeBeatEditor : ModuleRules
{
    public HearBladeBeatEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "HearBlade", "Slate", "SlateCore",
            "InputCore", "UnrealEd", "PropertyEditor", "AssetRegistry",
            "ToolMenus", "DesktopPlatform", "Projects"
        });
    }
}
