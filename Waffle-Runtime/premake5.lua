project "Waffle-Runtime"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	buildoptions { "/utf-8" }

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp",
		"Resources/Icon.rc"
	}

	defines
	{
		"YAML_CPP_STATIC_DEFINE"
	}

	includedirs
	{
		"%{wks.location}/Waffle/vendor/spdlog/include",
		"%{wks.location}/Waffle/src",
		"%{wks.location}/Waffle/vendor",
		"%{Includedir.GLFW}",
		"%{Includedir.GLAD}",
		"%{Includedir.glm}",
		"%{Includedir.entt}",
		"%{Includedir.Lua}",
		"%{Includedir.yaml_cpp}"
	}

	links
	{
		"Waffle"
	}

	postbuildcommands
	{
		"{COPYDIR} \"../Waffle-Editor/Assets\" \"%{cfg.targetdir}/Assets\""
	}

	filter "system:windows"
		systemversion "latest"
	
	filter "configurations:Debug"
		kind "ConsoleApp"
		defines "WF_DEBUG"
		runtime "Debug"
		symbols "on"
		linkoptions { "/IGNORE:4099" }
	
	filter "configurations:Release"
		kind "WindowedApp"
		defines "WF_RELEASE"
		runtime "Release"
		optimize "on"
		linkoptions { "/IGNORE:4099" }
		entrypoint "mainCRTStartup"
	
	filter "configurations:Dist"
		kind "WindowedApp"
		defines "WF_DIST"
		runtime "Release"
		optimize "on"
		linkoptions { "/IGNORE:4099" }
		entrypoint "mainCRTStartup"
