#pragma once

#include <filesystem>

namespace Waffle {

	class ContentBrowserPanel
	{
	private:
		std::filesystem::path m_CurrentDirectory;
	public:
		ContentBrowserPanel();
		void OnImGuiRender();
	};
}