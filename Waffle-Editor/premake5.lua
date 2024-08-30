project "Waffle-Editor"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++20"
		staticruntime "on"
		
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
			"%{Includedir.entt}"
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
