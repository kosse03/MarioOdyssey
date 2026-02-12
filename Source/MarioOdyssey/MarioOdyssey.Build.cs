// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MarioOdyssey : ModuleRules
{
	public MarioOdyssey(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "NavigationSystem", "AIModule", "LevelSequence", "MovieScene" });
	}
}
