#pragma once

#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"
#include "LuaIncludes.h"

namespace Waffle {

	class LuaScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop(Scene* scene);
		static void OnRuntimeUpdate(Scene* scene, Timestep ts);

		static Scene* GetSceneContext() { return s_SceneContext; }

	private:
		static lua_State* s_LuaState;
		static Scene* s_SceneContext;
	};

}