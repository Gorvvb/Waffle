#pragma once

#include "Waffle/Core/Ref.h"
#include "Waffle/Renderer/PostProcessing.h"
#include <filesystem>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace Waffle {

	struct ProjectConfig
	{
		std::string Name = "UntitledProject";
		std::string AppName = "MyGame";
		std::string AssetDirectory = "Assets";
		std::string StartScene = "Assets/Scenes/SampleScene.waffle";
		std::vector<std::string> SceneList;
		glm::vec2 Gravity = { 0.0f, -9.81f };
		std::string CustomIconPath;

		PostProcessingSettings PostProcessing;

		// Non-serialized runtime properties
		std::string ProjectDirectory;
		std::string ProjectFileName;
	};

	class Project : public RefCounted
	{
	public:
		Project() = default;
		~Project() = default;

		ProjectConfig& GetConfig() { return m_Config; }
		const ProjectConfig& GetConfig() const { return m_Config; }

		static Ref<Project> GetActive();
		static void SetActive(Ref<Project> project);

		static Ref<Project> New();
		static Ref<Project> Load(const std::filesystem::path& path);
		static bool SaveActive(const std::filesystem::path& path);

		static const std::string& GetProjectName();
		static std::filesystem::path GetProjectDirectory();
		static std::filesystem::path GetAssetDirectory();

	private:
		ProjectConfig m_Config;

		static Ref<Project> s_ActiveProject;
		inline static std::string s_EmptyString = "";
	};

}
