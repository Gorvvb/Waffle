#include "wfpch.h"
#include "Project.h"
#include "ProjectSerializer.h"

namespace Waffle {

	Ref<Project> Project::s_ActiveProject = nullptr;

	Ref<Project> Project::GetActive()
	{
		return s_ActiveProject;
	}

	void Project::SetActive(Ref<Project> project)
	{
		s_ActiveProject = project;
	}

	Ref<Project> Project::New()
	{
		s_ActiveProject = CreateRef<Project>();
		return s_ActiveProject;
	}

	Ref<Project> Project::Load(const std::filesystem::path& path)
	{
		Ref<Project> project = CreateRef<Project>();
		ProjectSerializer serializer(project);
		if (serializer.Deserialize(path))
		{
			s_ActiveProject = project;
			return s_ActiveProject;
		}
		return nullptr;
	}

	bool Project::SaveActive(const std::filesystem::path& path)
	{
		if (!s_ActiveProject)
			return false;

		ProjectSerializer serializer(s_ActiveProject);
		return serializer.Serialize(path);
	}

	const std::string& Project::GetProjectName()
	{
		return s_ActiveProject ? s_ActiveProject->m_Config.Name : s_EmptyString;
	}

	std::filesystem::path Project::GetProjectDirectory()
	{
		return s_ActiveProject ? s_ActiveProject->m_Config.ProjectDirectory : std::filesystem::path();
	}

	std::filesystem::path Project::GetAssetDirectory()
	{
		if (!s_ActiveProject) return std::filesystem::path();
		return std::filesystem::path(s_ActiveProject->m_Config.ProjectDirectory) / s_ActiveProject->m_Config.AssetDirectory;
	}

}
