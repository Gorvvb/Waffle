#pragma once

#include <filesystem>

#include "Waffle/Renderer/Texture.h"

namespace Waffle {

	extern std::filesystem::path g_AssetPath;

	class ContentBrowserPanel
	{
	private:
		std::filesystem::path m_CurrentDirectory;

		Ref<Texture2D> m_DirectoryIcon;
		Ref<Texture2D> m_FileIcon;

		// Modals / Item Operations
		bool m_OpenScriptModal = false;
		char m_ScriptClassBuffer[128] = "NewScript";

		std::filesystem::path m_ItemToDelete;
		bool m_OpenDeleteModal = false;

		std::filesystem::path m_ItemToRename;
		bool m_OpenRenameModal = false;
		char m_RenameItemBuffer[256] = "";

		std::filesystem::path m_SelectedItem;

		std::function<void(const std::filesystem::path&)> m_OpenSceneCallback;

	public:
		ContentBrowserPanel();
		void OnImGuiRender();

		const std::filesystem::path& GetCurrentDirectory() const { return m_CurrentDirectory; }
		void SetAssetDirectory(const std::filesystem::path& path);
		void SetOpenSceneCallback(const std::function<void(const std::filesystem::path&)>& callback) { m_OpenSceneCallback = callback; }
	};
}