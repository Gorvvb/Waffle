#include "wfpch.h"
#include "ProjectSerializer.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Waffle {

	ProjectSerializer::ProjectSerializer(Ref<Project> project)
		: m_Project(project)
	{
	}

	bool ProjectSerializer::Serialize(const std::filesystem::path& filepath)
	{
		const auto& config = m_Project->GetConfig();

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << config.Name;
		out << YAML::Key << "AppName" << YAML::Value << config.AppName;
		out << YAML::Key << "AssetDirectory" << YAML::Value << config.AssetDirectory;
		out << YAML::Key << "StartScene" << YAML::Value << config.StartScene;

		out << YAML::Key << "Gravity" << YAML::Value << YAML::Flow << YAML::BeginSeq << config.Gravity.x << config.Gravity.y << YAML::EndSeq;
		out << YAML::Key << "CustomIconPath" << YAML::Value << config.CustomIconPath;

		out << YAML::Key << "SceneList" << YAML::Value << YAML::BeginSeq;
		for (const auto& scene : config.SceneList)
		{
			out << scene;
		}
		out << YAML::EndSeq;

		// Post-processing settings live on the scene's cameras now (CameraComponent), not on the project.

		out << YAML::EndMap; // Project
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
		if (!fout)
		{
			WF_CORE_ERROR("Failed to write project file '{0}' (disk full or file locked?)", filepath.string());
			return false;
		}
		return true;
	}

	bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		auto& config = m_Project->GetConfig();

		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath.string());
		}
		catch (const YAML::Exception& e)
		{
			// BadFile / ParserException / InvalidNode alike: a corrupt project file must fail to load, not crash the app at startup.
			WF_CORE_ERROR("Failed to load project file '{0}': {1}", filepath.string(), e.what());
			return false;
		}

		auto projectNode = data["Project"];
		if (!projectNode)
			return false;

		// Every read below is a conversion that can throw on a truncated or hand-edited file - keep one bad field from killing the whole load.
		try
		{
		if (projectNode["Name"])
			config.Name = projectNode["Name"].as<std::string>();
		else
			config.Name = filepath.stem().string();
		if (projectNode["AppName"])
			config.AppName = projectNode["AppName"].as<std::string>();
		if (projectNode["AssetDirectory"])
			config.AssetDirectory = projectNode["AssetDirectory"].as<std::string>();
		if (projectNode["StartScene"])
			config.StartScene = projectNode["StartScene"].as<std::string>();

		if (projectNode["Gravity"])
		{
			auto gravity = projectNode["Gravity"];
			if (gravity.IsSequence() && gravity.size() >= 2)
			{
				config.Gravity.x = gravity[0].as<float>();
				config.Gravity.y = gravity[1].as<float>();
			}
		}

		if (projectNode["CustomIconPath"])
			config.CustomIconPath = projectNode["CustomIconPath"].as<std::string>();

		if (projectNode["SceneList"])
		{
			config.SceneList.clear();
			for (auto sceneNode : projectNode["SceneList"])
			{
				config.SceneList.push_back(sceneNode.as<std::string>());
			}
		}

		// Legacy "PostProcessing" keys in old project files are ignored - per-camera settings moved into CameraComponent.
		}
		catch (const YAML::Exception& e)
		{
			WF_CORE_ERROR("Project file '{0}' has malformed entries (continuing with defaults): {1}",
				filepath.string(), e.what());
		}

		config.ProjectDirectory = filepath.parent_path().string();
		config.ProjectFileName = filepath.filename().string();
		return true;
	}

}
