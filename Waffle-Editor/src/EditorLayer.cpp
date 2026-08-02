#include "EditorLayer.h"
#include "ProjectExporter.h"

#include "Waffle/Scene/SceneSerializer.h"
#include "Waffle/Utils/PlatformUtils.h"
#include "Waffle/Math/Math.h"

#include "Waffle/Scripting/LuaScriptEngine.h"
#include <Box2D/include/box2d/box2d.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>
#include <ImGuizmo.h>

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Waffle {

	extern std::filesystem::path g_AssetPath;

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_CameraController(1280.0f / 720.0f)
	{}

	// -------------------------------------------------------------------------
	// Lifecycle
	// -------------------------------------------------------------------------

	void EditorLayer::OnAttach()
	{
		WF_PROFILE_FUNCTION();

		// Route all spdlog and Lua script logs directly to in-editor ConsolePanel
		Log::SetLogCallback([](int level, const std::string& msg) {
			ConsoleMessage::Level lvl = ConsoleMessage::Level::Info;
			if (level <= 1) lvl = ConsoleMessage::Level::Trace; // trace / debug
			else if (level == 2) lvl = ConsoleMessage::Level::Info;
			else if (level == 3) lvl = ConsoleMessage::Level::Warn;
			else if (level == 4) lvl = ConsoleMessage::Level::Error;
			else if (level >= 5) lvl = ConsoleMessage::Level::Critical;

			ConsolePanel::AddMessage(lvl, msg);
		});

		// Toolbar icons
		m_IconPlay = Texture2D::Create("Resources/Icons/PlayButton.png");
		m_IconStop = Texture2D::Create("Resources/Icons/StopButton.png");
		m_IconPause = Texture2D::Create("Resources/Icons/PauseButton.png");
		m_IconStep = Texture2D::Create("Resources/Icons/StepButton.png");
		m_IconPauseInactive = Texture2D::Create("Resources/Icons/PauseButtonInactive.png");
		m_IconStepInactive = Texture2D::Create("Resources/Icons/StepButtonInactive.png");

		// Gizmo icons
		m_IconNoGizmo = Texture2D::Create("Resources/Icons/Gizmos/NoGizmo.png");
		m_IconTransformGizmo = Texture2D::Create("Resources/Icons/Gizmos/TransformGizmo.png");
		m_IconRotationGizmo = Texture2D::Create("Resources/Icons/Gizmos/RotationGizmo.png");
		m_IconScaleGizmo = Texture2D::Create("Resources/Icons/Gizmos/ScaleGizmo.png");
		m_IconNoGizmoActive = Texture2D::Create("Resources/Icons/Gizmos/NoGizmoActive.png");
		m_IconTransformGizmoActive = Texture2D::Create("Resources/Icons/Gizmos/TransformGizmoActive.png");
		m_IconRotationGizmoActive = Texture2D::Create("Resources/Icons/Gizmos/RotationGizmoActive.png");
		m_IconScaleGizmoActive = Texture2D::Create("Resources/Icons/Gizmos/ScaleGizmoActive.png");

		// Framebuffer
		FramebufferSpecification fbSpec;
		fbSpec.Attachments = {
			FramebufferTextureFormat::RGBA8,
			FramebufferTextureFormat::RED_INTEGER,
			FramebufferTextureFormat::Depth
		};
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbSpec);

		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;

		// Open project / scene from command line or default project
		auto commandLineArgs = Application::Get().GetSpecification().CommandLineArgs;
		if (commandLineArgs.Count > 1)
		{
			OpenScene(commandLineArgs[1]);
		}
		else
		{
			LoadEditorConfig();
		}

		m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);
		Renderer2D::SetLineWidth(4.0f);
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_ContentBrowserPanel.SetContext(m_ActiveScene.get());
		m_AnimationEditorPanel.SetContext(m_ActiveScene);

		m_ContentBrowserPanel.SetOpenSceneCallback(
			[this](const std::filesystem::path& path) { OpenScene(path); });

		m_ContentBrowserPanel.SetSceneRenamedCallback(
			[this](const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
			{
				// Compare case-insensitively on Windows
				std::string oldAbs = std::filesystem::absolute(oldPath).string();
				std::string currAbs = !m_EditorScenePath.empty()
					? std::filesystem::absolute(m_EditorScenePath).string() : "";
				for (auto& c : oldAbs)  c = (char)tolower(c);
				for (auto& c : currAbs) c = (char)tolower(c);

				if (!currAbs.empty() && oldAbs == currAbs)
				{
					m_EditorScenePath = newPath;
					if (m_EditorScene)
					{
						m_EditorScene->SetName(newPath.stem().string());
						SerializeScene(m_EditorScene, newPath);
					}
					UpdateWindowTitle();
				}
				else
				{
					Ref<Scene> tempScene = CreateRef<Scene>();
					SceneSerializer serializer(tempScene);
					if (serializer.Deserialize(newPath.string()))
					{
						tempScene->SetName(newPath.stem().string());
						serializer.Serialize(newPath.string());
					}
				}
			});
	}

	void EditorLayer::OnDetach()
	{
		WF_PROFILE_FUNCTION();
		SaveEditorConfig();
	}

	// -------------------------------------------------------------------------
	// Update
	// -------------------------------------------------------------------------

	void EditorLayer::OnUpdate(Timestep ts)
	{
		WF_PROFILE_FUNCTION();

		m_fps = ts;

		// Resize framebuffer if viewport changed
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
			(spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
		{
			m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_CameraController.OnResize(m_ViewportSize.x, m_ViewportSize.y);
			m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		}

		// Camera update (editor mode only)
		if (m_SceneState == SceneState::Edit)
		{
			if (m_ViewportFocused)
				m_CameraController.OnUpdate(ts);
			m_EditorCamera.OnUpdate(ts);
		}

		m_AnimationEditorPanel.SetSelectedEntity(m_SceneHierarchyPanel.GetSelectedEntity());

		// Clear
		Renderer2D::ResetStats();

		glm::vec4 clearColor = { 0.18f, 0.18f, 0.19f, 1.0f };
		if (Entity primaryCam = m_ActiveScene->GetPrimaryCameraEntity())
			clearColor = primaryCam.GetComponent<CameraComponent>().BackgroundColor;

		RenderCommand::SetClearColor(clearColor);
		m_Framebuffer->Bind();
		RenderCommand::Clear();
		m_Framebuffer->ClearAttachment(1, -1);

		// Scene update
		switch (m_SceneState)
		{
		case SceneState::Edit:
			m_ActiveScene->OnUpdateEditor(ts, m_EditorCamera);
			break;

		case SceneState::Play:
		{
			int pending = m_ActiveScene->OnUpdateRuntime(ts);
			if (pending != -1)
			{
				if (pending >= 0 && pending < (int)m_SceneList.size())
				{
					m_ActiveScene->OnRuntimeStop();

					Ref<Scene> nextScene = CreateRef<Scene>();
					SceneSerializer serializer(nextScene);
					if (serializer.Deserialize(m_SceneList[pending].string()))
					{
						LuaScriptEngine::SetCurrentSceneIndex(pending);
						nextScene->SetName(m_SceneList[pending].stem().string());

						m_ActiveScene = Scene::Copy(nextScene);
						m_ActiveScene->SetGravity(m_ProjectGravity);
						m_ActiveScene->OnViewportResize(
							(uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
						m_ActiveScene->OnRuntimeStart();
						m_SceneHierarchyPanel.SetContext(m_ActiveScene);
					}
					else
					{
						OnSceneStop();
					}
				}
				else
				{
					WF_CORE_WARN("ChangeScene: index {0} out of range (count: {1})",
						pending, m_SceneList.size());
					OnSceneStop();
				}
			}
			break;
		}
		}

		// Entity picking via framebuffer
		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;
		glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
		my = viewportSize.y - my;
		int mouseX = (int)mx;
		int mouseY = (int)my;

		if (mouseX >= 0 && mouseY >= 0 &&
			mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
		{
			int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY);
			if (pixelData != -1 && m_ActiveScene->IsEntityValid((entt::entity)pixelData))
				m_HoveredEntity = Entity((entt::entity)pixelData, m_ActiveScene.get());
			else
				m_HoveredEntity = Entity();
		}
		else
		{
			m_HoveredEntity = Entity();
		}

		OnOverlayRender();
		m_Framebuffer->Unbind();
	}

	// -------------------------------------------------------------------------
	// ImGui
	// -------------------------------------------------------------------------

	void EditorLayer::OnImGuiRender()
	{
		// -- DockSpace setup -------------------------------------------------
		static bool        dockSpaceOpen = true;
		static bool        opt_fullscreen = true;
		static bool        opt_padding = false;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags window_flags =
			ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

		if (opt_fullscreen)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
				| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
				| ImGuiWindowFlags_NoBringToFrontOnFocus
				| ImGuiWindowFlags_NoNavFocus;
		}
		else
		{
			dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
		}

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		if (!opt_padding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Waffle DockSpace", &dockSpaceOpen, window_flags);
		if (!opt_padding)
			ImGui::PopStyleVar();
		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		{
			ImGuiIO& io = ImGui::GetIO();
			ImGuiStyle& style = ImGui::GetStyle();
			float       minWinSizeX = style.WindowMinSize.x;
			style.WindowMinSize.x = 370.0f;

			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
			{
				ImGuiID dockspace_id = ImGui::GetID("WaffleDockSpace");
				ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
			}
			style.WindowMinSize.x = minWinSizeX;
		}

		// -- Menu bar --------------------------------------------------------
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Project..."))   NewProject();
				if (ImGui::MenuItem("Open Project..."))  OpenProject();
				ImGui::Separator();
				if (ImGui::MenuItem("New Scene", "Ctrl+N"))       NewScene();
				if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))    OpenScene();
				if (ImGui::MenuItem("Save Scene", "Ctrl+S"))       SaveScene();
				if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) SaveSceneAs();
				ImGui::Separator();
				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Project"))
			{
				if (ImGui::MenuItem("Project Settings"))
					m_ShowProjectTab = true;
				if (ImGui::MenuItem("Set Current as Default Template"))
					SetCurrentProjectAsDefaultTemplate();
				ImGui::Separator();
				if (ImGui::MenuItem("Export"))
				{
					PrepareExport();

					ProjectExporter::ExportOptions options;
					std::string projName = m_ProjectName.empty() ? "MyGame" : m_ProjectName;
					std::string appNameStr = m_ExportAppNameBuffer[0] != '\0'
						? m_ExportAppNameBuffer : projName;

					options.ProjectName = projName;
					options.ProjectPath = m_ProjectName.empty()
						? std::filesystem::path()
						: std::filesystem::path("Projects") / m_ProjectName;
					options.AppName = appNameStr;
					options.CustomIconPath = m_ExportIconPathBuffer;
					options.Gravity = m_ProjectGravity;

					if (!m_SceneList.empty())
						options.SelectedScenePath = m_SceneList[0].string();

					for (const auto& p : m_SceneList)
						options.SceneList.push_back(p.string());

					std::string errorMsg;
					if (ProjectExporter::ExportProject(options, errorMsg))
						WF_CORE_INFO("Exported to Projects/{0}/Exports/{1}.exe", projName, appNameStr);
					else
						WF_CORE_ERROR("Export failed: {0}", errorMsg);
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Window"))
			{
				ImGui::MenuItem("Animation Editor", nullptr, &m_ShowAnimationEditor);
				ImGui::MenuItem("Spritesheet Editor", nullptr, &m_ShowSpritesheetEditor);
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		// -- New Project modal ------------------------------------------------
		if (m_ShowNewProjectModal)
		{
			ImGui::OpenPopup("New Project");
			m_ShowNewProjectModal = false;
		}

		if (ImGui::BeginPopupModal("New Project", NULL,
			ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Enter Project Name:");
			ImGui::InputText("##ProjectNameInput",
				m_ProjectNameBuffer, sizeof(m_ProjectNameBuffer));

			if (ImGui::Button("Create Project", ImVec2(140, 0)) ||
				ImGui::IsKeyPressed(ImGuiKey_Enter))
			{
				std::string projName = m_ProjectNameBuffer;
				if (projName.empty()) projName = "NewProject";
				CreateProject(projName);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		// -- Project Settings window ------------------------------------------
		UI_ProjectSettings();

		if (m_ShowAnimationEditor)
			m_AnimationEditorPanel.OnImGuiRender();

		if (m_ShowSpritesheetEditor)
			m_SpritesheetEditorPanel.OnImGuiRender();

		// -- Export modal -----------------------------------------------------
		UI_ExportModal();

		// -- Stats -------------------------------------------------------------
		ImGui::Begin("Stats");
		{
			float frameTimeMs = m_fps * 1000.0f;
			float fps = m_fps > 0.0f ? 1.0f / m_fps : 0.0f;
			ImGui::Text("Performance: %.1f FPS (%.2f ms)", fps, frameTimeMs);
			ImGui::Separator();

			std::string hovName = "None";
			if (m_HoveredEntity && m_HoveredEntity.HasComponent<TagComponent>())
				hovName = m_HoveredEntity.GetComponent<TagComponent>().Tag;
			ImGui::Text("Hovered Entity: %s", hovName.c_str());
			ImGui::Separator();

			auto stats = Renderer2D::GetStats();
			ImGui::Text("Renderer2D Stats:");
			ImGui::Text("  Draw Calls: %d", stats.DrawCalls);
			ImGui::Text("  Quads: %d", stats.QuadCount);
			ImGui::Text("  Vertices: %d", stats.GetTotalVertexCount());
			ImGui::Text("  Indices: %d", stats.GetTotalIndexCount());
		}
		ImGui::End();

		m_SceneHierarchyPanel.OnImGuiRender();
		m_ContentBrowserPanel.OnImGuiRender();
		m_ConsolePanel.OnImGuiRender();

		ImGui::Begin("Settings");
		ImGui::Checkbox("Show Physics Colliders", &m_ShowPhysicsColliders);
		ImGui::End();

		// -- Viewport ----------------------------------------------------------
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");
		{
			auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
			auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
			auto viewportOffset = ImGui::GetWindowPos();
			m_ViewportBounds[0] = {
				viewportMinRegion.x + viewportOffset.x,
				viewportMinRegion.y + viewportOffset.y };
			m_ViewportBounds[1] = {
				viewportMaxRegion.x + viewportOffset.x,
				viewportMaxRegion.y + viewportOffset.y };

			m_ViewportFocused = ImGui::IsWindowFocused();
			m_ViewportHovered = ImGui::IsWindowHovered();
			Application::Get().GetImGuiLayer()->BlockEvents(
				!m_ViewportFocused && !m_ViewportHovered);

			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
			m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

			uint64_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
			ImGui::Image(
				reinterpret_cast<void*>(static_cast<uintptr_t>(textureID)),
				ImVec2{ m_ViewportSize.x, m_ViewportSize.y },
				ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

			// Drag-drop onto viewport
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					const wchar_t* pathStr = (const wchar_t*)payload->Data;
					std::filesystem::path path =
						std::filesystem::path(g_AssetPath) / pathStr;
					std::string ext = path.extension().string();
					for (auto& c : ext) c = (char)tolower(c);

					if (ext == ".waffle")
					{
						OpenScene(path);
					}
					else if (ext == ".prefab")
					{
						Entity instantiated = SceneSerializer::DeserializePrefabToEntity(m_ActiveScene.get(), path.string());
						if (instantiated)
							m_SceneHierarchyPanel.SetSelectedEntity(instantiated);
					}
					else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
					{
						Entity target = m_HoveredEntity
							? m_HoveredEntity
							: m_SceneHierarchyPanel.GetSelectedEntity();
						if (target)
						{
							if (!target.HasComponent<SpriteRendererComponent>())
								target.AddComponent<SpriteRendererComponent>();
							auto& src = target.GetComponent<SpriteRendererComponent>();
							src.Texture = Texture2D::Create(path.string(), src.FilterMode);
						}
					}
					else if (ext == ".lua")
					{
						Entity target = m_HoveredEntity
							? m_HoveredEntity
							: m_SceneHierarchyPanel.GetSelectedEntity();
						if (target)
						{
							if (!target.HasComponent<ScriptComponent>())
								target.AddComponent<ScriptComponent>();
							auto& sc = target.GetComponent<ScriptComponent>();
							std::string relStr =
								std::filesystem::relative(path, g_AssetPath).string();
							sc.ScriptPaths.push_back(relStr);
							LuaScriptEngine::ScrapeFieldsFromScript(path, relStr, sc);
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			// Gizmo toolbar (top-left of viewport)
			UI_GizmoToolbar();

			// Transform gizmos
			Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
			if (selectedEntity && m_GizmoType != -1)
			{
				const glm::mat4& cameraProjection = m_EditorCamera.GetProjection();
				glm::mat4        cameraView = m_EditorCamera.GetViewMatrix();

				ImGuizmo::SetOrthographic(cameraProjection[3][3] == 1.0f);
				ImGuizmo::SetDrawlist();
				ImGuizmo::SetRect(
					m_ViewportBounds[0].x, m_ViewportBounds[0].y,
					m_ViewportBounds[1].x - m_ViewportBounds[0].x,
					m_ViewportBounds[1].y - m_ViewportBounds[0].y);

				auto& tc = selectedEntity.GetComponent<TransformComponent>();
				glm::mat4 transform = tc.GetTransform();

				bool  snap = Input::IsKeyPressed(Key::LeftControl);
				float snapValue = (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
					? 45.0f : 0.5f;
				float snapValues[3] = { snapValue, snapValue, snapValue };

				if (m_SceneState == SceneState::Edit)
				{
					ImGuizmo::Manipulate(
						glm::value_ptr(cameraView),
						glm::value_ptr(cameraProjection),
						(ImGuizmo::OPERATION)m_GizmoType,
						ImGuizmo::LOCAL,
						glm::value_ptr(transform),
						nullptr,
						snap ? snapValues : nullptr);

					if (ImGuizmo::IsUsing())
					{
						glm::vec3 translation, rotationDeg, scale;
						ImGuizmo::DecomposeMatrixToComponents(
							glm::value_ptr(transform),
							glm::value_ptr(translation),
							glm::value_ptr(rotationDeg),
							glm::value_ptr(scale));

						scale.x = std::max(scale.x, 0.0001f);
						scale.y = std::max(scale.y, 0.0001f);
						scale.z = std::max(scale.z, 0.0001f);

						glm::vec3 rotationRad = glm::radians(rotationDeg);
						glm::vec3 deltaRotation = rotationRad - tc.Rotation;
						for (int i = 0; i < 3; i++)
						{
							while (deltaRotation[i] > glm::pi<float>())
								deltaRotation[i] -= glm::two_pi<float>();
							while (deltaRotation[i] < -glm::pi<float>())
								deltaRotation[i] += glm::two_pi<float>();
						}

						tc.Translation = translation;
						tc.Rotation += deltaRotation;
						tc.Scale = scale;
					}
				}
			}

			UI_Toolbar();
		}
		ImGui::End();
		ImGui::PopStyleVar();

		ImGui::End(); // DockSpace
	}

	// -------------------------------------------------------------------------
	// UI sub-panels
	// -------------------------------------------------------------------------

	void EditorLayer::UI_GizmoToolbar()
	{
		const float iconSize = 22.0f;
		const float padding = 10.0f;

		ImGui::SetCursorPosY(38.0f);
		ImGui::SetCursorPosX(16.0f);
		ImGui::BeginGroup();

		auto GizmoBtn = [&](const char* id, ImTextureID icon, int gizmoType)
			{
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + padding);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding * 0.5f);
				if (ImGui::ImageButton(id, icon,
					ImVec2(iconSize, iconSize), ImVec2(0, 0), ImVec2(1, 1)))
					m_GizmoType = gizmoType;
			};

		GizmoBtn("##NoGizmo",
			(m_GizmoType == -1)
			? (ImTextureID)(uintptr_t)m_IconNoGizmoActive->GetRendererID()
			: (ImTextureID)(uintptr_t)m_IconNoGizmo->GetRendererID(),
			-1);

		GizmoBtn("##TranslateGizmo",
			(m_GizmoType == ImGuizmo::OPERATION::TRANSLATE)
			? (ImTextureID)(uintptr_t)m_IconTransformGizmoActive->GetRendererID()
			: (ImTextureID)(uintptr_t)m_IconTransformGizmo->GetRendererID(),
			ImGuizmo::OPERATION::TRANSLATE);

		GizmoBtn("##RotateGizmo",
			(m_GizmoType == ImGuizmo::OPERATION::ROTATE)
			? (ImTextureID)(uintptr_t)m_IconRotationGizmoActive->GetRendererID()
			: (ImTextureID)(uintptr_t)m_IconRotationGizmo->GetRendererID(),
			ImGuizmo::OPERATION::ROTATE);

		GizmoBtn("##ScaleGizmo",
			(m_GizmoType == ImGuizmo::OPERATION::SCALE)
			? (ImTextureID)(uintptr_t)m_IconScaleGizmoActive->GetRendererID()
			: (ImTextureID)(uintptr_t)m_IconScaleGizmo->GetRendererID(),
			ImGuizmo::OPERATION::SCALE);

		ImGui::EndGroup();
	}

	void EditorLayer::UI_Toolbar()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(6, 6));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		auto& colors = ImGui::GetStyle().Colors;
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
			ImVec4(colors[ImGuiCol_ButtonHovered].x,
				colors[ImGuiCol_ButtonHovered].y,
				colors[ImGuiCol_ButtonHovered].z, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,
			ImVec4(colors[ImGuiCol_ButtonActive].x,
				colors[ImGuiCol_ButtonActive].y,
				colors[ImGuiCol_ButtonActive].z, 0.5f));

		const float    iconSize = 24.0f;
		const float    padding = 10.0f;
		const float    pillWidth = 3.0f * (iconSize + padding) + padding;
		const float    availableWidth = ImGui::GetWindowContentRegionMax().x;
		const float    startX = (availableWidth - pillWidth) * 0.5f;
		ImVec4         tintColor = ImVec4(1, 1, 1, (bool)m_ActiveScene ? 1.0f : 0.5f);

		ImGui::SetCursorPosY(38.0f);
		ImGui::SetCursorPosX(startX);

		// Play / Stop
		Ref<Texture2D> playIcon =
			(m_SceneState == SceneState::Edit) ? m_IconPlay : m_IconStop;
		if (ImGui::ImageButton("##Play",
			(ImTextureID)(uintptr_t)playIcon->GetRendererID(),
			ImVec2(iconSize, iconSize), ImVec2(0, 0), ImVec2(1, 1),
			ImVec4(0, 0, 0, 0), tintColor) && m_ActiveScene)
		{
			if (m_SceneState == SceneState::Edit) OnScenePlay();
			else                                  OnSceneStop();
		}

		// Pause
		ImGui::SameLine(0.0f, padding);
		bool isPaused = m_ActiveScene ? m_ActiveScene->IsPaused() : false;
		{
			Ref<Texture2D> icon = (m_SceneState == SceneState::Play)
				? (isPaused ? m_IconPlay : m_IconPause)
				: m_IconPauseInactive;
			if (ImGui::ImageButton("##Pause",
				(ImTextureID)(uintptr_t)icon->GetRendererID(),
				ImVec2(iconSize, iconSize), ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(0, 0, 0, 0), tintColor) && m_ActiveScene)
			{
				if (m_SceneState == SceneState::Play)
					m_ActiveScene->SetPaused(!isPaused);
			}
		}

		// Step
		ImGui::SameLine(0.0f, padding);
		{
			Ref<Texture2D> icon = (m_SceneState == SceneState::Play)
				? m_IconStep : m_IconStepInactive;
			if (ImGui::ImageButton("##Step",
				(ImTextureID)(uintptr_t)icon->GetRendererID(),
				ImVec2(iconSize, iconSize), ImVec2(0, 0), ImVec2(1, 1),
				ImVec4(0, 0, 0, 0), tintColor) && m_ActiveScene)
			{
				if (m_SceneState == SceneState::Play && isPaused)
					m_ActiveScene->Step();
			}
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
	}

	void EditorLayer::UI_ProjectSettings()
	{
		if (!m_ShowProjectTab)
			return;

		ImGui::SetNextWindowSize(ImVec2(540, 600), ImGuiCond_FirstUseEver);
		ImGui::Begin("Project Settings", &m_ShowProjectTab);

		// -- Identity ------------------------------------------------------
		ImGui::SeparatorText("Identity");

		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##AppName", m_ExportAppNameBuffer, sizeof(m_ExportAppNameBuffer));
		ImGui::SameLine(0, 0);
		ImGui::TextDisabled(" Application Name");
		if (ImGui::IsItemDeactivatedAfterEdit()) SaveProjectSettings();

		// Icon preview
		ImGui::Spacing();
		ImGui::Text("Application Icon");

		// Lazy-load preview texture when path changes
		{
			std::string currentIconPath = m_ExportIconPathBuffer;
			if (currentIconPath != m_ProjectIconPreviewPath)
			{
				m_ProjectIconPreviewPath = currentIconPath;
				m_ProjectIconPreviewTexture =
					(!currentIconPath.empty() && std::filesystem::exists(currentIconPath))
					? Texture2D::Create(currentIconPath)
					: nullptr;
			}
		}

		// Preview button (drag-drop target)
		if (m_ProjectIconPreviewTexture)
		{
			ImGui::ImageButton("##ProjectIcon",
				(ImTextureID)(uintptr_t)m_ProjectIconPreviewTexture->GetRendererID(),
				ImVec2(48, 48), ImVec2(0, 1), ImVec2(1, 0));
		}
		else
		{
			ImGui::Button("No Icon##ProjectIconBtn", ImVec2(48, 48));
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				std::filesystem::path dropped =
					std::filesystem::path(g_AssetPath) /
					(const wchar_t*)payload->Data;
				std::string ext = dropped.extension().string();
				for (auto& c : ext) c = (char)tolower(c);
				if (ext == ".png" || ext == ".jpg" || ext == ".ico")
				{
					strcpy_s(m_ExportIconPathBuffer,
						sizeof(m_ExportIconPathBuffer),
						dropped.string().c_str());
					SaveProjectSettings();
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
		ImGui::InputText("##IconPath", m_ExportIconPathBuffer, sizeof(m_ExportIconPathBuffer));
		if (ImGui::IsItemDeactivatedAfterEdit()) SaveProjectSettings();
		ImGui::SameLine();
		if (ImGui::Button("Browse...##IconBrowse"))
		{
			std::string sel = FileDialogs::OpenFile(
				"Image Files (*.png;*.jpg;*.ico)\0*.png;*.jpg;*.ico\0");
			if (!sel.empty())
			{
				strcpy_s(m_ExportIconPathBuffer,
					sizeof(m_ExportIconPathBuffer), sel.c_str());
				SaveProjectSettings();
			}
		}
		if (ImGui::Button("Clear##IconClear"))
		{
			m_ExportIconPathBuffer[0] = '\0';
			m_ProjectIconPreviewTexture = nullptr;
			m_ProjectIconPreviewPath.clear();
			SaveProjectSettings();
		}
		ImGui::EndGroup();

		// -- Physics -------------------------------------------------------
		ImGui::SeparatorText("Physics");
		if (ImGui::DragFloat("Gravity Y", &m_ProjectGravity, 0.1f, -100.0f, 0.0f))
		{
			if (m_ActiveScene && m_ActiveScene->GetPhysicsWorld())
				m_ActiveScene->GetPhysicsWorld()->SetGravity({ 0.0f, m_ProjectGravity });
			SaveProjectSettings();
		}

		// -- Scene Order ---------------------------------------------------
		ImGui::SeparatorText("Scene Order");
		ImGui::TextDisabled("Index 0 is the start scene. Drag rows or use arrows to reorder.");

		if (ImGui::Button("Refresh##SceneList"))
			RebuildSceneList();

		ImGui::Spacing();

		if (ImGui::BeginTable("SceneOrderTable", 5,
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY,
			ImVec2(0.0f, 200.0f)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.0f);
			ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("##up", ImGuiTableColumnFlags_WidthFixed, 24.0f);
			ImGui::TableSetupColumn("##dn", ImGuiTableColumnFlags_WidthFixed, 24.0f);
			ImGui::TableHeadersRow();

			for (int i = 0; i < (int)m_SceneList.size(); i++)
			{
				ImGui::TableNextRow();

				// Highlight currently open scene
				std::error_code ec;
				bool isCurrent = !m_EditorScenePath.empty() &&
					std::filesystem::equivalent(m_SceneList[i], m_EditorScenePath, ec);
				if (isCurrent)
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
						ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.5f, 0.2f, 0.4f)));

				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%d", i);

				ImGui::TableSetColumnIndex(1);
				std::string stem = m_SceneList[i].stem().string();
				ImGui::PushID(i);
				ImGui::TextUnformatted(stem.c_str());

				if (ImGui::IsItemHovered() &&
					ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					OpenScene(m_SceneList[i]);

				// Drag source
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					ImGui::SetDragDropPayload("SCENE_ORDER_ITEM", &i, sizeof(int));
					ImGui::Text("Moving: %s", stem.c_str());
					ImGui::EndDragDropSource();
				}
				// Drag target
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload =
						ImGui::AcceptDragDropPayload("SCENE_ORDER_ITEM"))
					{
						int from = *(const int*)payload->Data;
						if (from != i)
						{
							auto tmp = m_SceneList[from];
							m_SceneList.erase(m_SceneList.begin() + from);
							m_SceneList.insert(m_SceneList.begin() + i, tmp);
							SaveProjectSettings();
						}
					}
					ImGui::EndDragDropTarget();
				}
				ImGui::PopID();

				ImGui::TableSetColumnIndex(2);
				{
					std::string fullStr = m_SceneList[i].string();
					ImGui::TextDisabled("%s", fullStr.c_str());
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s", fullStr.c_str());
				}

				ImGui::TableSetColumnIndex(3);
				ImGui::PushID(i * 100 + 0);
				if (ImGui::ArrowButton("##up", ImGuiDir_Up) && i > 0)
				{
					std::swap(m_SceneList[i], m_SceneList[i - 1]);
					SaveProjectSettings();
				}
				ImGui::PopID();

				ImGui::TableSetColumnIndex(4);
				ImGui::PushID(i * 100 + 1);
				if (ImGui::ArrowButton("##dn", ImGuiDir_Down) &&
					i < (int)m_SceneList.size() - 1)
				{
					std::swap(m_SceneList[i], m_SceneList[i + 1]);
					SaveProjectSettings();
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		if (m_SceneList.empty())
			ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
				"No .waffle scenes found. Save a scene first.");

		// -- Export shortcut ------------------------------------------------
		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("Export Project...", ImVec2(-1.0f, 0.0f)))
		{
			PrepareExport();
			SaveProjectSettings();

			std::string projName = m_ProjectName.empty() ? "MyGame" : m_ProjectName;
			std::string appNameStr = m_ExportAppNameBuffer[0] != '\0'
				? m_ExportAppNameBuffer : projName;

			ProjectExporter::ExportOptions options;
			options.ProjectName = projName;
			options.ProjectPath = m_ProjectName.empty()
				? std::filesystem::path()
				: std::filesystem::path("Projects") / m_ProjectName;
			options.AppName = appNameStr;
			options.CustomIconPath = m_ExportIconPathBuffer;
			options.Gravity = m_ProjectGravity;

			if (!m_SceneList.empty())
				options.SelectedScenePath = m_SceneList[0].string();

			for (const auto& p : m_SceneList)
				options.SceneList.push_back(p.string());

			std::string errorMsg;
			if (ProjectExporter::ExportProject(options, errorMsg))
				WF_CORE_INFO("Exported to Projects/{0}/Exports/{1}.exe", projName, appNameStr);
			else
				WF_CORE_ERROR("Export failed: {0}", errorMsg);
		}

		ImGui::End();
	}

	void EditorLayer::UI_ExportModal()
	{
		if (m_ShowExportModal)
		{
			ImGui::OpenPopup("Export Project");
			m_ShowExportModal = false;
		}

		if (!ImGui::BeginPopupModal("Export Project", NULL,
			ImGuiWindowFlags_AlwaysAutoResize))
			return;

		std::string projName = m_ProjectName.empty() ? "MyGame" : m_ProjectName;
		std::string appNameStr = m_ExportAppNameBuffer[0] != '\0'
			? m_ExportAppNameBuffer : projName;

		ImGui::Text("Project: %s", projName.c_str());
		ImGui::Text("Target OS: Windows (x64)");
		ImGui::Separator();

		ImGui::InputText("Application Name",
			m_ExportAppNameBuffer, sizeof(m_ExportAppNameBuffer));

		// Start scene — choosing a scene here rotates it to index 0 in m_SceneList,
		// so "Scenes[0]" and "StartScene" in project.wfp always agree.
		if (!m_SceneList.empty())
		{
			std::vector<std::string> names;
			std::vector<const char*> items;
			for (const auto& p : m_SceneList)
				names.push_back(p.stem().string());
			for (const auto& n : names)
				items.push_back(n.c_str());

			// Show the current index-0 scene as the selected start scene
			int startIndex = 0;
			if (ImGui::Combo("Start Scene", &startIndex,
				items.data(), (int)items.size()))
			{
				if (startIndex != 0)
				{
					// Rotate chosen scene to the front
					std::rotate(m_SceneList.begin(),
						m_SceneList.begin() + startIndex,
						m_SceneList.begin() + startIndex + 1);
					SaveProjectSettings();
				}
			}
			ImGui::TextDisabled("Index 0 in the Scene Order is always the start scene.");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
				"No .waffle scenes found in project!");
		}

		// Icon
		ImGui::InputText("App Icon",
			m_ExportIconPathBuffer, sizeof(m_ExportIconPathBuffer));
		ImGui::SameLine();
		if (ImGui::Button("Browse...##ExportIcon"))
		{
			std::string sel = FileDialogs::OpenFile(
				"Image Files (*.png;*.jpg;*.ico)\0*.png;*.jpg;*.ico\0");
			if (!sel.empty())
				strcpy_s(m_ExportIconPathBuffer,
					sizeof(m_ExportIconPathBuffer), sel.c_str());
		}

		ImGui::Separator();
		ImGui::Text("Output: Projects/%s/Exports/%s.exe",
			projName.c_str(), appNameStr.c_str());
		ImGui::Separator();

		if (!m_ExportStatusMessage.empty())
		{
			if (m_ExportSuccess)
				ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
					"%s", m_ExportStatusMessage.c_str());
			else
				ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f),
					"%s", m_ExportStatusMessage.c_str());
			ImGui::Separator();
		}

		if (ImGui::Button("Export Executable", ImVec2(160, 0)))
		{
			SaveProjectSettings();

			ProjectExporter::ExportOptions options;
			options.ProjectName = projName;
			options.ProjectPath = m_ProjectName.empty()
				? std::filesystem::path()
				: std::filesystem::path("Projects") / m_ProjectName;
			options.AppName = appNameStr;
			options.CustomIconPath = m_ExportIconPathBuffer;
			options.Gravity = m_ProjectGravity;

			// Index 0 is always the start scene — no separate SelectedScenePath ambiguity
			if (!m_SceneList.empty())
				options.SelectedScenePath = m_SceneList[0].string();

			for (const auto& p : m_SceneList)
				options.SceneList.push_back(p.string());

			std::string errorMsg;
			if (ProjectExporter::ExportProject(options, errorMsg))
			{
				m_ExportSuccess = true;
				m_ExportStatusMessage = "Exported to Projects/" + projName
					+ "/Exports/" + appNameStr + ".exe";
			}
			else
			{
				m_ExportSuccess = false;
				m_ExportStatusMessage = "Export failed: " + errorMsg;
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Close", ImVec2(100, 0)))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	// -------------------------------------------------------------------------
	// Events
	// -------------------------------------------------------------------------

	void EditorLayer::OnEvent(Waffle::Event& e)
	{
		m_CameraController.OnEvent(e);
		if (m_SceneState == SceneState::Edit)
			m_EditorCamera.OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(
			WF_BIND_EVENT_FN(EditorLayer::OnkeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(
			WF_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
		dispatcher.Dispatch<WindowDropEvent>(
			WF_BIND_EVENT_FN(EditorLayer::OnWindowDrop));
	}

	bool EditorLayer::OnWindowDrop(WindowDropEvent& e)
	{
		const auto& targetDir = m_ContentBrowserPanel.GetCurrentDirectory();
		if (!std::filesystem::exists(targetDir))
			return false;

		for (const auto& path : e.GetPaths())
		{
			if (std::filesystem::exists(path))
			{
				std::error_code ec;
				std::filesystem::copy(path, targetDir / path.filename(),
					std::filesystem::copy_options::overwrite_existing |
					std::filesystem::copy_options::recursive, ec);
			}
		}
		return true;
	}

	bool EditorLayer::OnkeyPressed(KeyPressedEvent& e)
	{
		if (m_SceneState == SceneState::Play) return false;
		if (e.GetRepeatCount() > 0)           return false;

		bool control = Input::IsKeyPressed(Key::LeftControl) ||
			Input::IsKeyPressed(Key::RightControl);
		bool shift = Input::IsKeyPressed(Key::LeftShift) ||
			Input::IsKeyPressed(Key::RightShift);

		switch (e.GetKeyCode())
		{
		case Key::S: if (control) { shift ? SaveSceneAs() : SaveScene(); } break;
		case Key::O: if (control) OpenScene();         break;
		case Key::N: if (control) NewScene();          break;
		case Key::D: if (control) OnDuplicateEntity(); break;

			// Gizmos
		case Key::Q: m_GizmoType = -1;                            break;
		case Key::W: m_GizmoType = ImGuizmo::OPERATION::TRANSLATE; break;
		case Key::E: m_GizmoType = ImGuizmo::OPERATION::ROTATE;    break;
		case Key::R: m_GizmoType = ImGuizmo::OPERATION::SCALE;     break;

		case Key::F:
		{
			Entity sel = m_SceneHierarchyPanel.GetSelectedEntity();
			if (sel && sel.HasComponent<TransformComponent>())
				m_EditorCamera.SetFocalPoint(
					sel.GetComponent<TransformComponent>().Translation);
			break;
		}
		}
		return false;
	}

	bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e)
	{
		if (e.GetMouseButton() == Mouse::ButtonLeft &&
			m_ViewportHovered &&
			!ImGuizmo::IsOver() &&
			!Input::IsKeyPressed(Key::LeftControl))
		{
			if (m_HoveredEntity)
				m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity);
		}
		return false;
	}

	// -------------------------------------------------------------------------
	// Overlay
	// -------------------------------------------------------------------------

	void EditorLayer::OnOverlayRender()
	{
		if (m_SceneState == SceneState::Play)
		{
			Entity camera = m_ActiveScene->GetPrimaryCameraEntity();
			if (!camera) return;
			Renderer2D::BeginScene(
				camera.GetComponent<CameraComponent>().Camera,
				camera.GetComponent<TransformComponent>().GetTransform());
		}
		else
		{
			Renderer2D::BeginScene(m_EditorCamera);
		}

		if (m_ShowPhysicsColliders)
		{
			// Box colliders
			auto boxView = m_ActiveScene->GetAllEntitiesWith<
				TransformComponent, BoxCollider2DComponent>();
			for (auto entity : boxView)
			{
				auto [tc, bc2d] = boxView.get<
					TransformComponent, BoxCollider2DComponent>(entity);

				glm::mat4 transform =
					glm::translate(glm::mat4(1.0f),
						tc.Translation + glm::vec3(bc2d.Offset, 0.001f))
					* glm::rotate(glm::mat4(1.0f), tc.Rotation.z,
						glm::vec3(0.0f, 0.0f, 1.0f))
					* glm::scale(glm::mat4(1.0f),
						tc.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f));

				Renderer2D::DrawRect(transform, glm::vec4(0, 1, 0, 1));
			}

			// Circle colliders
			auto circleView = m_ActiveScene->GetAllEntitiesWith<
				TransformComponent, CircleCollider2DComponent>();
			for (auto entity : circleView)
			{
				auto [tc, cc2d] = circleView.get<
					TransformComponent, CircleCollider2DComponent>(entity);

				glm::mat4 transform =
					glm::translate(glm::mat4(1.0f),
						tc.Translation + glm::vec3(cc2d.Offset, 0.001f))
					* glm::scale(glm::mat4(1.0f),
						tc.Scale * glm::vec3(cc2d.Radius * 2.0f));

				Renderer2D::DrawCircle(transform, glm::vec4(0, 1, 0, 1), 0.01f);
			}
		}

		// Selected entity outline
		if (Entity sel = m_SceneHierarchyPanel.GetSelectedEntity())
		{
			Renderer2D::DrawRect(
				sel.GetComponent<TransformComponent>().GetTransform(),
				glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
		}

		Renderer2D::EndScene();
	}

	// -------------------------------------------------------------------------
	// Project management
	// -------------------------------------------------------------------------

	void EditorLayer::NewProject()
	{
		strcpy_s(m_ProjectNameBuffer, sizeof(m_ProjectNameBuffer), "NewProject");
		m_ShowNewProjectModal = true;
	}

	void EditorLayer::OpenProject()
	{
		std::string folderpath = FileDialogs::OpenFolder();
		if (!folderpath.empty())
			OpenProjectAtPath(folderpath);
	}

	void EditorLayer::CreateProject(const std::string& projectName)
	{
		std::filesystem::path projectPath = std::filesystem::path("Projects") / projectName;
		std::filesystem::path defaultProjPath = FindDefaultProjectPath();
		std::error_code ec;

		if (!defaultProjPath.empty() && std::filesystem::exists(defaultProjPath))
		{
			std::filesystem::create_directories(projectPath, ec);
			std::filesystem::copy(defaultProjPath, projectPath,
				std::filesystem::copy_options::overwrite_existing |
				std::filesystem::copy_options::recursive, ec);

			// Clear old exports
			if (std::filesystem::exists(projectPath / "Exports"))
				std::filesystem::remove_all(projectPath / "Exports", ec);
			std::filesystem::create_directories(projectPath / "Exports", ec);
		}
		else
		{
			std::filesystem::path assetsPath = projectPath / "Assets";
			std::filesystem::create_directories(assetsPath / "Scenes", ec);
			std::filesystem::create_directories(assetsPath / "Textures", ec);
			std::filesystem::create_directories(assetsPath / "Scripts", ec);
			std::filesystem::create_directories(assetsPath / "Prefabs", ec);
			std::filesystem::create_directories(projectPath / "Exports", ec);
		}

		OpenProjectAtPath(projectPath);
		SaveEditorConfig();
	}

	void EditorLayer::OpenProjectAtPath(const std::filesystem::path& projectPath)
	{
		if (!std::filesystem::exists(projectPath))
			return;

		std::filesystem::path assetsPath = projectPath / "Assets";
		if (!std::filesystem::exists(assetsPath))
			assetsPath = projectPath / "assets";
		if (!std::filesystem::exists(assetsPath))
			assetsPath = projectPath;

		m_ProjectName = projectPath.filename().string();
		m_ContentBrowserPanel.SetAssetDirectory(assetsPath);
		LuaScriptEngine::SetAssetPath(assetsPath);

		// Load saved project settings (gravity, name, icon, scene order)
		std::filesystem::path wfpPath = assetsPath / "project.wfp";
		if (std::filesystem::exists(wfpPath))
		{
			try
			{
				YAML::Node data = YAML::LoadFile(wfpPath.string());
				auto       project = data["Project"];
				if (project)
				{
					if (project["Gravity"])
						m_ProjectGravity = project["Gravity"].as<float>();

					if (project["Name"])
						strcpy_s(m_ExportAppNameBuffer, sizeof(m_ExportAppNameBuffer),
							project["Name"].as<std::string>().c_str());

					if (project["IconPath"])
						strcpy_s(m_ExportIconPathBuffer, sizeof(m_ExportIconPathBuffer),
							project["IconPath"].as<std::string>().c_str());

					// Restore saved scene order first, then merge with disk
					if (project["Scenes"])
					{
						m_SceneList.clear();
						for (auto node : project["Scenes"])
						{
							std::filesystem::path p = node.as<std::string>();
							if (std::filesystem::exists(p))
								m_SceneList.push_back(p);
						}
					}
				}
			}
			catch (...) {}
		}

		// Merge: pick up any new scenes not yet in the saved list
		RebuildSceneList();
		UpdateWindowTitle();
		SaveEditorConfig();

		// Open the first available scene
		std::filesystem::path defaultScene;

		// Prefer Scenes/ subdirectory
		if (std::filesystem::exists(assetsPath / "Scenes"))
		{
			for (const auto& entry :
				std::filesystem::directory_iterator(assetsPath / "Scenes"))
			{
				if (entry.is_regular_file() &&
					entry.path().extension() == ".waffle")
				{
					defaultScene = entry.path();
					break;
				}
			}
		}

		// Fall back to any .waffle in assets
		if (defaultScene.empty() && std::filesystem::exists(assetsPath))
		{
			for (const auto& entry :
				std::filesystem::recursive_directory_iterator(assetsPath))
			{
				if (entry.is_regular_file() &&
					entry.path().extension() == ".waffle")
				{
					defaultScene = entry.path();
					break;
				}
			}
		}

		if (!defaultScene.empty())
			OpenScene(defaultScene);
		else
			NewScene();
	}

	std::filesystem::path EditorLayer::FindDefaultProjectPath()
	{
		std::vector<std::filesystem::path> candidates = {
			"Projects/DefaultProject",
			"Waffle-Editor/Projects/DefaultProject",
			"../Waffle-Editor/Projects/DefaultProject"
		};
		for (const auto& path : candidates)
		{
			if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
				return path;
		}
		return "";
	}

	void EditorLayer::PrepareExport()
	{
		m_ExportStatusMessage = "";
		m_ExportSuccess = false;

		if (m_ExportAppNameBuffer[0] == '\0')
		{
			std::string def = m_ProjectName.empty() ? "MyGame" : m_ProjectName;
			strcpy_s(m_ExportAppNameBuffer, sizeof(m_ExportAppNameBuffer), def.c_str());
		}

		RebuildSceneList();
	}

	// -------------------------------------------------------------------------
	// Scene management
	// -------------------------------------------------------------------------

	void EditorLayer::NewScene()
	{
		m_EditorScene = CreateRef<Scene>();
		m_EditorScene->SetName("Untitled");
		m_EditorScene->OnViewportResize(
			(uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_ActiveScene = m_EditorScene;
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_EditorScenePath.clear();
		UpdateWindowTitle();
	}

	void EditorLayer::OpenScene()
	{
		std::string filepath =
			FileDialogs::OpenFile("Waffle Scene (*.waffle)\0*.waffle\0");
		if (!filepath.empty())
			OpenScene(filepath);
	}

	void EditorLayer::OpenScene(const std::filesystem::path& path)
	{
		if (m_SceneState != SceneState::Edit)
			OnSceneStop();

		if (path.extension().string() != ".waffle")
		{
			WF_WARN("Could not load {0} - not a scene file", path.filename().string());
			return;
		}

		Ref<Scene>       newScene = CreateRef<Scene>();
		SceneSerializer  serializer(newScene);
		if (serializer.Deserialize(path.string()))
		{
			m_EditorScene = newScene;
			m_EditorScene->SetName(path.stem().string());
			m_EditorScene->OnViewportResize(
				(uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_SceneHierarchyPanel.SetContext(m_EditorScene);
			m_ActiveScene = m_EditorScene;
			m_EditorScenePath = path;
			UpdateWindowTitle();
			SaveEditorConfig();
		}
	}

	void EditorLayer::SaveScene()
	{
		if (!m_EditorScenePath.empty())
			SerializeScene(m_ActiveScene, m_EditorScenePath);
		else
			SaveSceneAs();
	}

	void EditorLayer::SaveSceneAs()
	{
		std::string filepath =
			FileDialogs::SaveFile("Waffle Scene (*.waffle)\0*.waffle\0");
		if (!filepath.empty())
		{
			std::filesystem::path path(filepath);
			m_ActiveScene->SetName(path.stem().string());
			SerializeScene(m_ActiveScene, filepath);
			m_EditorScenePath = filepath;
			UpdateWindowTitle();
			SaveEditorConfig();
		}
	}

	void EditorLayer::SerializeScene(Ref<Scene> scene,
		const std::filesystem::path& path)
	{
		SceneSerializer serializer(scene);
		serializer.Serialize(path.string());
	}

	void EditorLayer::RebuildSceneList()
	{
		std::filesystem::path assetsPath = m_ProjectName.empty()
			? std::filesystem::path("Assets")
			: std::filesystem::path("Projects") / m_ProjectName / "Assets";

		// Scan for all .waffle files
		std::vector<std::filesystem::path> found;
		if (std::filesystem::exists(assetsPath))
		{
			for (const auto& entry :
				std::filesystem::recursive_directory_iterator(assetsPath))
			{
				if (entry.is_regular_file() &&
					entry.path().extension() == ".waffle")
					found.push_back(entry.path());
			}
		}
		std::sort(found.begin(), found.end());

		// Keep existing order, append newly discovered scenes at the end
		std::vector<std::filesystem::path> merged;
		for (const auto& existing : m_SceneList)
		{
			std::error_code ec;
			bool stillExists = std::any_of(found.begin(), found.end(),
				[&](const auto& f) {
					return std::filesystem::equivalent(f, existing, ec);
				});
			if (stillExists)
				merged.push_back(existing);
		}
		for (const auto& f : found)
		{
			std::error_code ec;
			bool alreadyIn = std::any_of(merged.begin(), merged.end(),
				[&](const auto& m) {
					return std::filesystem::equivalent(f, m, ec);
				});
			if (!alreadyIn)
				merged.push_back(f);
		}

		m_SceneList = std::move(merged);
	}

	// -------------------------------------------------------------------------
	// Project settings persistence
	// -------------------------------------------------------------------------

	void EditorLayer::SaveProjectSettings()
	{
		if (m_ProjectName.empty())
			return;

		std::filesystem::path wfpPath =
			std::filesystem::path("Projects") / m_ProjectName / "Assets" / "project.wfp";

		std::string appName = m_ExportAppNameBuffer[0] != '\0'
			? std::string(m_ExportAppNameBuffer) : m_ProjectName;

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Name" << YAML::Value << appName;
		out << YAML::Key << "Gravity" << YAML::Value << m_ProjectGravity;
		out << YAML::Key << "IconPath" << YAML::Value
			<< std::string(m_ExportIconPathBuffer);

		out << YAML::Key << "Scenes" << YAML::Value << YAML::BeginSeq;
		for (const auto& p : m_SceneList)
			out << p.string();
		out << YAML::EndSeq;

		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream fout(wfpPath);
		fout << out.c_str();
	}

	void EditorLayer::SaveEditorConfig()
	{
		std::filesystem::path configPath = std::filesystem::path("Projects") / "editor_config.yaml";
		std::filesystem::create_directories("Projects");

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "EditorConfig" << YAML::Value << YAML::BeginMap;
		if (!m_ProjectName.empty())
		{
			std::filesystem::path projPath = std::filesystem::path("Projects") / m_ProjectName;
			out << YAML::Key << "LastOpenedProject" << YAML::Value << projPath.string();
		}
		if (!m_EditorScenePath.empty())
		{
			out << YAML::Key << "LastOpenedScene" << YAML::Value << m_EditorScenePath.string();
		}
		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream fout(configPath);
		fout << out.c_str();
	}

	void EditorLayer::LoadEditorConfig()
	{
		std::filesystem::path configPath = std::filesystem::path("Projects") / "editor_config.yaml";
		bool opened = false;
		if (std::filesystem::exists(configPath))
		{
			try
			{
				YAML::Node data = YAML::LoadFile(configPath.string());
				auto cfg = data["EditorConfig"];
				if (cfg)
				{
					std::string lastProjStr = cfg["LastOpenedProject"] ? cfg["LastOpenedProject"].as<std::string>() : "";
					std::string lastSceneStr = cfg["LastOpenedScene"] ? cfg["LastOpenedScene"].as<std::string>() : "";

					if (!lastProjStr.empty() && std::filesystem::exists(lastProjStr))
					{
						OpenProjectAtPath(lastProjStr);
						opened = true;

						if (!lastSceneStr.empty() && std::filesystem::exists(lastSceneStr))
						{
							OpenScene(lastSceneStr);
						}
					}
				}
			}
			catch (...) {}
		}

		if (!opened)
		{
			std::filesystem::path defaultProj = FindDefaultProjectPath();
			if (!defaultProj.empty())
				OpenProjectAtPath(defaultProj);
		}
	}

	void EditorLayer::SetCurrentProjectAsDefaultTemplate()
	{
		if (m_ProjectName.empty()) return;
		std::filesystem::path currentPath = std::filesystem::path("Projects") / m_ProjectName;
		std::filesystem::path defaultPath = std::filesystem::path("Projects") / "DefaultProject";
		if (!std::filesystem::exists(currentPath)) return;

		SaveScene();
		SaveProjectSettings();

		std::error_code ec;
		std::filesystem::create_directories(defaultPath, ec);
		std::filesystem::copy(currentPath, defaultPath,
			std::filesystem::copy_options::overwrite_existing |
			std::filesystem::copy_options::recursive, ec);

		if (std::filesystem::exists(defaultPath / "Exports"))
			std::filesystem::remove_all(defaultPath / "Exports", ec);

		WF_CORE_INFO("Set current project '{0}' as default template!", m_ProjectName);
	}

	void EditorLayer::UpdateWindowTitle()
	{
		std::string title = "Waffle Editor";
		if (!m_ProjectName.empty())
			title += " - " + m_ProjectName;
		if (!m_EditorScenePath.empty())
			title += " [" + m_EditorScenePath.filename().string() + "]";
		Application::Get().GetWindow().SetTitle(title);
	}

	// -------------------------------------------------------------------------
	// Scene play / stop
	// -------------------------------------------------------------------------

	void EditorLayer::OnScenePlay()
	{
		if (!m_EditorScene) return;

		m_SceneState = SceneState::Play;
		RebuildSceneList();

		// Determine which index the current scene maps to
		int currentIndex = 0;
		for (int i = 0; i < (int)m_SceneList.size(); i++)
		{
			std::error_code ec;
			if (!m_EditorScenePath.empty() &&
				std::filesystem::equivalent(m_SceneList[i], m_EditorScenePath, ec))
			{
				currentIndex = i;
				break;
			}
		}
		LuaScriptEngine::SetCurrentSceneIndex(currentIndex);

		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_ActiveScene->SetGravity(m_ProjectGravity);
		m_ActiveScene->OnViewportResize(
			(uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_ActiveScene->OnRuntimeStart();
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_ContentBrowserPanel.SetContext(m_ActiveScene.get());
	}

	void EditorLayer::OnSceneStop()
	{
		m_SceneState = SceneState::Edit;
		m_ActiveScene->OnRuntimeStop();
		m_ActiveScene = m_EditorScene;
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_ContentBrowserPanel.SetContext(m_ActiveScene.get());
		m_AnimationEditorPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OnScenePause()
	{
		if (m_SceneState == SceneState::Edit) return;
		m_ActiveScene->SetPaused(true);
	}

	void EditorLayer::OnDuplicateEntity()
	{
		if (m_SceneState != SceneState::Edit) return;
		if (Entity sel = m_SceneHierarchyPanel.GetSelectedEntity())
			m_EditorScene->DuplicateEntity(sel);
	}

}