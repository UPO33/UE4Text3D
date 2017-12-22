// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UText3D : ModuleRules
{
	public UText3D(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				"UText3D/Public"
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"UText3D/Private",
                "UText3D/Private/poly2tri"
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "Slate",
                "SlateCore",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
                "RHI",
                "ShaderCore",
                "RenderCore",

                "FreeType2",
                "HarfBuzz",
                "ICU",
                //"ProceduralMeshComponent"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

        if (Target.Type != TargetType.Server)
        {
            if (UEBuildConfiguration.bCompileFreeType)
            {
                AddEngineThirdPartyPrivateStaticDependencies(Target, "FreeType2");
                Definitions.Add("WITH_FREETYPE=1");
                Definitions.Add("WITH_HARFBUZZ=1");
            }
            else
            {
                Definitions.Add("WITH_FREETYPE=0");
            }

            if (UEBuildConfiguration.bCompileICU)
            {
                AddEngineThirdPartyPrivateStaticDependencies(Target, "ICU");
            }

            AddEngineThirdPartyPrivateStaticDependencies(Target, "HarfBuzz");
        }
        else
        {
            Definitions.Add("WITH_FREETYPE=0");
            Definitions.Add("WITH_HARFBUZZ=0");
        }
    }
}
