project "Waffle-Editor"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++20"
		staticruntime "off"
		
		targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
		objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

		files
		{
			"src/**.h",
			"src/**.cpp"
		}

		includedirs
		{
			"%{wks.location}/Waffle/vendor/spdlog/include",
			"%{wks.location}/Waffle/src",
			"%{wks.location}/Waffle/vendor",
			"%{Includedir.glm}",
			"%{Includedir.entt}",
			"%{Includedir.ImGuizmo}"
		}

		links
		{
			"Waffle"
		}

		filter "system:windows"
			systemversion "latest"
		
		filter "configurations:Debug"
			defines "WF_DEBUG"
			runtime "Debug"
			symbols "on"
			linkoptions { "/IGNORE:4099" }
		
		filter "configurations:Release"
			defines "WF_RELEASE"
			runtime "Release"
			optimize "on"
			linkoptions { "/IGNORE:4099", "/SUBSYSTEM:WINDOWS" }
			entrypoint "mainCRTStartup"
		
		filter "configurations:Dist"
			defines "WF_DIST"
			runtime "Release"
			optimize "on"
			linkoptions { "/IGNORE:4099", "/SUBSYSTEM:WINDOWS" }
			entrypoint "mainCRTStartup"
