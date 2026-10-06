project "WaffleTests"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	buildoptions { "/utf-8", "/Zc:preprocessor" }
	kind "ConsoleApp"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"Unit/**.h",
		"Unit/**.cpp"
	}

	defines
	{
		"YAML_CPP_STATIC_DEFINE",
		"NOMINMAX",
		"WIN32_LEAN_AND_MEAN",
		"WF_DEBUGBREAK()="
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
		"%{Includedir.yaml_cpp}"
	}

	links
	{
		"Waffle"
	}

	-- shaderc_shared.dll must sit next to the exe (Waffle.lib pulls the shaderc imports).
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
		defines "WF_DEBUG"
		runtime "Debug"
		symbols "on"
		linkoptions { "/IGNORE:4099" }

	filter "configurations:Release or Dist"
		runtime "Release"
		optimize "on"
		linkoptions { "/IGNORE:4099" }
