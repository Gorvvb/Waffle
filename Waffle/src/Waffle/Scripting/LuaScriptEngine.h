#pragma once

#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"
#include "Waffle/Scene/Components.h"

struct lua_State;

namespace Waffle {

	class LuaContactListener;

	class LuaScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop(Scene* scene);
		static void OnRuntimeUpdate(Scene* scene, Timestep ts);

		static Scene* GetSceneContext() { return s_SceneContext; }
		static lua_State* GetLuaState() { return s_LuaState; }

		static void SetPendingSceneChange(int index) { s_PendingSceneChange = index; }
		static int  GetPendingSceneChange() { return s_PendingSceneChange; }
		static void ClearPendingSceneChange() { s_PendingSceneChange = -1; }

		static void SetCurrentSceneIndex(int index) { s_CurrentSceneIndex = index; }
		static int  GetCurrentSceneIndex() { return s_CurrentSceneIndex; }

		static void ScrapeFieldsFromScript(const std::filesystem::path& fullPath, const std::string& scriptPath, ScriptComponent& sc);

		static void SetAssetPath(const std::filesystem::path& path) { s_AssetPath = path; }
		static std::filesystem::path GetAssetPath() { return s_AssetPath; }

	private:
		static std::filesystem::path s_AssetPath;
		static lua_State* s_LuaState;
		static Scene* s_SceneContext;
		static LuaContactListener* s_ContactListener;
		static int                   s_PendingSceneChange;
		static int                   s_CurrentSceneIndex;
	};

}