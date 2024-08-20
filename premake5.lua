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

Includedir = {}
Includedir["GLFW"] = "Waffle/vendor/GLFW/include"
Includedir["GLAD"] = "Waffle/vendor/GLAD/include"
Includedir["ImGui"] = "Waffle/vendor/imgui/"
Includedir["glm"] = "Waffle/vendor/glm"

include "Waffle/vendor/GLFW"
include "Waffle/vendor/GLAD"
include "Waffle/vendor/imgui"

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
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/vendor/glm/glm/**.hpp",
        "%{prj.name}/vendor/glm/glm/**.inl"
    }

    includedirs
    {
        "%{prj.name}/vendor/spdlog/include",
        "%{prj.name}/src",
        "%{Includedir.GLFW}",
        "%{Includedir.GLAD}",
        "%{Includedir.ImGui}",
        "%{Includedir.glm}"
    }

    links
    {
        "GLFW",
        "GLAD",
        "ImGui",
        "opengl32.lib"
    }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"

        defines
        {
            "WF_PLATFORM_WINDOWS",
            "WF_BUILD_DLL",
            "GLFW_INCLUDE_NONE"
        }

        postbuildcommands
        {
            ("{COPY} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Sandbox/\"")
        }
    
    filter "configurations:Debug"
        defines "WF_DEBUG"
        buildoptions "/MDd"
        symbols "On"
    
    filter "configurations:Release"
        defines "WF_RELEASE"
        buildoptions "/MD"
        optimize "On"
    
    filter "configurations:Dist"
        defines "WF_DIST"
        buildoptions "/MD"
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
            "Waffle/src",
			"Waffle/vendor",
            "%{Includedir.glm}"
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
        buildoptions "/MDd"
            symbols "On"
        
        filter "configurations:Release"
            defines "WF_RELEASE"
            buildoptions "/MD"
            optimize "On"
        
        filter "configurations:Dist"
            defines "WF_DIST"
            buildoptions "/MD"
            optimize "On"
