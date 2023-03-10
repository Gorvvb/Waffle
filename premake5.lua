workspace "Waffle"
    architecture "x64"
    startproject "Sandbox"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Waffle"
    location "Waffle"
    kind "SharedLib"
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "wfpch.h"
    pchsource "Waffle/src/wfpch.cpp"

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs
    {
        "%{prj.name}/vendor/spdlog/include",
        "%{prj.name}/src"
    }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"

        defines
        {
            "WF_PLATFORM_WINDOWS",
            "WF_BUILD_DLL"
        }

        postbuildcommands
        {
            ("{COPY} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Sandbox/\"")
        }
    
    filter "configurations:Debug"
    defines "WF_DEBUG"
        symbols "On"
    
    filter "configurations:Release"
        defines "WF_RELEASE"
        optimize "On"
    
    filter "configurations:Dist"
        defines "WF_DIST"
        optimize "On"

project "Sandbox"
        location "Sandbox"
        kind "ConsoleApp"
        language "C++"

        targetdir ("bin/" .. outputdir .. "/%{prj.name}")
        objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

        files
        {
            "%{prj.name}/src/**.h",
            "%{prj.name}/src/**.cpp"
        }

        includedirs
        {
            "Waffle/vendor/spdlog/include",
            "Waffle/src"
        }

        links
        {
            "Waffle"
        }

        filter "system:windows"
            cppdialect "C++20"
            staticruntime "On"
            systemversion "latest"

            defines
            {
                "WF_PLATFORM_WINDOWS"
            }
        
        filter "configurations:Debug"
        defines "WF_DEBUG"
            symbols "On"
        
        filter "configurations:Release"
            defines "WF_RELEASE"
            optimize "On"
        
        filter "configurations:Dist"
            defines "WF_DIST"
            optimize "On"

