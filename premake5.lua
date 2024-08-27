workspace "Waffle"
    architecture "x64"
    startproject "Waffle-Engine"

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
Includedir["ImGui"] = "Waffle/vendor/imgui"
Includedir["glm"] = "Waffle/vendor/glm"
Includedir["stb_image"] = "Waffle/vendor/stb_image"

group "Dependencies"
    include "Waffle/vendor/GLFW"
    include "Waffle/vendor/GLAD"
    include "Waffle/vendor/imgui"
group ""

project "Waffle"
    location "Waffle"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "wfpch.h"
    pchsource "Waffle/src/wfpch.cpp"

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/vendor/stb_image/**.h",
        "%{prj.name}/vendor/stb_image/**.cpp",
        "%{prj.name}/vendor/glm/glm/**.hpp",
        "%{prj.name}/vendor/glm/glm/**.inl"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE"
    }

    includedirs
    {
        "%{prj.name}/vendor/spdlog/include",
        "%{prj.name}/src",
        "%{Includedir.GLFW}",
        "%{Includedir.GLAD}",
        "%{Includedir.ImGui}",
        "%{Includedir.glm}",
        "%{Includedir.stb_image}"
    }

    links
    {
        "GLFW",
        "GLAD",
        "ImGui",
        "opengl32.lib"
    }

    filter "system:windows"
        systemversion "latest"

        defines
        {
        }
    
    filter "configurations:Debug"
        defines "WF_DEBUG"
        symbols "on"
    
    filter "configurations:Release"
        defines "WF_RELEASE"
        optimize "on"
    
    filter "configurations:Dist"
        defines "WF_DIST"
        optimize "on"

project "Sandbox"
        location "Sandbox"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++20"
        staticruntime "on"

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
            systemversion "latest"
        
        filter "configurations:Debug"
        defines "WF_DEBUG"
            symbols "on"
        
        filter "configurations:Release"
            defines "WF_RELEASE"
            optimize "on"
        
        filter "configurations:Dist"
            defines "WF_DIST"
            optimize "on"

project "Waffle-Editor"
        location "Waffle-Editor"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++20"
        staticruntime "on"

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
            systemversion "latest"
        
        filter "configurations:Debug"
        defines "WF_DEBUG"
            symbols "on"
        
        filter "configurations:Release"
            defines "WF_RELEASE"
            optimize "on"
        
        filter "configurations:Dist"
            defines "WF_DIST"
            optimize "on"
