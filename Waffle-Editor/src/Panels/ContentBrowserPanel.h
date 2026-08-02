#pragma once

#include <filesystem>

#include "Waffle/Renderer/Texture.h"
#include "Waffle/Renderer/SubTexture2D.h"

namespace Waffle {

	class Scene;
	extern std::filesystem::path g_AssetPath;

	class ContentBrowserPanel
	{
	private:
		std::filesystem::path m_CurrentDirectory;
		Scene* m_SceneContext = nullptr;

		Ref<Texture2D> m_DirectoryIcon;
		Ref<Texture2D> m_FileIcon;
		std::unordered_map<std::string, Ref<Texture2D>> m_TextureCache;

		// Modals / Item Operations
		bool m_OpenScriptModal = false;
		char m_ScriptClassBuffer[128] = "NewScript";

		std::filesystem::path m_ItemToDelete;
		bool m_OpenDeleteModal = false;

		std::filesystem::path m_ItemToRename;
		bool m_OpenRenameModal = false;
		char m_RenameItemBuffer[256] = "";

		// Spritesheet Slicer Modal
		std::filesystem::path m_ItemToSlice;
		bool m_OpenSliceModal = false;
		int m_SliceColumns = 4;
		int m_SliceRows = 4;

		std::filesystem::path m_SelectedItem;

		// Selected Spritesheet Sub-Frame Viewer State
		std::filesystem::path m_SelectedSpritesheetPath;
		Ref<Texture2D> m_SpritesheetTexture;
		int m_SpritesheetCols = 1;
		int m_SpritesheetRows = 1;
		std::vector<Ref<SubTexture2D>> m_SpritesheetSubTextures;

		std::function<void(const std::filesystem::path&)> m_OpenSceneCallback;
		std::function<void(const std::filesystem::path&, const std::filesystem::path&)> m_SceneRenamedCallback;

	public:
		ContentBrowserPanel();
		void OnImGuiRender();

		const std::filesystem::path& GetCurrentDirectory() const { return m_CurrentDirectory; }
		void SetContext(Scene* scene) { m_SceneContext = scene; }
		void SetAssetDirectory(const std::filesystem::path& path);
		void SetOpenSceneCallback(const std::function<void(const std::filesystem::path&)>& callback) { m_OpenSceneCallback = callback; }
		void SetSceneRenamedCallback(const std::function<void(const std::filesystem::path&, const std::filesystem::path&)>& callback) { m_SceneRenamedCallback = callback; }
	};
}