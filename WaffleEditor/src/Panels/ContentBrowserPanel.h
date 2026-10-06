#pragma once

#include <chrono>
#include <filesystem>

#include "Waffle/Renderer/Texture.h"
#include "Waffle/Renderer/SubTexture2D.h"
#include "Waffle/Scene/Scene.h"
#include <glm/glm.hpp>

namespace Waffle {

	extern std::filesystem::path g_AssetPath;

	class ContentBrowserPanel
	{
	private:
	// Cached directory listing: iterating + stat-ing every frame was wasted I/O.
	// Refreshed on directory change and at most every 500 ms.
	struct BrowserEntry
	{
		std::filesystem::directory_entry Entry;
		std::filesystem::path Path;
		bool IsDirectory = false;
	};
	std::vector<BrowserEntry> m_BrowserEntries;
	std::filesystem::path m_BrowserEntriesDir;
	std::chrono::steady_clock::time_point m_BrowserEntriesRefreshed{};
		std::filesystem::path m_CurrentDirectory;
		// Ref, not a raw pointer: the panel outlives scene swaps - never deref a destroyed scene through a stale context.
		Ref<Scene> m_SceneContext;

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

		// Named-region info (from Spritesheet Editor "Regions" format)
		struct SpritesheetRegionInfo
		{
			std::string Name;
			glm::vec2   Min = { 0.0f, 0.0f };
			glm::vec2   Max = { 0.0f, 0.0f };
			glm::vec2   Pivot = { -1.0f, -1.0f }; // normalized, -1 = unset
		};
		std::vector<SpritesheetRegionInfo> m_SpritesheetRegions; // filled when Regions key present

		struct SpritesheetGroupInfo
		{
			std::string Name;
			std::vector<int> RegionIndices;
		};
		std::vector<SpritesheetGroupInfo> m_SpritesheetGroups;
		bool m_SpritesheetShowSheet = false; // sheet view (overlay) vs grouped grid
		std::string m_SpritesheetTexAbsPath; // cached absolute texture path for drag payloads
		bool m_ShowSpritesheetViewer = false;

		// Loads a .spritesheet (regions, groups, texture) into the viewer.
		void OpenSpritesheetViewer(const std::filesystem::path& sheetPath);

		std::function<void(const std::filesystem::path&)> m_OpenSceneCallback;
		std::function<void(const std::filesystem::path&, const std::filesystem::path&)> m_SceneRenamedCallback;
		std::function<void(const std::filesystem::path&)> m_OpenSpritesheetEditorCallback;
		std::function<void(const std::filesystem::path&)> m_OpenPrefabCallback;

		// Color-swatch thumbnails for prefabs whose renderer only has a color, cached by quantized RGBA.
		Ref<Texture2D> GetColorSwatch(const glm::vec4& color);

		// Prefab thumbnail: the sprite texture tinted with the renderer's color (CPU composite, downsampled) - matches how the sprite renders.
		Ref<Texture2D> GetTintedThumbnail(const std::filesystem::path& texturePath, const glm::vec4& color);

	public:
		ContentBrowserPanel();
		void OnImGuiRender();

		const std::filesystem::path& GetCurrentDirectory() const { return m_CurrentDirectory; }
		void SetContext(const Ref<Scene>& scene) { m_SceneContext = scene; }
		void SetAssetDirectory(const std::filesystem::path& path);
		void SetOpenSceneCallback(const std::function<void(const std::filesystem::path&)>& callback) { m_OpenSceneCallback = callback; }
		void SetSceneRenamedCallback(const std::function<void(const std::filesystem::path&, const std::filesystem::path&)>& callback) { m_SceneRenamedCallback = callback; }
		void SetOpenSpritesheetEditorCallback(const std::function<void(const std::filesystem::path&)>& callback) { m_OpenSpritesheetEditorCallback = callback; }
		void SetOpenPrefabCallback(const std::function<void(const std::filesystem::path&)>& callback) { m_OpenPrefabCallback = callback; }
	};
}