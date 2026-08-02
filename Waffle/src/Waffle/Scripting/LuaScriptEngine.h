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

		static void ScrapeFieldsFromScript(const std::filesystem::path& fullPath, const std::string& scriptPath, ScriptComponent& sc);

	private:
		static lua_State* s_LuaState;
		static Scene* s_SceneContext;
		static LuaContactListener* s_ContactListener;
	};

}