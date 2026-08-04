#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace Waffle {

	struct ProjectEntry
	{
		std::string Name;
		std::string Path;
		std::string LastOpened;
		std::string IconPath;
	};

	class ProjectManager
	{
	public:
		static void Init();
		static void SaveManifest();
		static void LoadManifest();

		static std::vector<ProjectEntry>& GetProjects() { return s_Projects; }
		static void AddOrUpdateProject(const std::string& name, const std::string& path, const std::string& iconPath = "");
		static void RemoveProject(size_t index);

		static bool CreateProjectFromTemplate(const std::string& projectName, const std::filesystem::path& targetParentFolder, const std::string& templateType, std::filesystem::path& outCreatedPath, std::string& outErrorMsg);

		static bool LaunchEditorForProject(const std::filesystem::path& projectPath);

		static std::filesystem::path GetDefaultProjectsDirectory();
		static std::filesystem::path GetEditorExecutablePath();

	private:
		static std::vector<ProjectEntry> s_Projects;
		static std::filesystem::path s_ManifestPath;
	};

}
