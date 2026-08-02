#pragma once

#include <string>
#include <filesystem>

namespace Waffle {

	struct ExportOptions
	{
		std::string ProjectName;
		std::filesystem::path ProjectPath;
		std::string AppName;
		std::string SelectedScenePath;
		std::string CustomIconPath;
	};

	class ProjectExporter
	{
	public:
		using ExportOptions = Waffle::ExportOptions;
		static bool ExportProject(const ExportOptions& options, std::string& outErrorMessage);
	};

}
