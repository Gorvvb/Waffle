#include "wfpch.h"
#include "CSharpScriptEngine.h"

#include "Waffle/Core/Log.h"
#include "Waffle/Core/VFS.h"
#include "Waffle/Core/Input.h"

#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <cstdlib>

#include <box2d/b2_body.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_world.h>
#include <box2d/b2_world_callbacks.h>
#include <box2d/b2_contact.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>
#include <imgui.h>

namespace Waffle {

	namespace
	{
		// True only while an ImGui text field owns the keyboard - typing in
		// editor UI must not steer gameplay.
		bool GameplayKeyboardBlocked()
		{
			return ImGui::GetIO().WantTextInput;
		}

		// --- minimal platform layer so the host stays portable (Linux port
		// only needs these four; hostfxr itself is cross-platform) ---
#if defined(WF_PLATFORM_WINDOWS)
		void* LoadSharedLibrary(const std::filesystem::path& path)
		{
			return reinterpret_cast<void*>(::LoadLibraryW(path.c_str()));
		}
		void* GetSymbol(void* library, const char* name)
		{
			return reinterpret_cast<void*>(::GetProcAddress(reinterpret_cast<HMODULE>(library), name));
		}
		std::filesystem::path ExecutablePath()
		{
			wchar_t path[MAX_PATH] = {};
			if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0)
				return {};
			return std::filesystem::path(path);
		}
		std::string Utf8Path(const std::filesystem::path& path)
		{
			std::u8string u8 = path.u8string();
			return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
		}
#else
		void* LoadSharedLibrary(const std::filesystem::path& path)
		{
			return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
		}
		void* GetSymbol(void* library, const char* name)
		{
			return dlsym(library, name);
		}
		std::filesystem::path ExecutablePath()
		{
			char buffer[4096];
			ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
			if (length <= 0)
				return {};
			buffer[length] = '\0';
			return std::filesystem::path(buffer);
		}
		std::string Utf8Path(const std::filesystem::path& path)
		{
			return path.native();
		}
#endif

#if defined(WF_PLATFORM_WINDOWS)
		constexpr const char* k_HostFxrFileName = "hostfxr.dll";
#else
		constexpr const char* k_HostFxrFileName = "libhostfxr.so";
#endif

		hostfxr_initialize_for_runtime_config_fn  s_InitForConfig = nullptr;
		hostfxr_get_runtime_delegate_fn           s_GetDelegate = nullptr;
		hostfxr_close_fn                          s_CloseHost = nullptr;
		hostfxr_handle                            s_Context = nullptr;
		load_assembly_and_get_function_pointer_fn s_LoadAndGetFn = nullptr;

		// Version string "10.0.12" -> {10,0,12,0}; longer/shorter forms padded.
		bool ParseVersion(const std::string& text, unsigned long long out[4])
		{
			out[0] = out[1] = out[2] = out[3] = 0;
			int index = 0;
			for (char c : text)
			{
				if (c == '.') { if (++index > 3) return false; continue; }
				if (c < '0' || c > '9') return false;
				out[index] = out[index] * 10 + (c - '0');
			}
			return index > 0;
		}

		bool VersionLess(const unsigned long long a[4], const unsigned long long b[4])
		{
			for (int i = 0; i < 4; i++)
			{
				if (a[i] != b[i]) return a[i] < b[i];
			}
			return false;
		}

		// Highest-versioned hostfxr under <dotnet root>/host/fxr, where the
		// root is tried as: the exe's own folder (self-contained layout),
		// $DOTNET_ROOT, then the system install locations.
		std::filesystem::path FindHostFxr()
		{
			std::vector<std::filesystem::path> roots;

			std::filesystem::path exePath = ExecutablePath();
			if (!exePath.empty())
				roots.push_back(exePath.parent_path());

			if (const char* env = std::getenv("DOTNET_ROOT"); env && *env)
				roots.push_back(env);

			if (const char* pf = std::getenv("ProgramFiles"); pf && *pf)
				roots.push_back(std::filesystem::path(pf) / "dotnet");
			roots.push_back("/usr/share/dotnet"); // Linux default install (absent on Windows)

			std::error_code ec;
			for (const auto& root : roots)
			{
				std::filesystem::path fxrDir = root / "host" / "fxr";
				if (!std::filesystem::exists(fxrDir, ec))
					continue;

				unsigned long long best[4] = { 0, 0, 0, 0 };
				std::filesystem::path bestPath;
				for (const auto& entry : std::filesystem::directory_iterator(fxrDir, ec))
				{
					std::filesystem::path dll = entry.path() / k_HostFxrFileName;
					if (!std::filesystem::exists(dll, ec))
						continue;

					unsigned long long version[4];
					if (!ParseVersion(entry.path().filename().string(), version))
						continue;
					if (bestPath.empty() || VersionLess(best, version))
					{
						std::copy(std::begin(version), std::end(version), std::begin(best));
						bestPath = dll;
					}
				}
				if (!bestPath.empty())
					return bestPath;
			}
			return {};
		}

		bool LoadHostFxr()
		{
			std::filesystem::path hostFxrPath = FindHostFxr();
			if (hostFxrPath.empty())
			{
				WF_CORE_WARN("CSharpScriptEngine: no hostfxr found (looked next to the exe, $DOTNET_ROOT, system installs) - C# scripting disabled");
				return false;
			}

			void* library = LoadSharedLibrary(hostFxrPath);
			if (!library)
			{
				WF_CORE_ERROR("CSharpScriptEngine: failed to load '{0}'", hostFxrPath.string());
				return false;
			}

			s_InitForConfig = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
				GetSymbol(library, "hostfxr_initialize_for_runtime_config"));
			s_GetDelegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
				GetSymbol(library, "hostfxr_get_runtime_delegate"));
			s_CloseHost = reinterpret_cast<hostfxr_close_fn>(
				GetSymbol(library, "hostfxr_close"));

			if (!s_InitForConfig || !s_GetDelegate || !s_CloseHost)
			{
				WF_CORE_ERROR("CSharpScriptEngine: hostfxr is missing required exports");
				return false;
			}
			return true;
		}

		// ScriptingRuntime folder: contract assembly + runtimeconfig + the
		// project's GameScripts.dll. Env override wins, then the packaged
		// layout next to the exe, then the repo dev layout, then the cwd.
		std::filesystem::path FindScriptingRuntimeDir()
		{
			std::vector<std::filesystem::path> candidates;

			if (const char* env = std::getenv("WAFFLE_SCRIPT_RUNTIME"); env && *env)
				candidates.push_back(env);

			std::filesystem::path exePath = ExecutablePath();
			if (!exePath.empty())
			{
				std::filesystem::path exeDir = exePath.parent_path();
				candidates.push_back(exeDir / "ScriptingRuntime");
				// <repo>/bin/<cfg>-Windows-x64/<app>/exe -> <repo>/bin/ScriptingRuntime
				candidates.push_back(exeDir / ".." / ".." / "ScriptingRuntime");
			}

			std::error_code ec;
			candidates.push_back(std::filesystem::current_path() / "ScriptingRuntime");

			for (const auto& candidate : candidates)
			{
				std::filesystem::path dir = std::filesystem::weakly_canonical(candidate, ec);
				if (std::filesystem::exists(dir / "Waffle.Scripting.runtimeconfig.json", ec) &&
					std::filesystem::exists(dir / "Waffle.Scripting.dll", ec))
					return dir;
			}

			WF_CORE_INFO("CSharpScriptEngine: no ScriptingRuntime folder with Waffle.Scripting.runtimeconfig.json found (set WAFFLE_SCRIPT_RUNTIME to override) - C# scripting disabled");
			return {};
		}

		constexpr const wchar_t* s_ManagedType = L"Waffle.Scripting.Internal.WaffleNative, Waffle.Scripting";

		bool GetManagedFn(const std::filesystem::path& contractDll, const char* methodUtf8, void** outFn)
		{
			// hostfxr takes wide method/type names on Windows.
			std::wstring method(methodUtf8, methodUtf8 + std::strlen(methodUtf8));
			int rc = s_LoadAndGetFn(contractDll.c_str(), s_ManagedType, method.c_str(),
				UNMANAGEDCALLERSONLY_METHOD, nullptr, outFn);
			if (rc != 0)
			{
				WF_CORE_ERROR("CSharpScriptEngine: resolving '{0}' failed with hostfxr error 0x{1:08x}", methodUtf8, (unsigned)rc);
				return false;
			}
			return true;
		}

		std::filesystem::path s_ContractDll;

		bool LoadHostFxrAndContract();

		// Where the contract assembly + staged GameScripts.dll live (found
		// once at Init, reused by the packed-export fallback).
		std::filesystem::path s_ScriptingRuntimeDir;

		bool LoadHostFxrAndContract()
		{
			if (!LoadHostFxr())
				return false;

			std::filesystem::path runtimeDir = FindScriptingRuntimeDir();
			if (runtimeDir.empty())
				return false;

			std::filesystem::path configPath = runtimeDir / "Waffle.Scripting.runtimeconfig.json";
			std::filesystem::path contractDll = runtimeDir / "Waffle.Scripting.dll";

			hostfxr_handle context = nullptr;
			// Second arg = optional hostfxr_initialize_parameters (nullptr = defaults).
			int rc = s_InitForConfig(configPath.c_str(), nullptr, &context);
			if (rc != 0 || !context)
			{
				WF_CORE_ERROR("CSharpScriptEngine: hostfxr_initialize_for_runtime_config('{0}') failed with 0x{1:08x}",
					configPath.string(), (unsigned)rc);
				return false;
			}
			s_Context = context;

			rc = s_GetDelegate(context, hdt_load_assembly_and_get_function_pointer, reinterpret_cast<void**>(&s_LoadAndGetFn));
			if (rc != 0 || !s_LoadAndGetFn)
			{
				WF_CORE_ERROR("CSharpScriptEngine: hostfxr_get_runtime_delegate failed with 0x{0:08x}", (unsigned)rc);
				return false;
			}

			s_ContractDll = contractDll;
			s_ScriptingRuntimeDir = runtimeDir;
			return true;
		}


		// Script file path (relative to the asset root, with or without
		// extension) -> full path on disk.
		std::filesystem::path ResolveScriptPath(const std::string& scriptPath)
		{
			std::filesystem::path normalized = std::filesystem::path(scriptPath).make_preferred();
			std::filesystem::path assetRoot = CSharpScriptEngine::GetAssetPath();

			if (VFS::Exists(normalized))
				return normalized;

			std::filesystem::path fromAssets = assetRoot / normalized;
			if (VFS::Exists(fromAssets))
				return fromAssets;

			if (!fromAssets.has_extension())
			{
				std::filesystem::path withExt = fromAssets;
				withExt += ".cs";
				if (VFS::Exists(withExt))
					return withExt;
			}

			if (std::filesystem::exists(assetRoot))
			{
				std::string searchFilename = normalized.filename().string();
				if (searchFilename.find(".cs") == std::string::npos)
					searchFilename += ".cs";

				for (auto& entry : std::filesystem::recursive_directory_iterator(assetRoot))
				{
					if (entry.is_regular_file() && entry.path().filename().string() == searchFilename)
						return entry.path();
				}
			}

			return normalized;
		}

	} // anonymous namespace

	// Defined in CSharpScriptBindings.cpp.
	void PopulateHostFunctions(CSharpScriptHost::HostFunctions& fns);

	// --- static state ---------------------------------------------------------

	Scene* CSharpScriptEngine::s_SceneContext = nullptr;
	std::filesystem::path CSharpScriptEngine::s_AssetPath = "Assets";
	glm::vec2 CSharpScriptEngine::s_GameViewportOrigin = glm::vec2(0.0f);
	glm::vec2 CSharpScriptEngine::s_GameViewportSize = glm::vec2(0.0f);
	bool CSharpScriptEngine::s_HasGameViewport = false;
	int CSharpScriptEngine::s_PendingSceneChange = -1;
	int CSharpScriptEngine::s_CurrentSceneIndex = 0;
	std::function<void()> CSharpScriptEngine::s_QuitHandler;
	bool CSharpScriptEngine::s_QuitRequested = false;

	std::unordered_map<int, bool> CSharpScriptEngine::s_PrevKeyStates;
	std::unordered_map<int, bool> CSharpScriptEngine::s_CurrKeyStates;
	std::unordered_map<int, bool> CSharpScriptEngine::s_PrevMouseStates;
	std::unordered_map<int, bool> CSharpScriptEngine::s_CurrMouseStates;

	std::vector<uint32_t> CSharpScriptEngine::s_PendingDestroys;
	std::vector<DelayedDestroy> CSharpScriptEngine::s_DelayedDestroys;

	std::vector<DebugDrawLine> CSharpScriptEngine::s_DebugLines;
	std::vector<DebugDrawCircle> CSharpScriptEngine::s_DebugCircles;

	bool CSharpScriptEngine::s_EditorGizmoPass = false;

	CSharpScriptEngine::wf_init_fn                   CSharpScriptEngine::s_ManagedInit = nullptr;
	CSharpScriptEngine::wf_shutdown_fn               CSharpScriptEngine::s_ManagedShutdown = nullptr;
	CSharpScriptEngine::wf_frame_fn                  CSharpScriptEngine::s_ManagedFrame = nullptr;
	CSharpScriptEngine::wf_runtime_start_fn          CSharpScriptEngine::s_ManagedRuntimeStart = nullptr;
	CSharpScriptEngine::wf_runtime_stop_fn           CSharpScriptEngine::s_ManagedRuntimeStop = nullptr;
	CSharpScriptEngine::wf_instance_create_fn        CSharpScriptEngine::s_ManagedInstanceCreate = nullptr;
	CSharpScriptEngine::wf_instance_start_fn         CSharpScriptEngine::s_ManagedInstanceStart = nullptr;
	CSharpScriptEngine::wf_instance_set_field_fn     CSharpScriptEngine::s_ManagedInstanceSetField = nullptr;
	CSharpScriptEngine::wf_instance_get_field_fn     CSharpScriptEngine::s_ManagedInstanceGetField = nullptr;
	CSharpScriptEngine::wf_instance_destroy_fn       CSharpScriptEngine::s_ManagedInstanceDestroy = nullptr;
	CSharpScriptEngine::wf_entity_update_fn          CSharpScriptEngine::s_ManagedEntityUpdate = nullptr;
	CSharpScriptEngine::wf_entity_gizmos_fn          CSharpScriptEngine::s_ManagedEntityGizmos = nullptr;
	CSharpScriptEngine::wf_gizmo_instances_clear_fn  CSharpScriptEngine::s_ManagedGizmoInstancesClear = nullptr;
	CSharpScriptEngine::wf_fire_enable_fn            CSharpScriptEngine::s_ManagedFireEnable = nullptr;
	CSharpScriptEngine::wf_collision_event_fn        CSharpScriptEngine::s_ManagedCollisionEvent = nullptr;
	CSharpScriptEngine::wf_ui_handler_fn             CSharpScriptEngine::s_ManagedUiHandler = nullptr;
	CSharpScriptEngine::wf_scrape_fields_fn          CSharpScriptEngine::s_ManagedScrapeFields = nullptr;
	CSharpScriptEngine::wf_compile_scripts_fn        CSharpScriptEngine::s_ManagedCompileScripts = nullptr;
	CSharpScriptEngine::wf_set_script_assembly_fn    CSharpScriptEngine::s_ManagedSetScriptAssemblyPath = nullptr;
	CSharpScriptEngine::wf_last_error_fn             CSharpScriptEngine::s_ManagedLastError = nullptr;

	// Reads the managed runtime's last error text (via the LastError entry).
	std::string CSharpScriptEngine::LastManagedError()
	{
		char buffer[1024] = {};
		if (s_ManagedLastError && s_ManagedLastError(buffer, sizeof(buffer)) > 0)
			return buffer;
		return "unknown error";
	}

	// --- contact listener ---------------------------------------------------------

	// Collision events are queued during b2World::Step and dispatched after it -
	// running scripts mid-solve (body create/destroy/mutate) is UB in Box2D.
	class CSharpContactListener : public b2ContactListener
	{
	public:
		struct ContactEvent
		{
			int      Kind;
			uint32_t SelfID;
			uint32_t OtherID;
		};

		void BeginContact(b2Contact* contact) override
		{
			QueueContact(contact, CSharpScriptHost::Contact_CollisionBegin, CSharpScriptHost::Contact_TriggerBegin);
		}
		void EndContact(b2Contact* contact) override
		{
			QueueContact(contact, CSharpScriptHost::Contact_CollisionEnd, CSharpScriptHost::Contact_TriggerEnd);
		}

		void DrainEvents(Scene* scene)
		{
			if (m_Events.empty())
				return;
			if (!CSharpScriptEngine::s_ManagedCollisionEvent)
			{
				m_Events.clear();
				return;
			}

			// Swap so scripts fired here that produce new contacts append to the
			// next batch instead of mutating the vector being iterated.
			std::vector<ContactEvent> events = std::move(m_Events);
			m_Events.clear();

			for (const auto& ev : events)
			{
				if (!scene->GetRegistry().valid((entt::entity)ev.SelfID))
					continue;
				if (!scene->GetRegistry().all_of<ScriptComponent>((entt::entity)ev.SelfID))
					continue;
				CSharpScriptEngine::s_ManagedCollisionEvent(ev.SelfID, ev.OtherID, ev.Kind);
			}
		}

	private:
		void QueueContact(b2Contact* contact, int collisionKind, int triggerKind)
		{
			Scene* scene = CSharpScriptEngine::GetSceneContext();
			if (!scene)
				return;

			auto& bodyMap = scene->GetBodyEntityMap();
			b2Body* bodyA = contact->GetFixtureA()->GetBody();
			b2Body* bodyB = contact->GetFixtureB()->GetBody();
			auto itA = bodyMap.find(bodyA);
			auto itB = bodyMap.find(bodyB);
			if (itA == bodyMap.end() || itB == bodyMap.end())
				return;

			bool isSensor = contact->GetFixtureA()->IsSensor() || contact->GetFixtureB()->IsSensor();
			int kind = isSensor ? triggerKind : collisionKind;

			m_Events.push_back({ kind, itA->second, itB->second });
			m_Events.push_back({ kind, itB->second, itA->second });
		}

		std::vector<ContactEvent> m_Events;
	};

	static CSharpContactListener* s_ContactListener = nullptr;

	// --- init / shutdown -----------------------------------------------------------

	bool CSharpScriptEngine::Init()
	{
		if (s_Initialized)
			return true;
		if (s_InitAttempted)
			return false;
		s_InitAttempted = true;

		auto t0 = std::chrono::steady_clock::now();

		if (!LoadHostFxrAndContract())
			return false;

		// Resolve every managed entry point (names are ABI - see WaffleNative.cs).
		struct Entry { const char* Name; void** Out; };
		Entry entries[] = {
			{ "Init",                 reinterpret_cast<void**>(&s_ManagedInit) },
			{ "Shutdown",             reinterpret_cast<void**>(&s_ManagedShutdown) },
			{ "Frame",                reinterpret_cast<void**>(&s_ManagedFrame) },
			{ "RuntimeStart",         reinterpret_cast<void**>(&s_ManagedRuntimeStart) },
			{ "RuntimeStop",          reinterpret_cast<void**>(&s_ManagedRuntimeStop) },
			{ "InstanceCreate",       reinterpret_cast<void**>(&s_ManagedInstanceCreate) },
			{ "InstanceStart",        reinterpret_cast<void**>(&s_ManagedInstanceStart) },
			{ "InstanceSetField",     reinterpret_cast<void**>(&s_ManagedInstanceSetField) },
			{ "InstanceGetField",     reinterpret_cast<void**>(&s_ManagedInstanceGetField) },
			{ "InstanceDestroy",      reinterpret_cast<void**>(&s_ManagedInstanceDestroy) },
			{ "EntityUpdate",         reinterpret_cast<void**>(&s_ManagedEntityUpdate) },
			{ "EntityGizmos",         reinterpret_cast<void**>(&s_ManagedEntityGizmos) },
			{ "GizmoInstancesClear",  reinterpret_cast<void**>(&s_ManagedGizmoInstancesClear) },
			{ "FireEnable",           reinterpret_cast<void**>(&s_ManagedFireEnable) },
			{ "CollisionEvent",       reinterpret_cast<void**>(&s_ManagedCollisionEvent) },
			{ "UiHandler",            reinterpret_cast<void**>(&s_ManagedUiHandler) },
			{ "ScrapeFields",         reinterpret_cast<void**>(&s_ManagedScrapeFields) },
			{ "CompileScripts",       reinterpret_cast<void**>(&s_ManagedCompileScripts) },
			{ "SetScriptAssemblyPath", reinterpret_cast<void**>(&s_ManagedSetScriptAssemblyPath) },
			{ "LastError",            reinterpret_cast<void**>(&s_ManagedLastError) },
		};
		for (const auto& entry : entries)
		{
			if (!GetManagedFn(s_ContractDll, entry.Name, entry.Out))
				return false;
		}

		CSharpScriptHost::HostFunctions hostFns{};
		PopulateHostFunctions(hostFns);

		if (s_ManagedInit(&hostFns) != 0)
		{
			WF_CORE_ERROR("CSharpScriptEngine: managed Init failed: {0}", LastManagedError());
			return false;
		}

		s_Initialized = true;
		std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - t0;
		s_InitDurationMs = elapsed.count();
		WF_CORE_INFO("CSharpScriptEngine: .NET runtime started in {0:.1f} ms", s_InitDurationMs);
		return true;
	}

	void CSharpScriptEngine::Shutdown()
	{
		if (s_Initialized && s_ManagedShutdown)
			s_ManagedShutdown();

		s_Initialized = false;
		s_ManagedInit = nullptr;
		s_ManagedShutdown = nullptr;
		s_ManagedFrame = nullptr;
		s_ManagedRuntimeStart = nullptr;
		s_ManagedRuntimeStop = nullptr;
		s_ManagedInstanceCreate = nullptr;
		s_ManagedInstanceStart = nullptr;
		s_ManagedInstanceSetField = nullptr;
		s_ManagedInstanceGetField = nullptr;
		s_ManagedInstanceDestroy = nullptr;
		s_ManagedEntityUpdate = nullptr;
		s_ManagedEntityGizmos = nullptr;
		s_ManagedGizmoInstancesClear = nullptr;
		s_ManagedFireEnable = nullptr;
		s_ManagedCollisionEvent = nullptr;
		s_ManagedUiHandler = nullptr;
		s_ManagedScrapeFields = nullptr;
		s_ManagedCompileScripts = nullptr;
		s_ManagedSetScriptAssemblyPath = nullptr;
		s_ManagedLastError = nullptr;
		s_LoadAndGetFn = nullptr;

		// Closing the context releases hostfxr's bookkeeping; the runtime
		// itself stays in the process (unloading CoreCLR is not supported).
		if (s_Context && s_CloseHost)
			s_CloseHost(s_Context);
		s_Context = nullptr;
	}

	// --- script compilation -------------------------------------------------------------

	bool CSharpScriptEngine::CompileProjectScripts()
	{
		if (!s_Initialized || !s_ManagedCompileScripts)
			return false;

		if (s_AssetPath.empty())
			return false;

		std::filesystem::path sourcesDir = s_AssetPath / "Scripts";
		if (!std::filesystem::exists(sourcesDir))
		{
			// Packed export: there is no physical Assets/Scripts folder and
			// nothing to compile - the pre-compiled assembly ships next to
			// ScriptingRuntime. Point the managed runtime at it directly.
			for (const std::filesystem::path& candidate : {
				s_ScriptingRuntimeDir / "GameScripts" / "GameScripts.dll",
				s_ScriptingRuntimeDir / "GameScripts.dll" })
			{
				if (std::filesystem::exists(candidate))
				{
					std::string utf8 = Utf8Path(candidate);
					s_ManagedSetScriptAssemblyPath(utf8.c_str());
					return true;
				}
			}
			return false;
		}

		std::filesystem::path outputDll = s_AssetPath / "cache" / "Scripting" / "GameScripts.dll";
		std::string srcUtf8 = Utf8Path(sourcesDir);
		std::string outUtf8 = Utf8Path(outputDll);

		int rc = s_ManagedCompileScripts(srcUtf8.c_str(), outUtf8.c_str());
		if (rc == 0 || rc == 2)
			return std::filesystem::exists(outputDll);
		if (rc == -2)
		{
			WF_CORE_WARN("CSharpScriptEngine: cannot compile scripts - no .NET SDK found; using last compiled assembly");
			return std::filesystem::exists(outputDll);
		}

		WF_CORE_ERROR("CSharpScriptEngine: script compilation failed: {0}", LastManagedError());
		return std::filesystem::exists(outputDll);
	}

	// --- scene lifecycle -------------------------------------------------------------

	void CSharpScriptEngine::LoadScriptsForEntity(Scene* scene, Entity entity)
	{
		if (!s_Initialized || !s_ManagedInstanceCreate || !entity)
			return;

		auto& sc = entity.GetComponent<ScriptComponent>();
		sc.ScriptHandles.clear();

		std::vector<std::string> scriptsToLoad = sc.ScriptPaths;
		if (scriptsToLoad.empty() && !sc.ClassName.empty())
			scriptsToLoad.push_back(sc.ClassName);

		for (const auto& scriptPath : scriptsToLoad)
		{
			if (scriptPath.empty())
				continue;

			// The class comes from the compiled assembly, not the source
			// file: in packed exports the .cs sources live inside the archive
			// (or nowhere loose), so a failed resolve must not skip the
			// instance - the stem still names the class.
			std::filesystem::path fullPath = ResolveScriptPath(scriptPath);
			std::string typeName = VFS::Exists(fullPath)
				? fullPath.stem().string()
				: std::filesystem::path(scriptPath).stem().string();

			int handle = s_ManagedInstanceCreate((uint32_t)(entt::entity)entity, typeName.c_str());
			if (handle < 0)
			{
				WF_CORE_WARN("CSharpScriptEngine: no script class '{0}' found (from '{1}')", typeName, scriptPath);
				sc.ScriptHandles.push_back(-1);
				continue;
			}

			// Apply the editor-stored field values onto the fresh instance.
			auto fieldsIt = sc.Fields.find(scriptPath);
			if (fieldsIt != sc.Fields.end())
			{
				for (const auto& field : fieldsIt->second)
				{
					s_ManagedInstanceSetField(handle, field.Name.c_str(), (int)field.Type,
						field.FloatVal, field.FloatVal2, field.IntVal, field.BoolVal ? 1 : 0,
						field.StringVal.c_str());
				}
			}

			s_ManagedInstanceStart(handle);
			WF_CORE_INFO("CSharpScriptEngine: '{0}' -> {1} (entity {2})", scriptPath, typeName, (uint32_t)(entt::entity)entity);
			sc.ScriptHandles.push_back(handle);
		}
	}

	void CSharpScriptEngine::DestroyScriptsForEntity(Scene* scene, Entity entity)
	{
		if (!entity)
			return;
		auto& sc = entity.GetComponent<ScriptComponent>();
		for (int handle : sc.ScriptHandles)
		{
			if (handle >= 0 && s_ManagedInstanceDestroy)
				s_ManagedInstanceDestroy(handle);
		}
		sc.ScriptHandles.clear();
	}

	void CSharpScriptEngine::OnRuntimeStart(Scene* scene)
	{
		s_DebugLines.clear();
		s_DebugCircles.clear();
		Init();
		s_SceneContext = scene;
		if (!scene || !s_Initialized)
			return;

		// Fresh assembly load every play: compile-on-play doubles as the
		// editor's hot reload; the exported runtime short-circuits when the
		// cached assembly is already up to date.
		CompileProjectScripts();

		s_ManagedRuntimeStart();

		// Snapshot BEFORE scripts: OnStart can CreateEntity/InstantiatePrefab,
		// reallocating the pool under a live view iterator (UB).
		std::vector<entt::entity> scriptedEntities;
		{
			auto view = scene->m_Registry.view<ScriptComponent>();
			for (auto entityID : view)
				scriptedEntities.push_back(entityID);
		}

		s_RuntimeRunning = true;
		for (auto entityID : scriptedEntities)
		{
			if (!scene->m_Registry.valid(entityID))
				continue;
			LoadScriptsForEntity(scene, Entity{ entityID, scene });
		}

		if (scene->GetPhysicsWorld())
		{
			s_ContactListener = new CSharpContactListener();
			scene->GetPhysicsWorld()->SetContactListener(s_ContactListener);
		}
	}

	void CSharpScriptEngine::InitScriptsForEntity(Scene* scene, Entity entity)
	{
		if (!scene || !s_Initialized || !s_RuntimeRunning || !entity)
			return;
		if (!scene->m_Registry.all_of<ScriptComponent>((entt::entity)(uint32_t)entity))
			return;
		LoadScriptsForEntity(scene, entity);
	}

	void CSharpScriptEngine::OnRuntimeStop(Scene* scene)
	{
		s_DebugLines.clear();
		s_DebugCircles.clear();
		s_RuntimeRunning = false;

		if (!scene || !s_Initialized)
		{
			s_SceneContext = nullptr;
			return;
		}

		// Snapshot BEFORE scripts: OnDestroy can Create/DestroyEntity, mutating
		// the pool under a live view iterator (UB).
		std::vector<entt::entity> scriptedEntities;
		{
			auto view = scene->m_Registry.view<ScriptComponent>();
			for (auto entityID : view)
				scriptedEntities.push_back(entityID);
		}

		for (auto entityID : scriptedEntities)
		{
			if (!scene->m_Registry.valid(entityID))
				continue;
			DestroyScriptsForEntity(scene, Entity{ entityID, scene });
		}

		if (scene->GetPhysicsWorld())
			scene->GetPhysicsWorld()->SetContactListener(nullptr);
		delete s_ContactListener;
		s_ContactListener = nullptr;

		s_PendingDestroys.clear();
		s_DelayedDestroys.clear();

		// Drops the script ALC (Unload) + gizmo instances - PersistentData
		// survives, exactly like the old Global table.
		if (s_ManagedRuntimeStop)
			s_ManagedRuntimeStop();

		s_SceneContext = nullptr;
	}

	void CSharpScriptEngine::ProcessPendingDestroys(Scene* scene)
	{
		// Swap first: OnDestroy can DestroyEntity (push_back into the vector
		// being iterated = UB); new requests wait for the next frame.
		std::vector<uint32_t> pendingDestroys = std::move(s_PendingDestroys);
		s_PendingDestroys.clear();

		for (uint32_t id : pendingDestroys)
		{
			entt::entity e = (entt::entity)id;
			if (scene->m_Registry.valid(e))
			{
				Entity entity{ e, scene };
				// Fire OnDestroy and drop the instance - prevents stale handles
				// colliding with recycled entity ids.
				if (scene->m_Registry.all_of<ScriptComponent>(e))
					DestroyScriptsForEntity(scene, entity);
				scene->DestroyEntity(entity);
			}
		}
	}

	void CSharpScriptEngine::OnRuntimeUpdate(Scene* scene, Timestep ts)
	{
		if (!scene || !s_Initialized || !s_RuntimeRunning)
			return;

		UpdateInputStates();

		// Debug gizmos last a single frame: the editor drains them in its
		// overlay pass; the exported runtime never reads them.
		s_DebugLines.clear();
		s_DebugCircles.clear();

		// --- Script update (skip disabled entities) - snapshot BEFORE scripts:
		// OnUpdate can CreateEntity/InstantiatePrefab (UB under a live view).
		std::vector<entt::entity> scriptedEntities;
		{
			auto view = scene->m_Registry.view<ScriptComponent>();
			for (auto entityID : view)
			{
				if (!scene->m_Registry.all_of<DisabledComponent>(entityID))
					scriptedEntities.push_back(entityID);
			}
		}

		for (auto entityID : scriptedEntities)
		{
			// Re-check: an earlier script this frame may have destroyed or
			// disabled this entity.
			if (!scene->m_Registry.valid(entityID))
				continue;
			if (scene->m_Registry.all_of<DisabledComponent>(entityID))
				continue;

			s_ManagedEntityUpdate((uint32_t)entityID, (float)ts);
		}

		// Timers pump (C#-side) - runs even when no entities are scripted.
		s_ManagedFrame((float)ts);

		// --- Tick delayed destroys ---
		for (auto& d : s_DelayedDestroys)
			d.Remaining -= (float)ts;
		for (auto& d : s_DelayedDestroys)
		{
			if (d.Remaining <= 0.0f)
				s_PendingDestroys.push_back(d.EntityID);
		}
		s_DelayedDestroys.erase(
			std::remove_if(s_DelayedDestroys.begin(), s_DelayedDestroys.end(),
				[](const DelayedDestroy& d) { return d.Remaining <= 0.0f; }),
			s_DelayedDestroys.end());

		ProcessPendingDestroys(scene);
	}

	void CSharpScriptEngine::DrainCollisionEvents(Scene* scene)
	{
		if (s_ContactListener && scene)
			s_ContactListener->DrainEvents(scene);
	}

	void CSharpScriptEngine::FireEnable(uint32_t entityId, bool enabled)
	{
		// OnEnable / OnDisable fire only for script-initiated SetActive
		// transitions (the SetActive binding routes through here).
		if (s_Initialized && s_RuntimeRunning && s_ManagedFireEnable)
			s_ManagedFireEnable(entityId, enabled ? 1 : 0);
	}

	// --- editor hooks -----------------------------------------------------------------

	void CSharpScriptEngine::OnEditorGizmos(Scene* scene)
	{
		if (!scene)
			return;
		if (!s_Initialized && !Init())
			return;
		if (!s_Initialized)
			return;

		s_DebugLines.clear();
		s_DebugCircles.clear();

		// Recompile + drop cached gizmo instances when any script changed on
		// disk (mirrors the Lua gizmo-env hot reload).
		{
			bool stale = false;
			std::error_code ec;
			std::filesystem::path assemblyDll = s_AssetPath / "cache" / "Scripting" / "GameScripts.dll";
			std::filesystem::file_time_type assemblyTime = std::filesystem::exists(assemblyDll, ec)
				? std::filesystem::last_write_time(assemblyDll, ec) : std::filesystem::file_time_type{};
			if (std::filesystem::exists(s_AssetPath / "Scripts", ec))
			{
				for (auto& entry : std::filesystem::recursive_directory_iterator(s_AssetPath / "Scripts", ec))
				{
					if (!entry.is_regular_file(ec) || entry.path().extension() != ".cs")
						continue;
					if (assemblyTime < std::filesystem::last_write_time(entry.path(), ec))
					{
						stale = true;
						break;
					}
				}
			}
			if (stale)
			{
				WF_CORE_INFO("CSharpScriptEngine: script changes detected - recompiling...");
				CompileProjectScripts();
				if (s_ManagedGizmoInstancesClear)
					s_ManagedGizmoInstancesClear();
			}
		}

		auto view = scene->m_Registry.view<ScriptComponent>();
		Scene* previousContext = s_SceneContext;
		s_SceneContext = scene;
		s_EditorGizmoPass = true;

		for (auto entityID : view)
		{
			if (!scene->m_Registry.valid(entityID))
				continue;
			if (scene->m_Registry.all_of<DisabledComponent>(entityID))
				continue;

			auto& sc = scene->m_Registry.get<ScriptComponent>(entityID);
			std::vector<std::string> scripts = sc.ScriptPaths;
			if (scripts.empty() && !sc.ClassName.empty())
				scripts.push_back(sc.ClassName);

			for (const auto& scriptPath : scripts)
			{
				if (scriptPath.empty())
					continue;
				std::filesystem::path fullPath = ResolveScriptPath(scriptPath);
				if (!VFS::Exists(fullPath))
					continue;
				s_ManagedEntityGizmos((uint32_t)entityID, fullPath.stem().string().c_str());
			}
		}

		s_EditorGizmoPass = false;
		s_SceneContext = previousContext;
	}

	// --- field scrape + live inspector values ---------------------------------------------

	static_assert(sizeof(CSharpScriptHost::ScriptFieldDef) == 64 + 4 + 4 + 4 + 4 + 4 + 256 + 4 + 4 + 4 + 128,
		"ScriptFieldDef layout drift - mirror in ScriptRuntime.cs (StructLayout Sequential)");

	void CSharpScriptEngine::ScrapeFieldsFromScript(const std::filesystem::path& fullPath,
		const std::string& scriptPath, ScriptComponent& sc)
	{
		if (!s_Initialized || !s_ManagedScrapeFields)
			return;

		// The type must exist in a compiled assembly - compile when missing or stale.
		{
			std::error_code ec;
			std::filesystem::path assemblyDll = s_AssetPath / "cache" / "Scripting" / "GameScripts.dll";
			std::filesystem::file_time_type assemblyTime = std::filesystem::exists(assemblyDll, ec)
				? std::filesystem::last_write_time(assemblyDll, ec) : std::filesystem::file_time_type{};
			if (!std::filesystem::exists(assemblyDll, ec) ||
				assemblyTime < std::filesystem::last_write_time(fullPath, ec))
			{
				CompileProjectScripts();
			}
		}

		CSharpScriptHost::ScriptFieldDef defs[64];
		int count = s_ManagedScrapeFields(fullPath.stem().string().c_str(), defs, 64);
		if (count <= 0)
			return;

		// Don't overwrite fields that were already loaded (e.g. from the scene
		// file); respect UserModified like the Lua scraper did.
		auto& fields = sc.Fields[scriptPath];
		for (int i = 0; i < count; i++)
		{
			const auto& def = defs[i];
			std::string name = def.Name;

			auto existing = std::find_if(fields.begin(), fields.end(),
				[&name](const ScriptField& f) { return f.Name == name; });
			if (existing != fields.end())
			{
				existing->HasRange = def.HasRange != 0;
				existing->RangeMin = def.RangeMin;
				existing->RangeMax = def.RangeMax;
				existing->Tooltip = def.Tooltip;
				continue;
			}

			ScriptField field;
			field.Name = name;
			field.Type = (ScriptFieldType)def.Kind;
			field.HasRange = def.HasRange != 0;
			field.RangeMin = def.RangeMin;
			field.RangeMax = def.RangeMax;
			field.Tooltip = def.Tooltip;
			switch (field.Type)
			{
			case ScriptFieldType::Float:  field.FloatVal = def.DefaultFloat; break;
			case ScriptFieldType::Vec2:   field.FloatVal = def.DefaultFloat; field.FloatVal2 = def.DefaultFloat2; break;
			case ScriptFieldType::Int:    field.IntVal = def.DefaultInt; break;
			case ScriptFieldType::Bool:   field.BoolVal = def.DefaultBool != 0; break;
			case ScriptFieldType::String: field.StringVal = def.DefaultString; break;
			}
			fields.push_back(field);
		}
	}

	bool CSharpScriptEngine::GetInstanceFieldValue(int handle, const ScriptField& field, ScriptField& outValue)
	{
		if (!s_Initialized || !s_ManagedInstanceGetField || handle < 0)
			return false;
		char stringBuffer[256] = {};
		float f = 0, f2 = 0;
		int i = 0, b = 0;
		int kind = (int)field.Type;
		if (!s_ManagedInstanceGetField(handle, field.Name.c_str(), kind,
			&f, &f2, &i, &b, stringBuffer, sizeof(stringBuffer)))
			return false;

		outValue = field;
		switch (field.Type)
		{
		case ScriptFieldType::Float:  outValue.FloatVal = f; break;
		case ScriptFieldType::Vec2:   outValue.FloatVal = f; outValue.FloatVal2 = f2; break;
		case ScriptFieldType::Int:    outValue.IntVal = i; break;
		case ScriptFieldType::Bool:   outValue.BoolVal = b != 0; break;
		case ScriptFieldType::String: outValue.StringVal = stringBuffer; break;
		}
		return true;
	}

	bool CSharpScriptEngine::SetInstanceFieldValue(int handle, const ScriptField& field)
	{
		if (!s_Initialized || !s_ManagedInstanceSetField || handle < 0)
			return false;
		s_ManagedInstanceSetField(handle, field.Name.c_str(), (int)field.Type,
			field.FloatVal, field.FloatVal2, field.IntVal, field.BoolVal ? 1 : 0,
			field.StringVal.c_str());
		return true;
	}

	// --- misc -------------------------------------------------------------------------

	void CSharpScriptEngine::CallUIHandler(const std::string& handlerName, uint32_t buttonEntityID)
	{
		if (!s_Initialized || !s_SceneContext || !s_RuntimeRunning || handlerName.empty() || !s_ManagedUiHandler)
			return;
		s_ManagedUiHandler(handlerName.c_str(), buttonEntityID);
	}

	bool CSharpScriptEngine::IsGameplayMouseBlocked()
	{
		// Input arbitration: WantCaptureMouse covers ANY ImGui window (incl.
		// the viewport), so editor gameplay input is gated only outside the
		// game viewport rect; the exported runtime has no viewport rect and
		// is never gated.
		ImGuiIO& io = ImGui::GetIO();
		if (!io.WantCaptureMouse)
			return false;
		if (!HasGameViewport())
			return false;

		glm::vec2 m = Input::GetMousePosition();
		glm::vec2 o = GetGameViewportOrigin();
		glm::vec2 sz = GetGameViewportSize();
		if (sz.x <= 0.0f || sz.y <= 0.0f)
			return false;
		bool insideGame = (m.x >= o.x && m.y >= o.y && m.x < o.x + sz.x && m.y < o.y + sz.y);
		return !insideGame;
	}

	void CSharpScriptEngine::TrackKey(int code)
	{
		if (s_CurrKeyStates.find(code) == s_CurrKeyStates.end())
		{
			s_CurrKeyStates[code] = Input::IsKeyPressed((KeyCode)code);
			s_PrevKeyStates[code] = false;
		}
	}

	void CSharpScriptEngine::TrackMouse(int code)
	{
		if (s_CurrMouseStates.find(code) == s_CurrMouseStates.end())
		{
			s_CurrMouseStates[code] = Input::IsMouseButtonPressed((MouseCode)code);
			s_PrevMouseStates[code] = false;
		}
	}

	void CSharpScriptEngine::UpdateInputStates()
	{
		for (auto& [key, curr] : s_CurrKeyStates)
		{
			s_PrevKeyStates[key] = curr;
			curr = Input::IsKeyPressed((KeyCode)key);
		}
		for (auto& [btn, curr] : s_CurrMouseStates)
		{
			s_PrevMouseStates[btn] = curr;
			curr = Input::IsMouseButtonPressed((MouseCode)btn);
		}
	}

} // namespace Waffle
