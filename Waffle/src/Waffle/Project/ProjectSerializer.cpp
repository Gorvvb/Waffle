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
		const auto& pp = config.PostProcessing;

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

		out << YAML::Key << "PostProcessing" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "EnablePostProcessing" << YAML::Value << pp.EnablePostProcessing;
		out << YAML::Key << "EnableBloom" << YAML::Value << pp.EnableBloom;
		out << YAML::Key << "BloomThreshold" << YAML::Value << pp.BloomThreshold;
		out << YAML::Key << "BloomIntensity" << YAML::Value << pp.BloomIntensity;
		out << YAML::Key << "BloomColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << pp.BloomColor.r << pp.BloomColor.g << pp.BloomColor.b << YAML::EndSeq;

		out << YAML::Key << "EnableVignette" << YAML::Value << pp.EnableVignette;
		out << YAML::Key << "VignetteIntensity" << YAML::Value << pp.VignetteIntensity;
		out << YAML::Key << "VignetteSmoothness" << YAML::Value << pp.VignetteSmoothness;
		out << YAML::Key << "VignetteColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << pp.VignetteColor.r << pp.VignetteColor.g << pp.VignetteColor.b << YAML::EndSeq;

		out << YAML::Key << "EnableTonemapping" << YAML::Value << pp.EnableTonemapping;
		out << YAML::Key << "Exposure" << YAML::Value << pp.Exposure;
		out << YAML::Key << "Contrast" << YAML::Value << pp.Contrast;
		out << YAML::Key << "Saturation" << YAML::Value << pp.Saturation;
		out << YAML::Key << "ColorGradingTint" << YAML::Value << YAML::Flow << YAML::BeginSeq << pp.ColorGradingTint.r << pp.ColorGradingTint.g << pp.ColorGradingTint.b << YAML::EndSeq;
		out << YAML::EndMap; // PostProcessing

		out << YAML::EndMap; // Project
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
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
		catch (YAML::ParserException e)
		{
			WF_CORE_ERROR("Failed to load project file '{0}': {1}", filepath.string(), e.what());
			return false;
		}

		auto projectNode = data["Project"];
		if (!projectNode)
			return false;

		config.Name = projectNode["Name"].as<std::string>();
		if (projectNode["AppName"])
			config.AppName = projectNode["AppName"].as<std::string>();
		if (projectNode["AssetDirectory"])
			config.AssetDirectory = projectNode["AssetDirectory"].as<std::string>();
		if (projectNode["StartScene"])
			config.StartScene = projectNode["StartScene"].as<std::string>();

		if (projectNode["Gravity"])
		{
			config.Gravity.x = projectNode["Gravity"][0].as<float>();
			config.Gravity.y = projectNode["Gravity"][1].as<float>();
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

		auto ppNode = projectNode["PostProcessing"];
		if (ppNode)
		{
			auto& pp = config.PostProcessing;
			if (ppNode["EnablePostProcessing"]) pp.EnablePostProcessing = ppNode["EnablePostProcessing"].as<bool>();
			if (ppNode["EnableBloom"]) pp.EnableBloom = ppNode["EnableBloom"].as<bool>();
			if (ppNode["BloomThreshold"]) pp.BloomThreshold = ppNode["BloomThreshold"].as<float>();
			if (ppNode["BloomIntensity"]) pp.BloomIntensity = ppNode["BloomIntensity"].as<float>();
			if (ppNode["BloomColor"])
			{
				pp.BloomColor.r = ppNode["BloomColor"][0].as<float>();
				pp.BloomColor.g = ppNode["BloomColor"][1].as<float>();
				pp.BloomColor.b = ppNode["BloomColor"][2].as<float>();
			}

			if (ppNode["EnableVignette"]) pp.EnableVignette = ppNode["EnableVignette"].as<bool>();
			if (ppNode["VignetteIntensity"]) pp.VignetteIntensity = ppNode["VignetteIntensity"].as<float>();
			if (ppNode["VignetteSmoothness"]) pp.VignetteSmoothness = ppNode["VignetteSmoothness"].as<float>();
			if (ppNode["VignetteColor"])
			{
				pp.VignetteColor.r = ppNode["VignetteColor"][0].as<float>();
				pp.VignetteColor.g = ppNode["VignetteColor"][1].as<float>();
				pp.VignetteColor.b = ppNode["VignetteColor"][2].as<float>();
			}

			if (ppNode["EnableTonemapping"]) pp.EnableTonemapping = ppNode["EnableTonemapping"].as<bool>();
			if (ppNode["Exposure"]) pp.Exposure = ppNode["Exposure"].as<float>();
			if (ppNode["Contrast"]) pp.Contrast = ppNode["Contrast"].as<float>();
			if (ppNode["Saturation"]) pp.Saturation = ppNode["Saturation"].as<float>();
			if (ppNode["ColorGradingTint"])
			{
				pp.ColorGradingTint.r = ppNode["ColorGradingTint"][0].as<float>();
				pp.ColorGradingTint.g = ppNode["ColorGradingTint"][1].as<float>();
				pp.ColorGradingTint.b = ppNode["ColorGradingTint"][2].as<float>();
			}

			PostProcessing::GetSettings() = pp;
		}

		config.ProjectDirectory = filepath.parent_path().string();
		config.ProjectFileName = filepath.filename().string();
		return true;
	}

}
