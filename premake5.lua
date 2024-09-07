include "./vendor/premake/premake_customization/solution_items.lua"
include "Dependencies.lua"

workspace "Waffle"
	architecture "x64"
	startproject "Waffle-Editor"

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

group "Dependencies"
	include "vendor/premake"
	include "Waffle/vendor/Box2D"
	include "Waffle/vendor/GLFW"
	include "Waffle/vendor/GLAD"
	include "Waffle/vendor/imgui"
	include "Waffle/vendor/yaml-cpp"
group ""

include "Waffle"
include "Sandbox"
include "Waffle-Editor"