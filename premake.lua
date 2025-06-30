-------------------------------------------------------------------------------------
-- Args
-------------------------------------------------------------------------------------
newoption {
    trigger = "enginePath",
    description = "engine Path",
}

newoption {
    trigger = "includSourceCode",
    description = "includ Source Code",
}

HE = _OPTIONS["enginePath"]
includSourceCode = _OPTIONS["includSourceCode"]  == "true"
projectLocation = "%{wks.location}/Build/IDE"

RHI = {}
RHI.enableD3D11  = false
RHI.enableD3D12  = true
RHI.enableVulkan = true

include (HE .. "/build.lua")
print("[Engine] : " .. HE)

-------------------------------------------------------------------------------------
-- workspace
-------------------------------------------------------------------------------------
workspace "HRay"
    architecture "x86_64"
    configurations { "Debug", "Release", "Profile", "Dist" }
    startproject "HRay"
    flags
    {
      "MultiProcessorCompile",
    }

    IncludeHydraProject(includSourceCode)
    AddPlugins()

    group "HRay"
        project "HRay"
            kind "ConsoleApp"
            language "C++"
            cppdialect "C++latest"
            staticruntime "off"
            targetdir (binOutputDir)
            objdir (IntermediatesOutputDir)

            LinkHydraApp(includSourceCode)
            SetHydraFilters()

            files {
            
                "Source/**.h",
                "Source/**.cpp",
                "Source/**.cppm",
                "Source/Shaders/**.hlsl",
                "*.lua",

                "Resources/Icons/HRay.aps",
                "Resources/Icons/HRay.rc",
                "Resources/Icons/resource.h",
            }

            includedirs {
            
                "Source",
            }

            links {
            
                "ImGui",
                "Assets",
            }

            buildoptions {
            
                AddCppm("imgui"),
                AddCppm("Assets"),
            }

            SetupShaders(
                { D3D12 = true, VULKAN = true },       -- api
                "%{prj.location}/Source/Shaders",      -- sourceDir
                "%{prj.location}/Source/HRay/Embeded", -- cacheDir
                "--header"                             -- args
            )
    group ""
