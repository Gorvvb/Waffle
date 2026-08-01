project "Waffle"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	buildoptions { "/utf-8" }

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "wfpch.h"
	pchsource "src/wfpch.cpp"

	files
	{
		"src/**.h",
		"src/**.cpp",
		"vendor/stb_image/**.h",
		"vendor/stb_image/**.cpp",
		"vendor/glm/glm/**.hpp",
		"vendor/glm/glm/**.inl",

		"vendor/ImGuizmo/src/ImGuizmo.h",
		"vendor/ImGuizmo/src/ImGuizmo.cpp",
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"VK_NO_PROTOTYPES",
		"GLFW_INCLUDE_NONE",
		"YAML_CPP_STATIC_DEFINE"
	}

	includedirs
	{
		"src",
		"vendor/spdlog/include",
		"%{Includedir.Box2D}",
		"%{Includedir.GLFW}",
		"%{Includedir.GLAD}",
		"%{Includedir.ImGui}",
		"%{Includedir.glm}",
		"%{Includedir.stb_image}",
		"%{Includedir.entt}",
		"%{Includedir.yaml_cpp}",
		"%{Includedir.ImGuizmo}",
		"%{Includedir.Lua}",
		"%{Includedir.VulkanSDK}"
	}

	links
	{
		"Box2D",
		"GLFW",
		"GLAD",
		"ImGui",
		"Lua",
		"opengl32.lib",
		"yaml-cpp",
		"%{Library.Vulkan}"
	}

	filter "files:vendor/ImGuizmo/src/**.cpp"
	enablepch "Off"

	filter "files:vendor/lua/**.c"
	enablepch "Off"

	filter "system:windows"
		systemversion "latest"

		defines
		{
		}
	
	filter "configurations:Debug"
		defines "WF_DEBUG"
		runtime "Debug"
		symbols "on"
		linkoptions { "/IGNORE:4099", "/IGNORE:4006" }

		links
		{
			"%{Library.ShaderC_Debug}",
			"%{Library.SPIRV_Cross_Debug}",
			"%{Library.SPIRV_Cross_GLSL_Debug}"
		}
	
	filter "configurations:Release"
		defines "WF_RELEASE"
		runtime "Release"
		optimize "on"
		linkoptions { "/IGNORE:4099", "/IGNORE:4006" }

		links
		{
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}"
		}
	
	filter "configurations:Dist"
		defines "WF_DIST"
		runtime "Release"
		optimize "on"
		symbols "off"
		linkoptions { "/IGNORE:4099", "/IGNORE:4006" }

		links
		{
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}"
		}