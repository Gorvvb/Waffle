#pragma once

#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"
#include "Waffle/Scene/Components.h"

#include <unordered_map>
#include <vector>

struct lua_State;

namespace Waffle {

	class LuaContactListener;

	// -------------------------------------------------------------------------
	// Timer entry used by SetTimer / CancelTimer
	// -------------------------------------------------------------------------
	struct LuaTimerEntry
	{
		float    Remaining;
		float    Delay;      // original delay (unused after creation, kept for reference)
		int      CallbackRef; // luaL_ref into LUA_REGISTRYINDEX
		bool     Active;
		uint32_t ID;
	};

	// -------------------------------------------------------------------------
	// Delayed-destroy entry used by DestroyEntityDelayed
	// -------------------------------------------------------------------------
	struct LuaDelayedDestroy
	{
		uint32_t EntityID;
		float    Remaining;
	};

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

		// Initialize Lua scripts for a single entity — used when prefabs are
		// instantiated at runtime so their scripts get loaded and OnCreate fired.
		static void InitScriptsForEntity(Scene* scene, Entity entity);

		static void SetAssetPath(const std::filesystem::path& path) { s_AssetPath = path; }
		static std::filesystem::path GetAssetPath() { return s_AssetPath; }

		// Input tracking helpers — called by free C-bindings inside the same TU
		static void TrackKey(int code);
		static void TrackMouse(int code);
		static void UpdateInputStates();

	// All state is public so Lua C-binding free functions in LuaScriptEngine.cpp can access it.
	public:
		static std::filesystem::path         s_AssetPath;
		static lua_State*                    s_LuaState;
		static Scene*                        s_SceneContext;
		static LuaContactListener*           s_ContactListener;
		static int                           s_PendingSceneChange;
		static int                           s_CurrentSceneIndex;

		// Per-frame input tracking for IsKeyJustPressed / IsKeyJustReleased
		static std::unordered_map<int, bool> s_PrevKeyStates;
		static std::unordered_map<int, bool> s_CurrKeyStates;
		static std::unordered_map<int, bool> s_PrevMouseStates;
		static std::unordered_map<int, bool> s_CurrMouseStates;

		// Timer system
		static std::vector<LuaTimerEntry>    s_Timers;
		static uint32_t                      s_NextTimerID;

		// Deferred / delayed entity destruction
		static std::vector<uint32_t>         s_PendingDestroys;
		static std::vector<LuaDelayedDestroy> s_DelayedDestroys;

		// Current frame delta time (exposed to Lua via GetDeltaTime())
		static float                         s_CurrentDeltaTime;
	};

}