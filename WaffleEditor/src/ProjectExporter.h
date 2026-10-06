#pragma once

#include <string>
#include <filesystem>

namespace Waffle {

	struct ExportOptions
	{
		std::string ProjectName;
		std::filesystem::path ProjectPath;
		std::string AppName;
		std::string CustomIconPath;
		std::string SelectedScenePath;
		std::vector<std::string> SceneList;
		float Gravity = -9.8f;
	};

	class ProjectExporter
	{
	public:
		using ExportOptions = Waffle::ExportOptions;
		static bool ExportProject(const ExportOptions& options, std::string& outErrorMessage);
	};

}
