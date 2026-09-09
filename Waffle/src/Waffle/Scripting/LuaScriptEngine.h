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

		// Dispatch queued collision/trigger callbacks. MUST be called after
		// b2World::Step returns (scripts run deferred, never inside the step).
		static void DrainCollisionEvents(Scene* scene);

		// Game viewport rect in WINDOW coordinates. The editor sets this
		// every frame (the viewport is a sub-region of the window); the
		// exported runtime never does - there window == viewport and the
		// origin stays (0,0). Gameplay mouse queries (GetMousePosition) are
		// returned relative to the origin so scripts behave identically in
		// both, and gameplay-vs-GUI input arbitration uses the rect.
		static void SetGameViewport(const glm::vec2& origin, const glm::vec2& size)
		{
			s_GameViewportOrigin = origin;
			s_GameViewportSize = size;
			s_HasGameViewport = true;
		}
		static glm::vec2 GetGameViewportOrigin() { return s_GameViewportOrigin; }
		static glm::vec2 GetGameViewportSize() { return s_GameViewportSize; }
		static bool HasGameViewport() { return s_HasGameViewport; }

		static Scene* GetSceneContext() { return s_SceneContext; }
		static lua_State* GetLuaState() { return s_LuaState; }

		static void SetPendingSceneChange(int index) { s_PendingSceneChange = index; }
		static int  GetPendingSceneChange() { return s_PendingSceneChange; }
		static void ClearPendingSceneChange() { s_PendingSceneChange = -1; }

		static void SetCurrentSceneIndex(int index) { s_CurrentSceneIndex = index; }
		static int  GetCurrentSceneIndex() { return s_CurrentSceneIndex; }

		static void ScrapeFieldsFromScript(const std::filesystem::path& fullPath, const std::string& scriptPath, ScriptComponent& sc);

		// Initialize Lua scripts for a single entity - used when prefabs are
		// instantiated at runtime so their scripts get loaded and OnCreate fired.
		static void InitScriptsForEntity(Scene* scene, Entity entity);

		static void SetAssetPath(const std::filesystem::path& path) { s_AssetPath = path; }
		static std::filesystem::path GetAssetPath() { return s_AssetPath; }

		// Input tracking helpers - called by free C-bindings inside the same TU
		static void TrackKey(int code);
		static void TrackMouse(int code);
		static void UpdateInputStates();

	// All state is public so Lua C-binding free functions in LuaScriptEngine.cpp can access it.
	public:
		static std::filesystem::path         s_AssetPath;
		static lua_State*                    s_LuaState;
		static Scene*                        s_SceneContext;
		static LuaContactListener*           s_ContactListener;
		// Game viewport rect in window coords - see SetGameViewport.
		static glm::vec2                     s_GameViewportOrigin;
		static glm::vec2                     s_GameViewportSize;
		static bool                          s_HasGameViewport;
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