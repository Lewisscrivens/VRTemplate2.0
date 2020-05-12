// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class VRProject : ModuleRules
{
	public VRProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] 
        { 
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore" , 
            "InputDevice",
            "SteamVRInput",
            "SteamVRInputDevice",
            "HeadMountedDisplay", 
            "NavigationSystem", 
            "AIModule",
            "UMG", 
            "Slate", 
            "SlateCore", 
            "RenderCore", 
            "ApplicationCore", 
            "Paper2D", 
            "LevelSequence", 
            "ActorSequence" , 
            "MovieScene", 
            "PhysicsCore", 
            "PhysX" , 
            "APEX",  
            "GameplayTasks"
        });

        PrivateDependencyModuleNames.AddRange(new string[] 
        { 
            "Slate", 
            "SlateCore", 
            "RenderCore", 
            "HeadMountedDisplay", 
            "SteamVR"
        });
    }
}
