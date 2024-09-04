#pragma once

#include <filesystem>

#include "Waffle/Renderer/Texture.h"

namespace Waffle {

	class ContentBrowserPanel
	{
	private:
		std::filesystem::path m_CurrentDirectory;

		Ref<Texture2D> m_DirectoryIcon;
		Ref<Texture2D> m_FileIcon;
	public:
		ContentBrowserPanel();
		void OnImGuiRender();
	};
}