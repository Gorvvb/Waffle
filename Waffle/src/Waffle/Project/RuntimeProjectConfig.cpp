#include "wfpch.h"
#include "RuntimeProjectConfig.h"

#include "Waffle/Core/VFS.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Waffle {

	static std::string NormalizeScenePath(std::string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		// Older exports have a leading separator on scene paths - strip it.
		if (!path.empty() && path.front() == '/')
			path.erase(0, 1);
		return path;
	}

	bool WriteRuntimeProjectConfig(const std::filesystem::path& assetsDir, const RuntimeProjectConfig& config)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << config.Name;
		if (!config.StartScene.empty())
			out << YAML::Key << "StartScene" << YAML::Value << NormalizeScenePath(config.StartScene);
		out << YAML::Key << "IconPath" << YAML::Value << config.IconPath;
		out << YAML::Key << "Backend" << YAML::Value
			<< (config.Backend.empty() ? "Vulkan" : config.Backend);
		out << YAML::Key << "Gravity" << YAML::Value << config.GravityY;
		out << YAML::Key << "Scenes" << YAML::Value << YAML::BeginSeq;
		for (const auto& scene : config.Scenes)
			out << NormalizeScenePath(scene);
		out << YAML::EndSeq;
		out << YAML::EndMap;
		out << YAML::EndMap;

		std::filesystem::path path = assetsDir / "project.wproj";
		std::ofstream fout(path);
		if (!fout)
		{
			WF_CORE_ERROR("Failed to write runtime project config '{0}'", path.string());
			return false;
		}
		fout << out.c_str();
		return true;
	}

	const RuntimeProjectConfig& GetRuntimeProjectConfig()
	{
		static RuntimeProjectConfig s_Config;
		static bool s_Parsed = false;
		if (s_Parsed)
			return s_Config;
		s_Parsed = true;

		std::string text = VFS::ReadFileAsString("Assets/project.wproj");
		if (text.empty())
			return s_Config;

		try
		{
			YAML::Node data = YAML::Load(text);
			YAML::Node project = data["Project"];
			if (!project)
				return s_Config;

			if (project["Name"])
				s_Config.Name = project["Name"].as<std::string>();
			if (project["StartScene"])
				s_Config.StartScene = NormalizeScenePath(project["StartScene"].as<std::string>());
			if (project["IconPath"])
				s_Config.IconPath = project["IconPath"].as<std::string>();
			if (project["Backend"])
				s_Config.Backend = project["Backend"].as<std::string>();
			if (project["Gravity"])
				s_Config.GravityY = project["Gravity"].as<float>();
			if (project["Scenes"])
			{
				for (auto node : project["Scenes"])
				{
					std::string path = NormalizeScenePath(node.as<std::string>());
					if (!path.empty())
						s_Config.Scenes.push_back(path);
				}
			}
		}
		catch (const std::exception& e)
		{
			WF_CORE_ERROR("Failed to parse project.wproj: {0}", e.what());
		}
		return s_Config;
	}

}
