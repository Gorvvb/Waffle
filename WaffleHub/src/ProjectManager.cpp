#include "wfpch.h"
#include "ProjectManager.h"
#include "Waffle/Core/Log.h"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <chrono>
#include <ctime>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

namespace Waffle {

	std::vector<ProjectEntry> ProjectManager::s_Projects;
	std::filesystem::path ProjectManager::s_ManifestPath = "Projects/projects_manifest.yaml";

	static std::string GetCurrentTimestampString()
	{
		auto now = std::chrono::system_clock::now();
		std::time_t now_c = std::chrono::system_clock::to_time_t(now);
		char buf[100];
		struct tm timeinfo;
		localtime_s(&timeinfo, &now_c);
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &timeinfo);
		return std::string(buf);
	}

	void ProjectManager::Init()
	{
		std::filesystem::create_directories("Projects");
		LoadManifest();
	}

	void ProjectManager::LoadManifest()
	{
		s_Projects.clear();
		if (!std::filesystem::exists(s_ManifestPath))
		{
			// Discover default projects if no manifest exists
			std::filesystem::path defaultProj = "Projects/DefaultProject";
			if (std::filesystem::exists(defaultProj))
			{
				AddOrUpdateProject("DefaultProject", std::filesystem::absolute(defaultProj).string());
			}
			return;
		}

		try
		{
			YAML::Node data = YAML::LoadFile(s_ManifestPath.string());
			auto projectsNode = data["Projects"];
			if (projectsNode && projectsNode.IsSequence())
			{
				for (const auto& item : projectsNode)
				{
					ProjectEntry entry;
					entry.Name = item["Name"] ? item["Name"].as<std::string>() : "Untitled";
					entry.Path = item["Path"] ? item["Path"].as<std::string>() : "";
					entry.LastOpened = item["LastOpened"] ? item["LastOpened"].as<std::string>() : "";
					entry.IconPath = item["IconPath"] ? item["IconPath"].as<std::string>() : "";

					if (!entry.Path.empty() && std::filesystem::exists(entry.Path))
					{
						s_Projects.push_back(entry);
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			WF_CORE_ERROR("ProjectManager: Failed to load manifest: {0}", e.what());
		}
	}

	void ProjectManager::SaveManifest()
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Projects" << YAML::Value << YAML::BeginSeq;

		for (const auto& proj : s_Projects)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << proj.Name;
			out << YAML::Key << "Path" << YAML::Value << proj.Path;
			out << YAML::Key << "LastOpened" << YAML::Value << proj.LastOpened;
			out << YAML::Key << "IconPath" << YAML::Value << proj.IconPath;
			out << YAML::EndMap;
		}

		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::filesystem::create_directories(s_ManifestPath.parent_path());
		std::ofstream fout(s_ManifestPath);
		fout << out.c_str();
	}

	void ProjectManager::AddOrUpdateProject(const std::string& name, const std::string& path, const std::string& iconPath)
	{
		std::filesystem::path absPath = std::filesystem::absolute(path);
		std::string absPathStr = absPath.string();

		for (auto& proj : s_Projects)
		{
			std::error_code ec;
			if (std::filesystem::equivalent(proj.Path, absPathStr, ec))
			{
				proj.Name = name;
				proj.LastOpened = GetCurrentTimestampString();
				if (!iconPath.empty()) proj.IconPath = iconPath;
				SaveManifest();
				return;
			}
		}

		ProjectEntry entry;
		entry.Name = name;
		entry.Path = absPathStr;
		entry.LastOpened = GetCurrentTimestampString();
		entry.IconPath = iconPath;
		s_Projects.insert(s_Projects.begin(), entry);
		SaveManifest();
	}

	void ProjectManager::RemoveProject(size_t index)
	{
		if (index < s_Projects.size())
		{
			s_Projects.erase(s_Projects.begin() + index);
			SaveManifest();
		}
	}

	bool ProjectManager::CreateProjectFromTemplate(const std::string& projectName, const std::filesystem::path& targetParentFolder, const std::string& templateType, std::filesystem::path& outCreatedPath, std::string& outErrorMsg)
	{
		std::filesystem::path targetDir = targetParentFolder / projectName;
		outCreatedPath = std::filesystem::absolute(targetDir);

		if (std::filesystem::exists(targetDir))
		{
			outErrorMsg = "Directory already exists: " + targetDir.string();
			return false;
		}

		std::error_code ec;
		std::filesystem::create_directories(targetDir, ec);
		if (ec)
		{
			outErrorMsg = "Failed to create directory: " + ec.message();
			return false;
		}

		// Locate template folder
		std::vector<std::filesystem::path> templateCandidates = {
			"Resources/Templates/Blank2D",
			"WaffleHub/Resources/Templates/Blank2D",
			"../Resources/Templates/Blank2D",
			"Projects/DefaultProject"
		};

		std::filesystem::path sourceTemplate;
		for (const auto& cand : templateCandidates)
		{
			if (std::filesystem::exists(cand) && std::filesystem::is_directory(cand))
			{
				sourceTemplate = cand;
				break;
			}
		}

		if (!sourceTemplate.empty())
		{
			std::filesystem::copy(sourceTemplate, targetDir,
				std::filesystem::copy_options::overwrite_existing |
				std::filesystem::copy_options::recursive, ec);
		}

		// Ensure Assets directory structure exists
		std::filesystem::create_directories(targetDir / "Assets" / "Scripts", ec);
		std::filesystem::create_directories(targetDir / "Assets" / "Scenes", ec);
		std::filesystem::create_directories(targetDir / "Assets" / "Audio", ec);

		// Write Project.yaml
		std::filesystem::path projYaml = targetDir / "Project.yaml";
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << projectName;
		out << YAML::Key << "Gravity" << YAML::Value << -9.81f;
		out << YAML::Key << "Scenes" << YAML::Value << YAML::BeginSeq;

		// Find any .waffle files inside targetDir
		bool foundScene = false;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(targetDir))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".waffle")
			{
				out << entry.path().string();
				foundScene = true;
				break;
			}
		}
		out << YAML::EndSeq;
		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream fout(projYaml);
		fout << out.c_str();

		AddOrUpdateProject(projectName, outCreatedPath.string());
		return true;
	}

	bool ProjectManager::LaunchEditorForProject(const std::filesystem::path& projectPath)
	{
		std::filesystem::path editorExe = GetEditorExecutablePath();
		if (!std::filesystem::exists(editorExe))
		{
			WF_CORE_ERROR("ProjectManager: Editor executable not found at '{0}'", editorExe.string());
			return false;
		}

#if defined(_WIN32)
		std::string exeStr = std::filesystem::absolute(editorExe).string();
		std::string argsStr = "\"" + std::filesystem::absolute(projectPath).string() + "\"";
		std::string workingDirStr = std::filesystem::absolute(editorExe).parent_path().string();

		HINSTANCE hInst = ShellExecuteA(
			NULL,
			"open",
			exeStr.c_str(),
			argsStr.c_str(),
			workingDirStr.c_str(),
			SW_SHOW
		);

		return (INT_PTR)hInst > 32;
#else
		return false;
#endif
	}

	std::filesystem::path ProjectManager::GetDefaultProjectsDirectory()
	{
		if (std::filesystem::exists("../Projects"))
			return std::filesystem::absolute("../Projects");
		return std::filesystem::absolute("Projects");
	}

	std::filesystem::path ProjectManager::GetEditorExecutablePath()
	{
		std::vector<std::filesystem::path> candidates = {
			"../Waffle-Editor/Waffle-Editor.exe",
			"Waffle-Editor/Waffle-Editor.exe",
			"bin/Release-windows-x86_64/Waffle-Editor/Waffle-Editor.exe",
			"bin/Dist-windows-x86_64/Waffle-Editor/Waffle-Editor.exe",
			"bin/Debug-windows-x86_64/Waffle-Editor/Waffle-Editor.exe",
			"../bin/Debug-windows-x86_64/Waffle-Editor/Waffle-Editor.exe",
			"Waffle-Editor.exe"
		};

		for (const auto& path : candidates)
		{
			if (std::filesystem::exists(path))
				return path;
		}

		return candidates[0];
	}

}
