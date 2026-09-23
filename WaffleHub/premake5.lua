project "WaffleHub"
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
		"../Waffle-Editor/Resources/Icon.rc"
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
		"%{Includedir.ImGui}",
		"%{Includedir.yaml_cpp}"
	}

	links
	{
		"Waffle"
	}

	postbuildcommands
	{
		"{COPYDIR} \"../Waffle-Editor/Resources\" \"%{cfg.targetdir}/Resources\"",
		"{COPYDIR} \"../Waffle-Editor/Assets\" \"%{cfg.targetdir}/Assets\""
	}
	
	-- The exe loads shaderc_shared.dll through the engine's shader compilation;
	-- a player machine has neither the SDK nor it on PATH, so the DLL must
	-- sit next to the exe.
	local shadercDll = os.getenv("VULKAN_SDK")
	shadercDll = shadercDll and (shadercDll .. "/Bin/shaderc_shared.dll") or nil
	if shadercDll and os.isfile(shadercDll) then
		postbuildcommands
		{
			"{COPYFILE} \"" .. shadercDll .. "\" \"%{cfg.targetdir}/shaderc_shared.dll\""
		}
	end

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		kind "WindowedApp"
		defines "WF_DEBUG"
		runtime "Debug"
		symbols "on"
		linkoptions { "/IGNORE:4099" }
		entrypoint "mainCRTStartup"

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
