// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class GIGWorldsServerTarget : TargetRules
{
    public GIGWorldsServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		//BuildEnvironment = TargetBuildEnvironment.Shared;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("GIGWorlds");
	}
}
