include "./vendor/premake/premake_customization/solution_items.lua"

workspace "Waffle"
	architecture "x64"
	startproject "Waffle-Engine"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	solution_items
	{
		".editorconfig"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

Includedir = {}
Includedir["GLFW"] = "%{wks.location}/Waffle/vendor/GLFW/include"
Includedir["GLAD"] = "%{wks.location}/Waffle/vendor/GLAD/include"
Includedir["ImGui"] = "%{wks.location}/Waffle/vendor/imgui"
Includedir["glm"] = "%{wks.location}/Waffle/vendor/glm"
Includedir["stb_image"] = "%{wks.location}/Waffle/vendor/stb_image"
Includedir["entt"] = "%{wks.location}/Waffle/vendor/entt/include"
Includedir["yaml_cpp"] = "%{wks.location}/Waffle/vendor/yaml-cpp"

group "Dependencies"
	include "Waffle/vendor/GLFW"
	include "Waffle/vendor/GLAD"
	include "Waffle/vendor/imgui"
	include "Waffle/vendor/yaml-cpp"
group ""

include "Waffle"
include "Sandbox"
include "Waffle-Editor"