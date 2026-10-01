#pragma once

#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"
#include "Waffle/Scene/Components.h"
#include "CSharpScriptHost.h"

#include <functional>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

namespace Waffle {

	// -------------------------------------------------------------------------
	// CSharpScriptEngine - embedded .NET (CoreCLR) gameplay scripting.
	//
	// Static facade with the same integration surface the LuaScriptEngine used:
	// Scene::OnRuntimeStart/Stop/UpdateRuntime, the editor's play/gizmo/viewport
	// hooks, and exported-runtime quit/scene-index plumbing all call into this
	// class. Managed dispatch + instance registry live on the C# side; this
	// class owns the engine-side bookkeeping (deferred destroys, contact event
	// queueing, debug draw queues, input edge tracking) and the hostfxr bridge.
	// -------------------------------------------------------------------------

	// Editor-only debug gizmos queued from scripts (Gizmos.DrawRay etc.).
	// Drawn by the editor overlay while playing; the exported runtime never
	// draws them and drops them every frame.
	struct DebugDrawLine
	{
		glm::vec2 A;
		glm::vec2 B;
		glm::vec4 Color;
	};

	struct DebugDrawCircle
	{
		glm::vec2 Center;
		float Radius;
		glm::vec4 Color;
	};

	// Delayed-destroy entry used by Scene.DestroyDelayed.
	struct DelayedDestroy
	{
		uint32_t EntityID;
		float Remaining;
	};

	class CSharpScriptEngine
	{
	public:
		// Boots the .NET runtime (idempotent, one attempt per process) and
		// resolves all managed entry points. Returns false - logging why -
		// when the runtime is unavailable; the app runs without scripts.
		static bool Init();
		static void Shutdown();

		// --- scene lifecycle ------------------------------------------------
		// CompileProjectScripts runs first (cheap no-op when sources are
		// unchanged; warns and continues when no SDK is available), then all
		// scripted entities get instances + OnStart.
		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop(Scene* scene);
		static void OnRuntimeUpdate(Scene* scene, Timestep ts);
		static bool IsRuntimeRunning() { return s_RuntimeRunning; }

		// Dispatch queued collision/trigger callbacks. MUST be called after
		// b2World::Step returns (scripts run deferred, never inside the step).
		static void DrainCollisionEvents(Scene* scene);

		// Load + OnStart for one entity - used when prefabs are instantiated
		// at runtime so their scripts run immediately.
		static void InitScriptsForEntity(Scene* scene, Entity entity);

		// --- editor hooks -----------------------------------------------------
		// Unity-style OnDrawGizmos: runs each script's OnDrawGizmos while
		// EDITING (no play). During this pass, mutating bindings are ignored
		// with a warning. Recompiles + reloads when a script file changed.
		static void OnEditorGizmos(Scene* scene);
		static bool IsEditorGizmoPass() { return s_EditorGizmoPass; }

		// Editor field scrape: reflects the public fields of the script class
		// (by type name from the file stem) into sc.Fields, preserving values
		// the editor already stored (UserModified is respected).
		static void ScrapeFieldsFromScript(const std::filesystem::path& fullPath,
			const std::string& scriptPath, ScriptComponent& sc);

		// Live inspector values while playing.
		static bool GetInstanceFieldValue(int handle, const ScriptField& field, ScriptField& outValue);
		static bool SetInstanceFieldValue(int handle, const ScriptField& field);

		// Compile Assets/Scripts/**/*.cs -> Assets/cache/Scripting/GameScripts.dll
		// via the managed compiler service. Returns true when a compiled
		// assembly is available afterwards.
		static bool CompileProjectScripts();

		// Calls handlerName (method name on any script instance) for every
		// scripted instance that defines it, passing the button's entity id.
		// No-op outside runtime or with an empty name.
		static void CallUIHandler(const std::string& handlerName, uint32_t buttonEntityID);

		// Game viewport rect in WINDOW coordinates. The editor sets this
		// every frame; the exported runtime never does (window == viewport).
		// Gameplay mouse queries are viewport-relative and gameplay-vs-GUI
		// input arbitration uses the rect.
		static void SetGameViewport(const glm::vec2& origin, const glm::vec2& size)
		{
			s_GameViewportOrigin = origin;
			s_GameViewportSize = size;
			s_HasGameViewport = true;
		}
		static glm::vec2 GetGameViewportOrigin() { return s_GameViewportOrigin; }
		static glm::vec2 GetGameViewportSize() { return s_GameViewportSize; }
		static bool HasGameViewport() { return s_HasGameViewport; }

		// True when editor chrome (not the game viewport) owns the mouse.
		static bool IsGameplayMouseBlocked();

		// Deferred scene switching + quitting (consumed after the frame).
		static void SetPendingSceneChange(int index) { s_PendingSceneChange = index; }
		static int  GetPendingSceneChange() { return s_PendingSceneChange; }
		static void ClearPendingSceneChange() { s_PendingSceneChange = -1; }

		static void SetCurrentSceneIndex(int index) { s_CurrentSceneIndex = index; }
		static int  GetCurrentSceneIndex() { return s_CurrentSceneIndex; }

		// Quit is DEFERRED: handlers run after the frame.
		static void RequestQuit() { s_QuitRequested = true; }
		static bool IsQuitRequested() { return s_QuitRequested; }
		static void ClearQuitRequest() { s_QuitRequested = false; }

		// Context-aware Quit(): the editor installs a handler that stops play
		// mode; a standalone game has none and terminates the app.
		static void SetQuitHandler(const std::function<void()>& callback) { s_QuitHandler = callback; }

		// OnEnable/OnDisable for script-initiated SetActive transitions.
		static void FireEnable(uint32_t entityId, bool enabled);

		// Managed runtime's last error text (compile failures etc.).
		static std::string LastManagedError();

		// Editor-only debug gizmos (read by the editor overlay each frame).
		static const std::vector<DebugDrawLine>& GetPendingDebugLines() { return s_DebugLines; }
		static const std::vector<DebugDrawCircle>& GetPendingDebugCircles() { return s_DebugCircles; }

		static Scene* GetSceneContext() { return s_SceneContext; }

		static void SetAssetPath(const std::filesystem::path& path) { s_AssetPath = path; }
		static std::filesystem::path GetAssetPath() { return s_AssetPath; }

		// Input edge tracking for IsKeyJustPressed/Released (called by the
		// bindings; snapshots refresh once per frame in OnRuntimeUpdate).
		static void TrackKey(int code);
		static void TrackMouse(int code);
		static void UpdateInputStates();

		static double GetInitDurationMs() { return s_InitDurationMs; }
		static bool IsInitialized() { return s_Initialized; }

		// --- state shared with CSharpScriptBindings.cpp -----------------------
		static Scene*                        s_SceneContext;
		static std::filesystem::path         s_AssetPath;
		static glm::vec2                     s_GameViewportOrigin;
		static glm::vec2                     s_GameViewportSize;
		static bool                          s_HasGameViewport;
		static int                           s_PendingSceneChange;
		static int                           s_CurrentSceneIndex;
		static std::function<void()>         s_QuitHandler;
		static bool                          s_QuitRequested;

		// Per-frame input tracking for just-pressed / just-released.
		static std::unordered_map<int, bool> s_PrevKeyStates;
		static std::unordered_map<int, bool> s_CurrKeyStates;
		static std::unordered_map<int, bool> s_PrevMouseStates;
		static std::unordered_map<int, bool> s_CurrMouseStates;

		// Deferred entity destruction.
		static std::vector<uint32_t>         s_PendingDestroys;
		static std::vector<DelayedDestroy>   s_DelayedDestroys;

		// Editor-only debug gizmo queues.
		static std::vector<DebugDrawLine>    s_DebugLines;
		static std::vector<DebugDrawCircle>  s_DebugCircles;

		static bool                          s_EditorGizmoPass;

	private:
		friend class CSharpContactListener;

		static void ProcessPendingDestroys(Scene* scene);
		static void LoadScriptsForEntity(Scene* scene, Entity entity);
		static void DestroyScriptsForEntity(Scene* scene, Entity entity);

		// --- managed entry points (resolved once at Init) ---
		using wf_init_fn = int (*)(CSharpScriptHost::HostFunctions* hostFns);
		using wf_shutdown_fn = void (*)();
		using wf_frame_fn = void (*)(float dt);
		using wf_runtime_start_fn = void (*)();
		using wf_runtime_stop_fn = void (*)();
		using wf_instance_create_fn = int (*)(uint32_t entityId, const char* typeNameUtf8);
		using wf_instance_start_fn = void (*)(int handle);
		using wf_instance_set_field_fn = void (*)(int handle, const char* nameUtf8, int kind,
			float f, float f2, int i, int b, const char* sUtf8);
		using wf_instance_get_field_fn = int (*)(int handle, const char* nameUtf8, int kind,
			float* f, float* f2, int* i, int* b, char* sBuf, int sBufLen);
		using wf_instance_destroy_fn = void (*)(int handle);
		using wf_entity_update_fn = void (*)(uint32_t entityId, float dt);
		using wf_entity_gizmos_fn = void (*)(uint32_t entityId, const char* typeNameUtf8);
		using wf_gizmo_instances_clear_fn = void (*)();
		using wf_fire_enable_fn = void (*)(uint32_t entityId, int enabled);
		using wf_collision_event_fn = void (*)(uint32_t selfId, uint32_t otherId, int kind);
		using wf_ui_handler_fn = void (*)(const char* handlerNameUtf8, uint32_t buttonEntityId);
		using wf_scrape_fields_fn = int (*)(const char* typeNameUtf8,
			CSharpScriptHost::ScriptFieldDef* outDefs, int maxDefs);
		using wf_compile_scripts_fn = int (*)(const char* sourcesDirUtf8, const char* outputDllUtf8);
		using wf_set_script_assembly_fn = void (*)(const char* assemblyPathUtf8);
		using wf_last_error_fn = int (*)(char* buffer, int bufferLen);

		inline static bool s_Initialized = false;
		inline static bool s_InitAttempted = false;
		inline static double s_InitDurationMs = 0.0;
		inline static bool s_RuntimeRunning = false;
		inline static std::filesystem::file_time_type s_GizmoAssemblyTime{};

		static wf_init_fn                   s_ManagedInit;
		static wf_shutdown_fn               s_ManagedShutdown;
		static wf_frame_fn                  s_ManagedFrame;
		static wf_runtime_start_fn          s_ManagedRuntimeStart;
		static wf_runtime_stop_fn           s_ManagedRuntimeStop;
		static wf_instance_create_fn        s_ManagedInstanceCreate;
		static wf_instance_start_fn         s_ManagedInstanceStart;
		static wf_instance_set_field_fn     s_ManagedInstanceSetField;
		static wf_instance_get_field_fn     s_ManagedInstanceGetField;
		static wf_instance_destroy_fn       s_ManagedInstanceDestroy;
		static wf_entity_update_fn          s_ManagedEntityUpdate;
		static wf_entity_gizmos_fn          s_ManagedEntityGizmos;
		static wf_gizmo_instances_clear_fn  s_ManagedGizmoInstancesClear;
		static wf_fire_enable_fn            s_ManagedFireEnable;
		static wf_collision_event_fn        s_ManagedCollisionEvent;
		static wf_ui_handler_fn             s_ManagedUiHandler;
		static wf_scrape_fields_fn          s_ManagedScrapeFields;
		static wf_compile_scripts_fn        s_ManagedCompileScripts;
		static wf_set_script_assembly_fn    s_ManagedSetScriptAssemblyPath;
		static wf_last_error_fn             s_ManagedLastError;
	};

} // namespace Waffle
